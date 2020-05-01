#include "console.h"
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>
#include <TFE_Ui/markdown.h>
#include <TFE_System/parser.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>

namespace TFE_Console
{
	struct HistoryEntry
	{
		Vec4f color;
		std::string text;
	};

	static f32 c_maxConsoleHeight = 0.50f;

	static ImFont* s_consoleFont;
	static f32 s_height;
	static f32 s_anim;
	static s32 s_historyIndex;
	static s32 s_historyScroll;

	static std::vector<HistoryEntry> s_history;
	static char s_cmdLine[4096];

	static const Vec4f c_historyDefaultColor = { 0.5f, 0.5f, 0.75f, 1.0f };
	static const Vec4f c_historyLogColor     = { 0.5f, 0.5f, 0.5f, 1.0f };
	static const Vec4f c_historyErrorColor   = { 0.75f, 0.1f, 0.1f, 1.0f };

	static std::vector<CVar> s_var;

	// temp.
	static s32 s_varTest = -1;
	static f32 s_varTest2 = 0.274f;
	static bool s_varTestBool = false;
	static char s_varTestStr[32] = "Test String";
	
	void registerTestVar();

	bool init()
	{
		ImGuiIO& io = ImGui::GetIO();
		s_consoleFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 20);
		s_height = 0.0f;
		s_anim = 0.0f;
		s_historyIndex = -1;
		s_historyScroll = 0;

