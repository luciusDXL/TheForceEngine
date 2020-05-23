#include "frontEndUi.h"
#include "console.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Settings/settings.h>
#include <TFE_Ui/ui.h>
#include <TFE_Ui/markdown.h>

#include <TFE_Ui/imGUI/imgui_file_browser.h>
#include <TFE_Ui/imGUI/imgui.h>

namespace TFE_FrontEndUI
{
	enum SubUI
	{
		FEUI_NONE = 0,
		FEUI_MANUAL,
		FEUI_CONFIG,
		FEUI_COUNT
	};

	enum ConfigTab
	{
		CONFIG_GAME = 0,
		CONFIG_GRAPHICS,
		CONFIG_SOUND,
		CONFIG_COUNT
	};

	const char* c_configLabels[]=
	{
		"Game Settings",
		"Graphics",
		"Sound",
	};
	
	static AppState s_appState;
	static ImFont* s_menuFont;
	static ImFont* s_titleFont;
	static ImFont* s_dialogFont;
	static SubUI s_subUI;
	static ConfigTab s_configTab;

	static bool s_consoleActive = false;
	static imgui_addons::ImGuiFileBrowser s_fileDialog;

	void configGame();
	void configGraphics();
	void configSound();
	
	void init()
	{
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_FrontEndUI::init");
		s_appState = APP_STATE_MENU;
		s_subUI = FEUI_NONE;

		ImGuiIO& io = ImGui::GetIO();
		s_menuFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 32);
		s_titleFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 48);
		s_dialogFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 20);

		TFE_Console::init();
	}

	void shutdown()
	{
		TFE_Console::destroy();
	}

	AppState update()
	{
		return s_appState;
	}

	void setAppState(AppState state)
	{
		s_appState = state;
	}

	void toggleConsole()
	{
		s_consoleActive = !s_consoleActive;
		// Start open
		if (s_consoleActive)
		{
			TFE_Console::startOpen();
		}
		else
		{
			TFE_Console::startClose();
		}
	}

	bool isConsoleOpen()
	{
		return TFE_Console::isOpen();
	}

	void logToConsole(const char* str)
	{
		TFE_Console::addToHistory(str);
	}

	void showNoGameDataUI()
	{
		char settingsPath[TFE_MAX_PATH];
		TFE_Paths::appendPath(PATH_USER_DOCUMENTS, "settings.ini", settingsPath);
		const TFE_Game* game = TFE_Settings::getGame();
		
		ImGui::PushFont(s_dialogFont);
		
		bool active = true;
		ImGui::Begin("##NoGameData", &active, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
		ImGui::Text("No valid \"%s\" game data found. Please exit the application and edit the \"settings.ini\" file at", game->game);
		ImGui::Text("\"%s\"", settingsPath);
		ImGui::Separator();
		ImGui::Text("An example path, if the game was purchased through Steam is: \"D:/Program Files (x86)/Steam/steamapps/common/dark forces/Game/\" ");
		ImGui::Separator();
		if (ImGui::Button("Return to Menu"))
		{
			s_appState = APP_STATE_MENU;
		}
		ImGui::End();
		ImGui::PopFont();
	}

	void draw(bool drawFrontEnd, bool noGameData)
	{
		DisplayInfo display;
		TFE_RenderBackend::getDisplayInfo(&display);
		u32 w = display.width;
		u32 h = display.height;
		u32 menuWidth = 173;
		u32 menuHeight = 264;

		if (TFE_Console::isOpen())
		{
			TFE_Console::update();
		}
		if (noGameData)
		{
			drawFrontEnd = false;
			showNoGameDataUI();
		}
		if (!drawFrontEnd) { return; }

		if (s_subUI == FEUI_NONE)
		{
			// Title
			ImGui::PushFont(s_titleFont);
			ImGui::SetNextWindowPos(ImVec2(f32((w - 16*24 - 24) / 2), 100.0f));
			bool titleActive = true;
			ImGui::Begin("##Title", &titleActive, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
			ImGui::Text("The Force Engine");
			ImGui::End();
			ImGui::PopFont();

			// Main Menu
			ImGui::PushFont(s_menuFont);

			bool active = true;
			ImGui::SetNextWindowPos(ImVec2(f32((w - menuWidth) / 2), f32(h - menuHeight - 100)));
			ImGui::SetNextWindowSize(ImVec2((f32)menuWidth, (f32)menuHeight));
			ImGui::Begin("##MainMenu", &active, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

			if (ImGui::Button("Start    "))
			{
				s_appState = APP_STATE_DARK_FORCES;
			}
			if (ImGui::Button("Manual   "))
			{
				//s_subUI = FEUI_MANUAL;
			}
			if (ImGui::Button("Configure"))
			{
				s_subUI = FEUI_CONFIG;
				s_configTab = CONFIG_GAME;

				s_fileDialog.setCurrentPath(TFE_Paths::getPath(PATH_SOURCE_DATA));
			}
			if (ImGui::Button("Mods     "))
			{
			}
			if (ImGui::Button("Editor   "))
			{
				s_appState = APP_STATE_EDITOR;
			}
			if (ImGui::Button("Exit     "))
			{
				s_appState = APP_STATE_QUIT;
			}

			ImGui::End();
			ImGui::PopFont();
		}
		else if (s_subUI == FEUI_MANUAL)
		{
			bool active = true;
			ImGui::SetNextWindowPos(ImVec2(160.0f, 160.0f));
			ImGui::SetNextWindowSize(ImVec2(f32(w - 320), f32(h - 320)));
		}
		else if (s_subUI == FEUI_CONFIG)
		{
			// Game Source Paths
			// Dark Forces [.............................] [Browse]
			// Outlaws     [.............................] [Browse]
			// 
			// Selected Game [Dark Forces]V]
			//
			// Graphics
			// [] Fullscreen
			// [] Windowed
			// Resolution: [320x200]V]
			//             [640x400
			//             [640x480
			//             [...
			//             [Fit Window
			//             [Custom
			// Custom Resolution: [     ] x [     ]

			bool active = true;
			const u32 window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(160.0f, h));
			ImGui::Begin("##Sidebar", &active, window_flags);

			ImVec2 sideBarButtonSize(144, 24);
			ImGui::PushFont(s_dialogFont);
			if (ImGui::Button("Game", sideBarButtonSize))
			{
				s_configTab = CONFIG_GAME;
				TFE_Settings::writeToDisk();
			}
			if (ImGui::Button("Graphics", sideBarButtonSize))
			{
				s_configTab = CONFIG_GRAPHICS;
				TFE_Settings::writeToDisk();
			}
			if (ImGui::Button("Sound", sideBarButtonSize))
			{
				s_configTab = CONFIG_SOUND;
				TFE_Settings::writeToDisk();
			}
			ImGui::Separator();
			if (ImGui::Button("Return", sideBarButtonSize))
			{
				s_subUI = FEUI_NONE;
				TFE_Settings::writeToDisk();
			}
			ImGui::PopFont();

			ImGui::End();

			ImGui::SetNextWindowPos(ImVec2(160.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(w-160.0f, h));
			ImGui::SetNextWindowBgAlpha(0.25f);
			ImGui::Begin("##Settings", &active, window_flags);

			ImGui::PushFont(s_dialogFont);
			ImGui::LabelText("##ConfigLabel", "%s", c_configLabels[s_configTab]);
			ImGui::PopFont();
			ImGui::Separator();
			switch (s_configTab)
			{
			case CONFIG_GAME:
				configGame();
				break;
			case CONFIG_GRAPHICS:
				configGraphics();
				break;
			case CONFIG_SOUND:
				configSound();
				break;
			};

			ImGui::End();
		}
	}

	void configGame()
	{
		TFE_Settings_Game* darkForces = TFE_Settings::getGameSettings("Dark Forces");
		TFE_Settings_Game* outlaws = TFE_Settings::getGameSettings("Outlaws");

		//////////////////////////////////////////////////////
		// Source Game Data
		//////////////////////////////////////////////////////
		ImGui::PushFont(s_dialogFont);
		ImGui::LabelText("##ConfigLabel", "Game Source Data");
		ImGui::PopFont();

		ImGui::Text("Dark Forces:"); ImGui::SameLine(100);
		ImGui::InputText("##DarkForcesSource", darkForces->sourcePath, 1024); ImGui::SameLine();
		if (ImGui::Button("Browse##DarkForces"))
		{
			ImGui::OpenPopup("Select DARK.EXE or DARK.GOB");
		}

		ImGui::Text("Outlaws:"); ImGui::SameLine(100);
		ImGui::InputText("##OutlawsSource", outlaws->sourcePath, 1024); ImGui::SameLine();
		if (ImGui::Button("Browse##Outlaws"))
		{
			ImGui::OpenPopup("Select OUTLAWS.EXE or OUTLAWS.LAB");
		}
		ImGui::Separator();

		//////////////////////////////////////////////////////
		// Current Game
		//////////////////////////////////////////////////////
		const char* c_games[]=
		{
			"Dark Forces",
			// "Outlaws",  -- TODO
		};
		static s32 s_gameIndex = 0;
		ImGui::PushFont(s_dialogFont);
		ImGui::LabelText("##ConfigLabel", "Current Game");
		ImGui::PopFont();

		ImGui::SetNextItemWidth(256.0f);
		if (ImGui::Combo("##Current Game", &s_gameIndex, c_games, IM_ARRAYSIZE(c_games)))
		{
		}

		// File dialogs...
		char exePath[TFE_MAX_PATH];
		if (s_fileDialog.showOpenFileDialog("Select DARK.EXE or DARK.GOB", ImVec2(600, 300), ".EXE,.exe,.GOB,.gob"))
		{
			strcpy(exePath, s_fileDialog.selected_fn.c_str());
			FileUtil::getFilePath(exePath, darkForces->sourcePath);
		}
		else if (s_fileDialog.showOpenFileDialog("Select OUTLAWS.EXE or OUTLAWS.LAB", ImVec2(600, 300), ".EXE,.exe,.LAB,.lab"))
		{
			strcpy(exePath, s_fileDialog.selected_fn.c_str());
			FileUtil::getFilePath(exePath, outlaws->sourcePath);
		}
	}

	void configGraphics()
	{
	}

	void configSound()
	{
	}
}