#include "editor.h"
#include "editorConfig.h"
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>

namespace TFE_Editor
{
	enum EditorMode
	{
		EDIT_CONFIG = 0,
		EDIT_ASSET,
		EDIT_LEVEL,
	};

	struct MessageBox
	{
		bool active = false;
		char id[512];
		char msg[TFE_MAX_PATH * 2] = "";
	};
	
	static bool s_showPerf = true;
	static bool s_showEditor = true;
	static EditorMode s_editorMode = EDIT_ASSET;
	static EditorMode s_prevEditorMode = EDIT_ASSET;
	static bool s_exitEditor = false;
	static bool s_configView = false;
	static WorkBuffer s_workBuffer;

	static MessageBox s_msgBox = {};

	static ImFont* s_fonts[FONT_COUNT * FONT_SIZE_COUNT] = { 0 };
	
	void menu();
	void loadFonts();

	WorkBuffer& getWorkBuffer()
	{
		return s_workBuffer;
	}

	void enable()
	{
		// Ui begin/render is called so we have a "UI Frame" in which to setup UI state.
		s_editorMode = EDIT_ASSET;
		s_exitEditor = false;
		loadFonts();
		loadConfig();
		AssetBrowser::init();
		s_msgBox = {};
	}

	void disable()
	{
		AssetBrowser::destroy();
	}

	void messageBoxUi()
	{
		pushFont(FONT_SMALL);
		if (ImGui::BeginPopupModal(s_msgBox.id, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGuiStyle& style = ImGui::GetStyle();
			f32 textWidth = ImGui::CalcTextSize(s_msgBox.msg).x + style.FramePadding.x;
			f32 buttonWidth = ImGui::CalcTextSize("OK").x;

			ImGui::Text(s_msgBox.msg);
			ImGui::Separator();

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (textWidth - buttonWidth) * 0.5f);
			if (ImGui::Button("OK"))
			{
				s_msgBox.active = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		popFont();
	}

	bool update(bool consoleOpen)
	{
		TFE_RenderBackend::clearWindow();

		if (s_msgBox.active)
		{
			ImGui::OpenPopup(s_msgBox.id);
		}

		menu();

		if (configSetupRequired() && !s_msgBox.active)
		{
			s_editorMode = EDIT_CONFIG;
		}

		if (s_editorMode == EDIT_CONFIG)
		{
			if (configUi())
			{
				s_editorMode = s_prevEditorMode;
			}
		}
		else if (s_editorMode == EDIT_ASSET)
		{
			AssetBrowser::update();
		}

		if (s_msgBox.active)
		{
			messageBoxUi();
		}

		if (TFE_Input::keyPressed(KEY_ESCAPE))
		{
			if (s_msgBox.active)
			{
				s_msgBox.active = false;
				ImGui::CloseCurrentPopup();
			}
			else
			{
				s_exitEditor = true;
			}
		}
		
		return s_exitEditor;
	}

	bool render()
	{
		if (s_editorMode == EDIT_ASSET)
		{
			AssetBrowser::render();
		}
		return true;
	}

	bool beginMenuBar()
	{
		bool menuActive = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("MainMenuBar", { 0.0f, 0.0f });
		ImGui::SetWindowSize("MainMenuBar", { (f32)displayInfo.width, 32.0f });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("MainMenuBar", &menuActive, window_flags);
		return ImGui::BeginMenuBar();
	}

	void endMenuBar()
	{
		ImGui::EndMenuBar();
		ImGui::End();
	}
		
	void menu()
	{
		pushFont(FONT_SMALL);

		beginMenuBar();
		{
			// General menu items.
			if (ImGui::BeginMenu("Editor"))
			{
				if (ImGui::MenuItem("Editor Config", NULL, s_editorMode == EDIT_CONFIG))
				{
					s_prevEditorMode = s_editorMode;
					if (s_prevEditorMode == EDIT_CONFIG)
					{
						s_prevEditorMode = EDIT_ASSET;
					}
					s_editorMode = EDIT_CONFIG;
				}
				if (ImGui::MenuItem("Asset Browser", NULL, s_editorMode == EDIT_ASSET))
				{
					if (s_editorMode == EDIT_CONFIG) { saveConfig(); }
					s_editorMode = EDIT_ASSET;
				}
				if (ImGui::MenuItem("Level Editor", NULL, s_editorMode == EDIT_LEVEL))
				{
					if (s_editorMode == EDIT_CONFIG) { saveConfig(); }
					s_editorMode = EDIT_LEVEL;
				}
				if (ImGui::MenuItem("Return to Game", NULL, (bool*)NULL))
				{
					if (s_editorMode == EDIT_CONFIG) { saveConfig(); }
					s_exitEditor = true;
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Select"))
			{
				if (ImGui::MenuItem("Select All", NULL, (bool*)NULL))
				{
					if (s_editorMode == EDIT_ASSET)
					{
						AssetBrowser::selectAll();
					}
				}
				if (ImGui::MenuItem("Select None", NULL, (bool*)NULL))
				{
					if (s_editorMode == EDIT_ASSET)
					{
						AssetBrowser::selectNone();
					}
				}
				if (ImGui::MenuItem("Invert Selection", NULL, (bool*)NULL))
				{
					if (s_editorMode == EDIT_ASSET)
					{
						AssetBrowser::invertSelection();
					}
				}
				ImGui::EndMenu();
			}
		}
		endMenuBar();

		popFont();
	}
		
	void loadFonts()
	{
		// Assume if any fonts are loaded, they all are.
		if (s_fonts[0]) { return; }

		char fontPath[TFE_MAX_PATH];
		sprintf(fontPath, "%s", "Fonts/DroidSansMono.ttf");
		TFE_Paths::mapSystemPath(fontPath);

		ImGuiIO& io = ImGui::GetIO();
		ImFont** fontSmall = &s_fonts[FONT_SMALL * FONT_SIZE_COUNT];
		fontSmall[0] = io.Fonts->AddFontFromFileTTF(fontPath, 16 * 100 / 100);
		fontSmall[1] = io.Fonts->AddFontFromFileTTF(fontPath, 16 * 125 / 100);
		fontSmall[2] = io.Fonts->AddFontFromFileTTF(fontPath, 16 * 150 / 100);
		fontSmall[3] = io.Fonts->AddFontFromFileTTF(fontPath, 16 * 175 / 100);
		fontSmall[4] = io.Fonts->AddFontFromFileTTF(fontPath, 16 * 200 / 100);
		TFE_Ui::invalidateFontAtlas();
	}
		
	void pushFont(FontType type)
	{
		const s32 index = fontScaleToIndex(s_editorConfig.fontScale);
		ImFont** fonts = &s_fonts[type * FONT_SIZE_COUNT];
		ImGui::PushFont(fonts[index]);
	}

	void popFont()
	{
		ImGui::PopFont();
	}
		
	void showMessageBox(const char* type, const char* msg, ...)
	{
		char fullStr[TFE_MAX_PATH * 2];
		va_list arg;
		va_start(arg, msg);
		vsprintf(fullStr, msg, arg);
		va_end(arg);

		s_msgBox.active = true;
		strcpy(s_msgBox.msg, fullStr);
		sprintf(s_msgBox.id, "%s##MessageBox", type);
	}
}