		registerTestVar();
		return true;
	}

	void destroy()
	{
	}

	s32 textEditCallback(ImGuiInputTextCallbackData* data)
	{
		switch (data->EventFlag)
		{
			case ImGuiInputTextFlags_CallbackCompletion:
			{
			} break;
			case ImGuiInputTextFlags_CallbackHistory:
			{
				s32 prevHistoryIndex = s_historyIndex;
				if (data->EventKey == ImGuiKey_UpArrow)
				{
					s_historyIndex = std::min(s_historyIndex + 1, (s32)s_history.size() - 1);
				}
				else if (data->EventKey == ImGuiKey_DownArrow)
				{
					s_historyIndex = std::max(s_historyIndex - 1, -1);
				}

				if (s_historyIndex >= 0)
				{
					const s32 index = std::max((s32)s_history.size() - (s32)s_historyIndex - 1, 0);
					if (index >= 0)
					{
						data->DeleteChars(0, data->BufTextLen);
						data->InsertChars(0, s_history[index].text.c_str());
					}
				}
				else if (prevHistoryIndex != s_historyIndex)
				{
					data->DeleteChars(0, data->BufTextLen);
				}
			} break;
		};
		return 0;
	}
			
	void registerCVarInt(const char* name, u32 flags, s32* var)
	{
		CVar newCVar;
		newCVar.name = name;
		newCVar.type = CVAR_INT;
		newCVar.flags = flags;
		newCVar.valueInt = var;
		s_var.push_back(newCVar);
	}

	void registerCVarFloat(const char* name, u32 flags, f32* var)
	{
		CVar newCVar;
		newCVar.name = name;
		newCVar.type = CVAR_FLOAT;
		newCVar.flags = flags;
		newCVar.valueFloat = var;
		s_var.push_back(newCVar);
	}

	void registerCVarBool(const char* name, u32 flags, bool* var)
	{
		CVar newCVar;
		newCVar.name = name;
		newCVar.type = CVAR_BOOL;
		newCVar.flags = flags;
		newCVar.valueBool = var;
		s_var.push_back(newCVar);
	}

	void registerCVarString(const char* name, u32 flags, char* var, u32 maxLen)
	{
		CVar newCVar;
		newCVar.name = name;
		newCVar.type = CVAR_STRING;
		newCVar.flags = flags;
		newCVar.stringValue = var;
		newCVar.maxLen = maxLen;
		s_var.push_back(newCVar);
	}

	void registerTestVar()
	{
		CVAR_INT(s_varTest, "r_testInt", 0);
		CVAR_FLOAT(s_varTest2, "r_testFloat", 0);
		CVAR_BOOL(s_varTestBool, "r_testBool", 0);
		CVAR_STRING(s_varTestStr, "r_testString", 0);
	}

	void setVariableValue(CVar* var, const char* value)
	{
		char* endPtr;
		char msg[4096];
		switch (var->type)
		{
			case CVAR_INT:
				*var->valueInt = strtol(value, &endPtr, 10);
				sprintf(msg, "set %s = %d", var->name.c_str(), *var->valueInt);
			break;
			case CVAR_FLOAT:
				*var->valueFloat = (f32)strtod(value, &endPtr);
				sprintf(msg, "set %s = %f", var->name.c_str(), *var->valueFloat);
			break;
			case CVAR_BOOL:
				if (strcasecmp(value, "true") == 0)
				{
					*var->valueBool = true;
				}
				else if (strcasecmp(value, "false") == 0)
				{
					*var->valueBool = false;
				}
				else
				{
					s32 intValue = strtol(value, &endPtr, 10);
					*var->valueBool = intValue != 0;
				}
				sprintf(msg, "set %s = %s", var->name.c_str(), *var->valueBool ? "true" : "false");
			break;
			case CVAR_STRING:
				const size_t sizeToCopy = std::min(strlen(value), (size_t)var->maxLen);
				strncpy(var->stringValue, value, sizeToCopy);
				var->stringValue[sizeToCopy] = 0;
				sprintf(msg, "set %s = \"%s\"", var->name.c_str(), var->stringValue);
			break;
		};

		s_history.push_back({ c_historyDefaultColor, msg });
	}

	void setVariable(const char* name, const char* value)
	{
		size_t count = s_var.size();
		for (size_t i = 0; i < count; i++)
		{
			if (s_var[i].name == name)
			{
				setVariableValue(&s_var[i], value);
				return;
			}
		}

		char errorMsg[4096];
		sprintf(errorMsg, "set - Unknown variable \"%s\"", name);
		s_history.push_back({ c_historyErrorColor, errorMsg });
	}

	void getVariable(const char* name, char* value)
	{
		CVar* var = nullptr;
		size_t count = s_var.size();
		for (size_t i = 0; i < count; i++)
		{
			if (s_var[i].name == name)
			{
				var = &s_var[i];
				break;
			}
		}
		if (!var)
		{
			char errorMsg[4096];
			sprintf(errorMsg, "get - Unknown variable \"%s\"", name);
			s_history.push_back({ c_historyErrorColor, errorMsg });
			return;
		}

		char msg[4096];
		switch (var->type)
		{
		case CVAR_INT:
			sprintf(msg, "%s = %d", var->name.c_str(), *var->valueInt);
			break;
		case CVAR_FLOAT:
			sprintf(msg, "%s = %f", var->name.c_str(), *var->valueFloat);
			break;
		case CVAR_BOOL:
			sprintf(msg, "%s = %s", var->name.c_str(), *var->valueBool ? "true" : "false");
			break;
		case CVAR_STRING:
			sprintf(msg, "%s = \"%s\"", var->name.c_str(), var->stringValue);
			break;
		};

		s_history.push_back({ c_historyDefaultColor, msg });
	}

	// TODO: Proper handling and registering of commands.
	void handleCommand(const char* cmd)
	{
		char errorMsg[4096];

		TFE_Parser parser;
		parser.init(cmd, strlen(cmd));

		TokenList tokens;
		parser.tokenizeLine(cmd, tokens);
		if (tokens.size() < 1)
		{
			char errorMsg[4096];
			sprintf(errorMsg, "Invalid Command - only whitespace found.");
			s_history.push_back({ c_historyErrorColor, errorMsg });

			return;
		}

		if (strcasecmp(tokens[0].c_str(), "clear") == 0)
		{
			s_history.clear();
			s_historyIndex = -1;
		}
		else if (strcasecmp(tokens[0].c_str(), "exit") == 0)
		{
			s_history.push_back({ c_historyDefaultColor, cmd });
			startClose();
		}
		else if (strcasecmp(tokens[0].c_str(), "echo") == 0)
		{
			if (tokens.size() > 1 && tokens[1].length())
			{
				s_history.push_back({ c_historyDefaultColor, tokens[1].c_str() });
			}
		}
		else if (strcasecmp(tokens[0].c_str(), "set") == 0)
		{
			if (tokens.size() >= 3)
			{
				setVariable(tokens[1].c_str(), tokens[2].c_str());
			}
			else
			{
				sprintf(errorMsg, "set - not enough arguments %d: set name value", tokens.size());
				s_history.push_back({ c_historyErrorColor, errorMsg });
			}
		}
		else if (strcasecmp(tokens[0].c_str(), "get") == 0)
		{
			if (tokens.size() >= 2)
			{
				char value[64];
				getVariable(tokens[1].c_str(), value);
			}
			else
			{
				sprintf(errorMsg, "get - not enough arguments %d: get name", tokens.size());
				s_history.push_back({ c_historyErrorColor, errorMsg });
			}
		}
		else
		{
			sprintf(errorMsg, "Invalid command \"%s\"", tokens[0].c_str());
			s_history.push_back({ c_historyErrorColor, errorMsg });
		}
	}

	void update()
	{
		DisplayInfo display;
		TFE_RenderBackend::getDisplayInfo(&display);
		const u32 w = display.width;
		const u32 h = display.height;
		const f32 dt = (f32)TFE_System::getDeltaTime();

		if (s_anim != 0.0f)
		{
			s_height += s_anim * 3.0f * dt;
			if (s_height <= 0.0f)
			{
				s_height = 0.0f;
				s_anim = 0.0f;
			}
			else if (s_height >= 1.0f)
			{
				s_height = 1.0f;
				s_anim = 0.0f;
			}
		}
		if (s_height <= 0.0f) { return; }

		const f32 consoleHeight = floorf(s_height * c_maxConsoleHeight * f32(h));
		ImGui::PushFont(s_consoleFont);
		ImGui::OpenPopup("console");
		ImGui::SetNextWindowSize(ImVec2(w, consoleHeight));
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetWindowFocus("##InputField");
		if (ImGui::BeginPopup("console", ImGuiWindowFlags_NoScrollbar))
		{
			if (TFE_Input::bufferedKeyDown(KEY_PAGEUP))
			{
				s_historyScroll++;
			}
			else if (TFE_Input::bufferedKeyDown(KEY_PAGEDOWN))
			{
				s_historyScroll--;
			}

			const s32 count = (s32)s_history.size();
			const s32 elementsPerPage = (consoleHeight - 36) / 20;
			s_historyScroll = std::max(0, std::min(s_historyScroll, count - elementsPerPage));
			s32 start = count - 1 - s_historyScroll;

			s32 y = consoleHeight - 56;
			for (s32 i = start; i >= 0 && y > -20; i--, y -= 20)
			{
				ImGui::SetCursorPosY(y);
				ImGui::TextColored(ImVec4(s_history[i].color.x, s_history[i].color.y, s_history[i].color.z, s_history[i].color.w), s_history[i].text.c_str());
			}

			ImGui::SetKeyboardFocusHere();
			ImGui::SetNextItemWidth(w - 16);
			ImGui::SetCursorPosY(consoleHeight - 32);
			u32 flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_NoHorizontalScroll |
						ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
			// Make sure the key to close the console doesn't make its was into the command line.
			if (s_anim < 0.0f)
			{
				flags |= ImGuiInputTextFlags_ReadOnly;
			}

			if (ImGui::InputText("##InputField", s_cmdLine, sizeof(s_cmdLine), flags, textEditCallback))
			{
				// for now just add to the text list.
				if (strlen(s_cmdLine))
				{
					handleCommand(s_cmdLine);
				}

				// for now just clear the command when we hit enter.
				s_cmdLine[0] = 0;
				s_historyIndex = -1;
				s_historyScroll = 0;
			}
			ImGui::EndPopup();
		}
		ImGui::PopFont();
	}

	bool isOpen()
	{
		return s_height > 0.0f || s_anim > 0.0f;
	}

	void startOpen()
	{
		s_anim = 1.0f;
		s_historyIndex = -1;
		s_historyScroll = 0;
	}

	void startClose()
	{
		s_anim = -1.0f;
	}

	void addToHistory(const char* str)
	{
		s_history.push_back({ c_historyLogColor, str });
	}
}
