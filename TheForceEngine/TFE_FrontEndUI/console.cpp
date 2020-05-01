#include "console.h"
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>
#include <TFE_Ui/markdown.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>

namespace Console
{
	static f32 c_maxConsoleHeight = 0.50f;

	static ImFont* s_consoleFont;
	static f32 s_height;
	static f32 s_anim;
	static s32 s_historyIndex;
	static s32 s_historyScroll;

	static std::vector<std::string> s_history;
	static char s_cmdLine[4096];

	bool init()
	{
		ImGuiIO& io = ImGui::GetIO();
		s_consoleFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 20);
		s_height = 0.0f;
		s_anim = 0.0f;
		s_historyIndex = -1;
		s_historyScroll = 0;
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
						data->InsertChars(0, s_history[index].c_str());
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

	void handleCommand(const char* cmd)
	{
		if (strcasecmp(cmd, "clear") == 0)
		{
			s_history.clear();
			s_historyIndex = -1;
		}
		else
		{
			s_history.push_back(cmd);
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
				ImGui::TextColored(ImVec4({ 1.0f, 1.0f, 1.0f, 1.0f }), s_history[i].c_str());
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
		s_history.push_back(str);
	}
}
