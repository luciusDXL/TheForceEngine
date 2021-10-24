#include "frontEndUi.h"
#include "console.h"
#include "profilerView.h"
#include <TFE_DarkForces/config.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Archive/archive.h>
#include <TFE_Settings/settings.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Input/inputMapping.h>
#include <TFE_Ui/ui.h>
#include <TFE_Ui/markdown.h>
#include <TFE_Ui/imGUI/imgui.h>

using namespace TFE_Input;

namespace TFE_FrontEndUI
{
	struct UiImage
	{
		void* image;
		u32 width;
		u32 height;
	};

	enum SubUI
	{
		FEUI_NONE = 0,
		FEUI_MANUAL,
		FEUI_CREDITS,
		FEUI_CONFIG,
		FEUI_COUNT
	};

	enum ConfigTab
	{
		CONFIG_ABOUT = 0,
		CONFIG_GAME,
		CONFIG_INPUT,
		CONFIG_GRAPHICS,
		CONFIG_HUD,
		CONFIG_SOUND,
		CONFIG_COUNT
	};

	const char* c_configLabels[] =
	{
		"About",
		"Game Settings",
		"Input",
		"Graphics",
		"Hud",
		"Sound",
	};

	static const Vec2i c_resolutionDim[] =
	{
		{320,200},
		{640,400},
		{640,480},
		{800,600},
		{960,720},
		{1024,768},
		{1280,960},
		{1440,1050},
		{1440,1080},
		{1600,1200},
		{1856,1392},
		{1920,1440},
		{2048,1536},
		{2880,2160}
	};

	static const char* c_resolutions[] =
	{
		"200p  (320x200)",
		"400p  (640x400)",
		"480p  (640x480)",
		"600p  (800x600)",
		"720p  (960x720)",
		"768p  (1024x768)",
		"960p  (1280x960)",
		"1050p (1440x1050)",
		"1080p (1440x1080)",
		"1200p (1600x1200)",
		"1392p (1856x1392)",
		"1440p (1920x1440)",
		"1536p (2048x1536)",
		"2160p (2880x2160)",
		"Match Window",
		"Custom",
	};

	static const char* c_resolutionsWide[] =
	{
		"200p",
		"400p",
		"480p",
		"600p",
		"720p",
		"768p",
		"960p",
		"1050p",
		"1080p",
		"1200p",
		"1392p",
		"1440p",
		"1536p",
		"2160p",
		"Match Window",
		"Custom",
	};

	static const char* c_renderer[] =
	{
		"Classic (Software)",
		// TODO
		//"Classic (Hardware)",
	};

	typedef void(*MenuItemSelected)();

	static s32 s_resIndex = 0;
	static s32 s_rendererIndex = 0;
	static char* s_aboutDisplayStr = nullptr;
	static char* s_manualDisplayStr = nullptr;
	static char* s_creditsDisplayStr = nullptr;

	static AppState s_appState;
	static AppState s_menuRetState;
	static ImFont* s_menuFont;
	static ImFont* s_versionFont;
	static ImFont* s_titleFont;
	static ImFont* s_dialogFont;
	static SubUI s_subUI;
	static ConfigTab s_configTab;

	static bool s_consoleActive = false;
	static bool s_relativeMode;
	static bool s_restartGame;

	static UiImage s_logoGpuImage;
	static UiImage s_titleGpuImage;

	static UiImage s_buttonNormal[7];
	static UiImage s_buttonSelected[7];

	static MenuItemSelected s_menuItemselected[7];

	void configAbout();
	void configGame();
	void configInput();
	void configGraphics();
	void configHud();
	void configSound();
	void pickCurrentResolution();
	void manual();
	void credits();

	void menuItem_Start();
	void menuItem_Manual();
	void menuItem_Credits();
	void menuItem_Settings();
	void menuItem_Mods();
	void menuItem_Editor();
	void menuItem_Exit();

	bool loadGpuImage(const char* localPath, UiImage* uiImage)
	{
		char imagePath[TFE_MAX_PATH];
		TFE_Paths::appendPath(TFE_PathType::PATH_PROGRAM, localPath, imagePath, TFE_MAX_PATH);
		Image* image = TFE_Image::get(imagePath);
		if (image)
		{
			TextureGpu* gpuImage = TFE_RenderBackend::createTexture(image->width, image->height, image->data, MAG_FILTER_LINEAR);
			uiImage->image = TFE_RenderBackend::getGpuPtr(gpuImage);
			uiImage->width = image->width;
			uiImage->height = image->height;
			return true;
		}
		return false;
	}

	void initConsole()
	{
		TFE_Console::init();
		TFE_ProfilerView::init();
	}

	void init()
	{
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_FrontEndUI::init");
		s_appState = APP_STATE_MENU;
		s_menuRetState = APP_STATE_MENU;
		s_subUI = FEUI_NONE;
		s_relativeMode = false;
		s_restartGame = true;

		ImGuiIO& io = ImGui::GetIO();
		s_menuFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 32);
		s_versionFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 16);
		s_titleFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 48);
		s_dialogFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 20);

		if (!loadGpuImage("UI_Images/TFE_TitleLogo.png", &s_logoGpuImage))
		{
			TFE_System::logWrite(LOG_ERROR, "SystemUI", "Cannot load TFE logo: \"UI_Images/TFE_TitleLogo.png\"");
		}
		if (!loadGpuImage("UI_Images/TFE_TitleText.png", &s_titleGpuImage))
		{
			TFE_System::logWrite(LOG_ERROR, "SystemUI", "Cannot load TFE Title: \"UI_Images/TFE_TitleText.png\"");
		}

		bool buttonsLoaded = true;
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_StartNormal.png", &s_buttonNormal[0]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_StartSelected.png", &s_buttonSelected[0]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_ManualNormal.png", &s_buttonNormal[1]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_ManualSelected.png", &s_buttonSelected[1]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_CreditsNormal.png", &s_buttonNormal[2]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_CreditsSelected.png", &s_buttonSelected[2]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_SettingsNormal.png", &s_buttonNormal[3]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_SettingsSelected.png", &s_buttonSelected[3]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_ModsNormal.png", &s_buttonNormal[4]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_ModsSelected.png", &s_buttonSelected[4]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_EditorNormal.png", &s_buttonNormal[5]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_EditorSelected.png", &s_buttonSelected[5]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_ExitNormal.png", &s_buttonNormal[6]);
		buttonsLoaded &= loadGpuImage("UI_Images/TFE_ExitSelected.png", &s_buttonSelected[6]);
		if (!buttonsLoaded)
		{
			TFE_System::logWrite(LOG_ERROR, "SystemUI", "Cannot load title screen button images.");
		}
				
		// Setup menu item callbacks
		s_menuItemselected[0] = menuItem_Start;
		s_menuItemselected[1] = menuItem_Manual;
		s_menuItemselected[2] = menuItem_Credits;
		s_menuItemselected[3] = menuItem_Settings;
		s_menuItemselected[4] = menuItem_Mods;
		s_menuItemselected[5] = menuItem_Editor;
		s_menuItemselected[6] = menuItem_Exit;
	}

	void shutdown()
	{
		delete[] s_aboutDisplayStr;
		delete[] s_manualDisplayStr;
		delete[] s_creditsDisplayStr;
		TFE_Console::destroy();
		TFE_ProfilerView::destroy();
	}

	AppState update()
	{
		return s_appState;
	}

	void setAppState(AppState state)
	{
		s_appState = state;
	}

	void enableConfigMenu()
	{
		s_appState = APP_STATE_MENU;
		s_subUI = FEUI_CONFIG;
		s_relativeMode = TFE_Input::relativeModeEnabled();
		TFE_Input::enableRelativeMode(false);
		pickCurrentResolution();
	}

	bool isConfigMenuOpen()
	{
		return (s_appState == APP_STATE_MENU) && (s_subUI == FEUI_CONFIG);
	}

	void setMenuReturnState(AppState state)
	{
		s_menuRetState = state;
	}

	AppState menuReturn()
	{
		s_restartGame = s_menuRetState == APP_STATE_MENU;

		s_subUI = FEUI_NONE;
		s_appState = s_menuRetState;
		TFE_Settings::writeToDisk();
		TFE_Input::enableRelativeMode(s_relativeMode);
		inputMapping_serialize();

		return s_appState;
	}

	bool restartGame()
	{
		return s_restartGame;
	}

	bool shouldClearScreen()
	{
		return s_appState == APP_STATE_MENU && s_subUI == FEUI_NONE;
	}

	bool toggleConsole()
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
		return s_consoleActive;
	}

	bool isConsoleOpen()
	{
		return TFE_Console::isOpen();
	}

	void logToConsole(const char* str)
	{
		TFE_Console::addToHistory(str);
	}

	void toggleProfilerView()
	{
		bool isEnabled = TFE_ProfilerView::isEnabled();
		TFE_ProfilerView::enable(!isEnabled);
	}

	void showNoGameDataUI()
	{
		char settingsPath[TFE_MAX_PATH];
		TFE_Paths::appendPath(PATH_USER_DOCUMENTS, "settings.ini", settingsPath);
		const TFE_Game* game = TFE_Settings::getGame();

		ImGui::PushFont(s_dialogFont);

		bool active = true;
		ImGui::Begin("##NoGameData", &active, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
		ImGui::Text("No valid \"%s\" game data found.", game->game);
		ImGui::Text("Please select Configure from the menu,");
		ImGui::Text("open up Game settings and setup the game path.");
		ImGui::Separator();
		ImGui::Text("An example path, if the game was purchased through Steam is:");
		ImGui::Text("\"D:/Program Files (x86)/Steam/steamapps/common/dark forces/Game/\" ");
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
		u32 menuWidth = 156;
		u32 menuHeight = 306;

		if (TFE_Console::isOpen())
		{
			TFE_Console::update();
		}
		if (TFE_ProfilerView::isEnabled())
		{
			TFE_ProfilerView::update();
		}
		if (noGameData)
		{
			s_subUI = FEUI_CONFIG;
			s_configTab = CONFIG_GAME;
			s_appState = APP_STATE_MENU;
			pickCurrentResolution();
		}
		if (!drawFrontEnd) { return; }

		if (s_subUI == FEUI_NONE)
		{
			s_menuRetState = APP_STATE_MENU;
			s_relativeMode = false;

			const f32 windowPadding = 16.0f;	// required padding so that a window completely holds a control without clipping.
			const u32 windowInvisFlags = ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;

			const s32 logoScale = 2160;	// logo and title are authored for 2160p so it looks good at high resolutions, such as 1440p and 4k.
			const s32 posScale = 1080;	// positions are at 1080p so must be scaled for different resolutions.
			const s32 titlePosY = 100;

			const s32 logoHeight = s_logoGpuImage.height  * h / logoScale;
			const s32 logoWidth = s_logoGpuImage.width   * h / logoScale;
			const s32 titleHeight = s_titleGpuImage.height * h / logoScale;
			const s32 titleWidth = s_titleGpuImage.width  * h / logoScale;
			const s32 textHeight = s_buttonNormal[0].height * h / logoScale;
			const s32 textWidth = s_buttonNormal[0].width * h / logoScale;
			const s32 topOffset = titlePosY * h / posScale;

			// Title
			bool titleActive = true;
			ImGui::SetNextWindowSize(ImVec2(logoWidth + windowPadding, logoHeight + windowPadding));
			ImGui::SetNextWindowPos(ImVec2(f32((w - logoWidth - titleWidth) / 2), (f32)topOffset));
			ImGui::Begin("##Logo", &titleActive, windowInvisFlags);
			ImGui::Image(s_logoGpuImage.image, ImVec2((f32)logoWidth, (f32)logoHeight));
			ImGui::End();

			ImGui::SetNextWindowSize(ImVec2(titleWidth + windowPadding, titleWidth + windowPadding));
			ImGui::SetNextWindowPos(ImVec2(f32((w + logoWidth - titleWidth) / 2), (f32)topOffset + (logoHeight - titleHeight) / 2));
			ImGui::Begin("##Title", &titleActive, windowInvisFlags);
			ImGui::Image(s_titleGpuImage.image, ImVec2((f32)titleWidth, (f32)titleHeight));
			ImGui::End();

			ImGui::PushFont(s_versionFont);
			char versionText[256];
			sprintf(versionText, "Version %s", TFE_System::getVersionString());
			const f32 stringWidth = s_versionFont->CalcTextSizeA(16.0f, 1024.0f, 0.0f, versionText).x;

			ImGui::SetNextWindowPos(ImVec2(f32(w) - stringWidth - 32.0f, f32(h) - 64.0f));
			ImGui::Begin("##Version", &titleActive, windowInvisFlags);
			ImGui::Text(versionText);
			ImGui::End();
			ImGui::PopFont();

			s32 menuHeight = (textHeight + 24) * 7 + 4;
			// Warning Text - Note this is temporary and will be removed once things have settled.
			{
				f32 warningWidth;
				s32 yOffset;
				if (h > 700)
				{
					yOffset = 96;
					ImGui::PushFont(s_menuFont);
					warningWidth = s_menuFont->CalcTextSizeA(32.0f, 1024.0f, 0.0f, "Expect Bugs and Missing Features").x;
				}
				else
				{
					yOffset = 32;
					warningWidth = ImGui::GetFont()->CalcTextSizeA(16.0f, 1024.0f, 0.0f, "Expect Bugs and Missing Features").x;
				}

				ImGui::SetNextWindowPos(ImVec2(f32((w - warningWidth) / 2), f32(h - menuHeight - topOffset - yOffset)));
				ImGui::Begin("##Warning", &titleActive, windowInvisFlags);
				ImGui::Text("    Pre-Release Test Build -");
				ImGui::Text("Expect Bugs and Missing Features");
				ImGui::End();

				if (h > 700)
				{
					ImGui::PopFont();
				}
			}

			// Main Menu
			ImGui::PushFont(s_menuFont);

			bool active = true;
			ImGui::SetNextWindowPos(ImVec2(f32((w - textWidth - 24) / 2), f32(h - menuHeight - topOffset)));
			ImGui::SetNextWindowSize(ImVec2((f32)textWidth + 24, f32(menuHeight)));
			ImGui::Begin("##MainMenu", &active, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);

			ImVec2 textSize = ImVec2(f32(textWidth), f32(textHeight));
			for (s32 i = 0; i < 7; i++)
			{
				if (ImGui::ImageAnimButton(s_buttonNormal[i].image, s_buttonSelected[i].image, textSize))
				{
					s_menuItemselected[i]();
				}
			}

			ImGui::End();
			ImGui::PopFont();
		}
		else if (s_subUI == FEUI_MANUAL)
		{
			bool active = true;
			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(f32(w), f32(h)));

			const u32 window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

			ImGui::Begin("Manual", &active, window_flags);
			if (ImGui::Button("Return") || !active)
			{
				s_subUI = FEUI_NONE;
			}
			manual();
			ImGui::End();
		}
		else if (s_subUI == FEUI_CREDITS)
		{
			bool active = true;
			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(f32(w), f32(h)));

			const u32 window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

			ImGui::Begin("Credits", &active, window_flags);
			if (ImGui::Button("Return") || !active)
			{
				s_subUI = FEUI_NONE;
			}
			credits();
			ImGui::End();
		}
		else if (s_subUI == FEUI_CONFIG)
		{
			bool active = true;
			const u32 window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(160.0f, f32(h)));
			ImGui::Begin("##Sidebar", &active, window_flags);

			ImVec2 sideBarButtonSize(144, 24);
			ImGui::PushFont(s_dialogFont);
			if (ImGui::Button("About", sideBarButtonSize))
			{
				s_configTab = CONFIG_ABOUT;
				TFE_Settings::writeToDisk();
				inputMapping_serialize();
			}
			if (ImGui::Button("Game", sideBarButtonSize))
			{
				s_configTab = CONFIG_GAME;
				TFE_Settings::writeToDisk();
				inputMapping_serialize();
			}
			if (ImGui::Button("Input", sideBarButtonSize))
			{
				s_configTab = CONFIG_INPUT;
				TFE_Settings::writeToDisk();
				inputMapping_serialize();
			}
			if (ImGui::Button("Graphics", sideBarButtonSize))
			{
				s_configTab = CONFIG_GRAPHICS;
				TFE_Settings::writeToDisk();
				inputMapping_serialize();
			}
			if (ImGui::Button("Hud", sideBarButtonSize))
			{
				s_configTab = CONFIG_HUD;
				TFE_Settings::writeToDisk();
				inputMapping_serialize();
			}
			if (ImGui::Button("Sound", sideBarButtonSize))
			{
				s_configTab = CONFIG_SOUND;
				TFE_Settings::writeToDisk();
				inputMapping_serialize();
			}
			ImGui::Separator();
			if (ImGui::Button("Return", sideBarButtonSize))
			{
				s_restartGame = (s_menuRetState == APP_STATE_MENU);

				s_subUI = FEUI_NONE;
				s_appState = s_menuRetState;
				TFE_Settings::writeToDisk();
				TFE_Input::enableRelativeMode(s_relativeMode);
				inputMapping_serialize();
			}
			if (s_menuRetState != APP_STATE_MENU && ImGui::Button("Exit to Menu", sideBarButtonSize))
			{
				s_restartGame = true;
				s_menuRetState = APP_STATE_MENU;

				s_subUI = FEUI_NONE;
				s_appState = APP_STATE_MENU;
				TFE_Settings::writeToDisk();
				inputMapping_serialize();

				// End the current game.
				// TODO
				// TFE_GameLoop::endLevel();
			}
			ImGui::PopFont();

			ImGui::End();

			// adjust the width based on tab.
			s32 tabWidth = w - 160;
			if (s_configTab >= CONFIG_INPUT)
			{
				tabWidth = 414;
			}

			ImGui::SetNextWindowPos(ImVec2(160.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(f32(tabWidth), f32(h)));
			ImGui::SetNextWindowBgAlpha(0.95f);
			ImGui::Begin("##Settings", &active, window_flags);

			ImGui::PushFont(s_dialogFont);
			ImGui::LabelText("##ConfigLabel", "%s", c_configLabels[s_configTab]);
			ImGui::PopFont();
			ImGui::Separator();
			switch (s_configTab)
			{
			case CONFIG_ABOUT:
				configAbout();
				break;
			case CONFIG_GAME:
				configGame();
				break;
			case CONFIG_INPUT:
				configInput();
				break;
			case CONFIG_GRAPHICS:
				configGraphics();
				break;
			case CONFIG_HUD:
				configHud();
				break;
			case CONFIG_SOUND:
				configSound();
				break;
			};

			ImGui::End();
		}
	}

	void manual()
	{
		if (!s_manualDisplayStr)
		{
			// The document has not been loaded yet.
			char path[TFE_MAX_PATH];
			char fileName[TFE_MAX_PATH];
			strcpy(fileName, "Documentation/markdown/TheForceEngineManual.md");
			TFE_Paths::appendPath(PATH_PROGRAM, fileName, path);

			FileStream file;
			if (file.open(path, FileStream::MODE_READ))
			{
				const size_t len = file.getSize();
				s_manualDisplayStr = new char[len + 1];
				file.readBuffer(s_manualDisplayStr, (u32)len);
				s_manualDisplayStr[len] = 0;

				file.close();
			}
		}

		if (s_manualDisplayStr) { TFE_Markdown::draw(s_manualDisplayStr); }
	}

	void credits()
	{
		if (!s_creditsDisplayStr)
		{
			// The document has not been loaded yet.
			char path[TFE_MAX_PATH];
			char fileName[TFE_MAX_PATH];
			strcpy(fileName, "Documentation/markdown/credits.md");
			TFE_Paths::appendPath(PATH_PROGRAM, fileName, path);

			FileStream file;
			if (file.open(path, FileStream::MODE_READ))
			{
				const size_t len = file.getSize();
				s_creditsDisplayStr = new char[len + 1];
				file.readBuffer(s_creditsDisplayStr, (u32)len);
				s_creditsDisplayStr[len] = 0;

				file.close();
			}
		}

		if (s_creditsDisplayStr) { TFE_Markdown::draw(s_creditsDisplayStr); }
	}

	void configAbout()
	{
		if (!s_aboutDisplayStr)
		{
			// The document has not been loaded yet.
			char path[TFE_MAX_PATH];
			char fileName[TFE_MAX_PATH];
			strcpy(fileName, "Documentation/markdown/theforceengine.md");
			TFE_Paths::appendPath(PATH_PROGRAM, fileName, path);

			FileStream file;
			if (file.open(path, FileStream::MODE_READ))
			{
				const size_t len = file.getSize();
				s_aboutDisplayStr = new char[len + 1];
				file.readBuffer(s_aboutDisplayStr, (u32)len);
				s_aboutDisplayStr[len] = 0;

				file.close();
			}
		}

		if (s_aboutDisplayStr) { TFE_Markdown::draw(s_aboutDisplayStr); }
	}

	void configGame()
	{
		TFE_Settings_Game* darkForces = TFE_Settings::getGameSettings("Dark Forces");
		TFE_Settings_Game* outlaws = TFE_Settings::getGameSettings("Outlaws");

		s32 browseWinOpen = -1;
		
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
			browseWinOpen = 0;
		}

		ImGui::Text("Outlaws:"); ImGui::SameLine(100);
		ImGui::InputText("##OutlawsSource", outlaws->sourcePath, 1024); ImGui::SameLine();
		if (ImGui::Button("Browse##Outlaws"))
		{
			browseWinOpen = 1;
		}
		ImGui::Separator();

		//////////////////////////////////////////////////////
		// Current Game
		//////////////////////////////////////////////////////
		const char* c_games[] =
		{
			"Dark Forces",
			// "Outlaws",  -- TODO
		};
		static s32 s_gameIndex = 0;
		ImGui::PushFont(s_dialogFont);
		ImGui::LabelText("##ConfigLabel", "Current Game");
		ImGui::PopFont();

		ImGui::SetNextItemWidth(256.0f);
		ImGui::Combo("##Current Game", &s_gameIndex, c_games, IM_ARRAYSIZE(c_games));

		// File dialogs...
		if (browseWinOpen >= 0)
		{
			char exePath[TFE_MAX_PATH];
			char filePath[TFE_MAX_PATH];
			const char* games[]=
			{
				"Select DARK.EXE or DARK.GOB",
				"Select OUTLAWS.EXE or OUTLAWS.LAB"
			};
			const std::vector<std::string> filters[]=
			{
				{ "Executable", "*.exe", "GOB Archive", "*.gob" },
				{ "Executable", "*.exe", "LAB Archive", "*.lab" },
			};

			FileResult res = TFE_Ui::openFileDialog(games[browseWinOpen], DEFAULT_PATH, filters[browseWinOpen]);
			if (!res.empty() && !res[0].empty())
			{
				strcpy(exePath, res[0].c_str());
				FileUtil::getFilePath(exePath, filePath);
				FileUtil::fixupPath(filePath);

				if (browseWinOpen == 0)
				{
					// Before accepting this path, verify that some of the required files are here...
					char testFile[TFE_MAX_PATH];
					sprintf(testFile, "%sDARK.GOB", filePath);
					if (FileUtil::exists(testFile))
					{
						strcpy(darkForces->sourcePath, filePath);
						TFE_Paths::setPath(PATH_SOURCE_DATA, darkForces->sourcePath);
					}
					else
					{
						ImGui::OpenPopup("Invalid Source Data");
					}
				}
				else if (browseWinOpen == 0)
				{
					// Before accepting this path, verify that some of the required files are here...
					char testFile[TFE_MAX_PATH];
					sprintf(testFile, "%sOutlaws.lab", filePath);
					if (FileUtil::exists(testFile))
					{
						strcpy(outlaws->sourcePath, filePath);
						// TFE_Paths::setPath(PATH_SOURCE_DATA, outlaws->sourcePath);
					}
					else
					{
						ImGui::OpenPopup("Invalid Source Data");
					}
				}
			}
		}

		if (ImGui::BeginPopupModal("Invalid Source Data", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Invalid source data path!");
			ImGui::Text("Please select the source game executable or source asset (GOB or LAB).");

			if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
	}

	static const char* c_axisBinding[]=
	{
		"X Left Axis",
		"Y Left Axis",
		"X Right Axis",
		"Y Right Axis",
	};

	static const char* c_mouseMode[] =
	{
		"Menus Only",
		"Horizontal Turning",
		"Mouselook"
	};

	static InputConfig* s_inputConfig = nullptr;
	static bool s_controllerWinOpen = true;
	static bool s_mouseWinOpen = true;
	static bool s_inputMappingOpen = true;

	void getBindingString(InputBinding* binding, char* inputName)
	{
		if (binding->type == ITYPE_KEYBOARD)
		{
			if (binding->keyMod)
			{
				sprintf_s(inputName, 64, "%s + %s", TFE_Input::getKeyboardModifierName(binding->keyMod), TFE_Input::getKeyboardName(binding->keyCode));
			}
			else
			{
				strcpy_s(inputName, 64, TFE_Input::getKeyboardName(binding->keyCode));
			}
		}
		else if (binding->type == ITYPE_MOUSE)
		{
			strcpy_s(inputName, 64, TFE_Input::getMouseButtonName(binding->mouseBtn));
		}
		else if (binding->type == ITYPE_CONTROLLER)
		{
			strcpy_s(inputName, 64, TFE_Input::getControllButtonName(binding->ctrlBtn));
		}
		else if (binding->type == ITYPE_CONTROLLER_AXIS)
		{
			strcpy_s(inputName, 64, TFE_Input::getControllerAxisName(binding->axis));
		}
	}

	static s32 s_keyIndex = -1;
	static s32 s_keySlot = -1;
	static KeyModifier s_keyMod = KEYMOD_NONE;

	void inputMapping(const char* name, InputAction action)
	{
		u32 indices[2];
		u32 count = inputMapping_getBindingsForAction(action, indices, 2);

		char inputName1[256] = "##Input1";
		char inputName2[256] = "##Input2";

		ImGui::LabelText("##ConfigLabel", name); ImGui::SameLine(132);
		if (count >= 1)
		{
			InputBinding* binding = inputMapping_getBindindByIndex(indices[0]);
			getBindingString(binding, inputName1);
			strcat(inputName1, "##Input1");
			strcat(inputName1, name);
		}
		else
		{
			strcat(inputName1, name);
		}

		if (count >= 2)
		{
			InputBinding* binding = inputMapping_getBindindByIndex(indices[1]);
			getBindingString(binding, inputName2);
			strcat(inputName2, "##Input2");
			strcat(inputName2, name);
		}
		else
		{
			strcat(inputName2, name);
		}

		if (ImGui::Button(inputName1, ImVec2(120.0f, 0.0f)))
		{
			// Press Key Popup.
			s_keyIndex = s32(action);
			s_keySlot = 0;
			s_keyMod = TFE_Input::getKeyModifierDown();
			ImGui::OpenPopup("##ChangeBinding");
		}
		ImGui::SameLine();
		if (ImGui::Button(inputName2, ImVec2(120.0f, 0.0f)))
		{
			// Press Key Popup.
			s_keyIndex = s32(action);
			s_keySlot = 1;
			s_keyMod = TFE_Input::getKeyModifierDown();
			ImGui::OpenPopup("##ChangeBinding");
		}
	}

	bool uiControlsEnabled()
	{
		return (!ImGui::IsPopupOpen("##ChangeBinding") && s_configTab != CONFIG_INPUT) || s_subUI == FEUI_NONE;
	}

	void configInput()
	{
		const u32 window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
		f32 scroll = ImGui::GetScrollY();
		f32 yNext = 45.0f;

		s_inputConfig = inputMapping_get();

		ImGui::SetNextWindowPos(ImVec2(165.0f, yNext - scroll));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
		if (ImGui::BeginChild("Controller Options", ImVec2(390.0f, s_controllerWinOpen ? 400.0f : 29.0f), true, window_flags))
		{
			if (ImGui::Button("Controller Options", ImVec2(370.0f, 0.0f)))
			{
				s_controllerWinOpen = !s_controllerWinOpen;
			}
			ImGui::PopStyleVar();

			if (s_controllerWinOpen)
			{
				ImGui::Spacing();
				bool controllerEnable = (s_inputConfig->controllerFlags & CFLAG_ENABLE) != 0;
				if (ImGui::Checkbox("Enable", &controllerEnable))
				{
					if (controllerEnable) { s_inputConfig->controllerFlags |=  CFLAG_ENABLE; }
					                 else { s_inputConfig->controllerFlags &= ~CFLAG_ENABLE; }
				}

				ImGui::PushFont(s_dialogFont);
				ImGui::LabelText("##ConfigLabel", "Controller Axis");
				ImGui::PopFont();

				ImGui::LabelText("##ConfigLabel", "Horizontal Turn"); ImGui::SameLine(175);
				ImGui::SetNextItemWidth(196);
				ImGui::Combo("##CtrlLookHorz", (s32*)&s_inputConfig->axis[AA_LOOK_HORZ], c_axisBinding, IM_ARRAYSIZE(c_axisBinding));

				ImGui::LabelText("##ConfigLabel", "Vertical Look"); ImGui::SameLine(175);
				ImGui::SetNextItemWidth(196);
				ImGui::Combo("##CtrlLookVert", (s32*)&s_inputConfig->axis[AA_LOOK_VERT], c_axisBinding, IM_ARRAYSIZE(c_axisBinding));

				ImGui::LabelText("##ConfigLabel", "Move"); ImGui::SameLine(175);
				ImGui::SetNextItemWidth(196);
				ImGui::Combo("##CtrlStrafe", (s32*)&s_inputConfig->axis[AA_STRAFE], c_axisBinding, IM_ARRAYSIZE(c_axisBinding));

				ImGui::LabelText("##ConfigLabel", "Strafe"); ImGui::SameLine(175);
				ImGui::SetNextItemWidth(196);
				ImGui::Combo("##CtrlMove", (s32*)&s_inputConfig->axis[AA_MOVE], c_axisBinding, IM_ARRAYSIZE(c_axisBinding));

				ImGui::Separator();

				ImGui::PushFont(s_dialogFont);
				ImGui::LabelText("##ConfigLabel", "Sensitivity");
				ImGui::PopFont();

				ImGui::LabelText("##ConfigLabel", "Left Stick");
				ImGui::SetNextItemWidth(196);
				ImGui::SliderFloat("##LeftSensitivity", &s_inputConfig->ctrlSensitivity[0], 0.0f, 4.0f); ImGui::SameLine(220);
				ImGui::SetNextItemWidth(64);
				ImGui::InputFloat("##LeftSensitivityInput", &s_inputConfig->ctrlSensitivity[0]);

				ImGui::LabelText("##ConfigLabel", "Right Stick");
				ImGui::SetNextItemWidth(196);
				ImGui::SliderFloat("##RightSensitivity", &s_inputConfig->ctrlSensitivity[1], 0.0f, 4.0f); ImGui::SameLine(220);
				ImGui::SetNextItemWidth(64);
				ImGui::InputFloat("##RightSensitivityInput", &s_inputConfig->ctrlSensitivity[1]);

				ImGui::Separator();

				ImGui::PushFont(s_dialogFont);
				ImGui::LabelText("##ConfigLabel", "Invert Axis");
				ImGui::PopFont();

				bool invertLeftHorz = (s_inputConfig->controllerFlags & CFLAG_INVERT_LEFT_HORZ) != 0;
				bool invertLeftVert = (s_inputConfig->controllerFlags & CFLAG_INVERT_LEFT_VERT) != 0;
				ImGui::LabelText("##ConfigLabel", "Left Stick Invert: "); ImGui::SameLine(175);
				ImGui::Checkbox("Horizontal##Left", &invertLeftHorz); ImGui::SameLine();
				ImGui::Checkbox("Vertical##Left", &invertLeftVert);
				if (invertLeftHorz) { s_inputConfig->controllerFlags |=  CFLAG_INVERT_LEFT_HORZ; }
				else                { s_inputConfig->controllerFlags &= ~CFLAG_INVERT_LEFT_HORZ; }
				if (invertLeftVert) { s_inputConfig->controllerFlags |=  CFLAG_INVERT_LEFT_VERT; }
				               else { s_inputConfig->controllerFlags &= ~CFLAG_INVERT_LEFT_VERT; }
				
				bool invertRightHorz = (s_inputConfig->controllerFlags & CFLAG_INVERT_RIGHT_HORZ) != 0;
				bool invertRightVert = (s_inputConfig->controllerFlags & CFLAG_INVERT_RIGHT_VERT) != 0;
				ImGui::LabelText("##ConfigLabel", "Right Stick Invert: "); ImGui::SameLine(175);
				ImGui::Checkbox("Horizontal##Right", &invertRightHorz); ImGui::SameLine();
				ImGui::Checkbox("Vertical##Right", &invertRightVert);
				if (invertRightHorz) { s_inputConfig->controllerFlags |=  CFLAG_INVERT_RIGHT_HORZ; }
				                else { s_inputConfig->controllerFlags &= ~CFLAG_INVERT_RIGHT_HORZ; }
				if (invertRightVert) { s_inputConfig->controllerFlags |=  CFLAG_INVERT_RIGHT_VERT; }
				                else { s_inputConfig->controllerFlags &= ~CFLAG_INVERT_RIGHT_VERT; }

				yNext += 400.0f;
			}
			else
			{
				yNext += 29.0f;
			}
		}
		else
		{
			yNext += s_controllerWinOpen ? 400.0f : 29.0f;
			ImGui::PopStyleVar();
		}
		ImGui::EndChild();
				
		ImGui::SetNextWindowPos(ImVec2(165.0f, yNext - scroll));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
		if (ImGui::BeginChild("##Mouse Options", ImVec2(390.0f, s_mouseWinOpen ? 250.0f : 29.0f), true, window_flags))
		{
			if (ImGui::Button("Mouse Options", ImVec2(370.0f, 0.0f)))
			{
				s_mouseWinOpen = !s_mouseWinOpen;
			}
			ImGui::PopStyleVar();

			if (s_mouseWinOpen)
			{
				ImGui::Spacing();

				ImGui::LabelText("##ConfigLabel", "Mouse Mode"); ImGui::SameLine(100);
				ImGui::SetNextItemWidth(160);
				ImGui::Combo("##MouseMode", (s32*)&s_inputConfig->mouseMode, c_mouseMode, IM_ARRAYSIZE(c_mouseMode));

				ImGui::Separator();

				ImGui::PushFont(s_dialogFont);
				ImGui::LabelText("##ConfigLabel", "Sensitivity");
				ImGui::PopFont();

				ImGui::LabelText("##ConfigLabel", "Horizontal");
				ImGui::SetNextItemWidth(196);
				ImGui::SliderFloat("##HorzSensitivity", &s_inputConfig->mouseSensitivity[0], 0.0f, 4.0f); ImGui::SameLine(220);
				ImGui::SetNextItemWidth(64);
				ImGui::InputFloat("##HorzSensitivityInput", &s_inputConfig->mouseSensitivity[0]);

				ImGui::LabelText("##ConfigLabel", "Vertical");
				ImGui::SetNextItemWidth(196);
				ImGui::SliderFloat("##VertSensitivity", &s_inputConfig->mouseSensitivity[1], 0.0f, 4.0f); ImGui::SameLine(220);
				ImGui::SetNextItemWidth(64);
				ImGui::InputFloat("##VertSensitivityInput", &s_inputConfig->mouseSensitivity[1]);

				ImGui::Separator();

				ImGui::PushFont(s_dialogFont);
				ImGui::LabelText("##ConfigLabel", "Mouse Invert");
				ImGui::PopFont();

				bool invertHorz = (s_inputConfig->mouseFlags & MFLAG_INVERT_HORZ) != 0;
				bool invertVert = (s_inputConfig->mouseFlags & MFLAG_INVERT_VERT) != 0;
				ImGui::Checkbox("Horizontal##Mouse", &invertHorz); ImGui::SameLine();
				ImGui::Checkbox("Vertical##Mouse", &invertVert);
				if (invertHorz) { s_inputConfig->mouseFlags |=  MFLAG_INVERT_HORZ; }
				           else { s_inputConfig->mouseFlags &= ~MFLAG_INVERT_HORZ; }
				if (invertVert) { s_inputConfig->mouseFlags |=  MFLAG_INVERT_VERT; }
				           else { s_inputConfig->mouseFlags &= ~MFLAG_INVERT_VERT; }

				yNext += 250.0f;
			}
			else
			{
				yNext += 29.0f;
			}
		}
		else
		{
			yNext += s_controllerWinOpen ? 250.0f : 29.0f;
			ImGui::PopStyleVar();
		}
		ImGui::End();
		ImGui::SetNextWindowPos(ImVec2(165.0f, yNext - scroll));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
		f32 inputMappingHeight = 1360.0f;
		if (ImGui::BeginChild("##Input Mapping", ImVec2(390.0f, s_inputMappingOpen ? inputMappingHeight : 29.0f), true, window_flags))
		{
			if (ImGui::Button("Input Mapping", ImVec2(370.0f, 0.0f)))
			{
				s_inputMappingOpen = !s_inputMappingOpen;
			}
			ImGui::PopStyleVar();

			if (s_inputMappingOpen)
			{
				ImGui::Spacing();

				ImGui::PushFont(s_dialogFont);
				ImGui::LabelText("##ConfigLabel", "System");
				ImGui::PopFont();

				inputMapping("Console Toggle", IAS_CONSOLE);
				inputMapping("System Menu", IAS_SYSTEM_MENU);

				ImGui::Separator();

				ImGui::PushFont(s_dialogFont);
				ImGui::LabelText("##ConfigLabel", "Dark Forces - General");
				ImGui::PopFont();

				inputMapping("Menu Toggle",       IADF_MENU_TOGGLE);
				inputMapping("PDA Toggle",        IADF_PDA_TOGGLE);
				inputMapping("Night Vision",      IADF_NIGHT_VISION_TOG);
				inputMapping("Cleats",            IADF_CLEATS_TOGGLE);
				inputMapping("Gas Mask",          IADF_GAS_MASK_TOGGLE);
				inputMapping("Head Lamp",         IADF_HEAD_LAMP_TOGGLE);
				inputMapping("Headwave",          IADF_HEADWAVE_TOGGLE);
				inputMapping("HUD Toggle",        IADF_HUD_TOGGLE);
				inputMapping("Holster Weapon",    IADF_HOLSTER_WEAPON);
				inputMapping("Automount Toggle",  IADF_AUTOMOUNT_TOGGLE);
				inputMapping("Cycle Prev Weapon", IADF_CYCLEWPN_PREV);
				inputMapping("Cycle Next Weapon", IADF_CYCLEWPN_NEXT);
				inputMapping("Prev Weapon",       IADF_WPN_PREV);
				inputMapping("Pause",             IADF_PAUSE);
				inputMapping("Automap",           IADF_AUTOMAP);
								
				ImGui::Separator();

				ImGui::PushFont(s_dialogFont);
				ImGui::LabelText("##ConfigLabel", "Dark Forces - Automap");
				ImGui::PopFont();

				inputMapping("Zoom In",      IADF_MAP_ZOOM_IN);
				inputMapping("Zoom Out",     IADF_MAP_ZOOM_OUT);
				inputMapping("Enable Scroll",IADF_MAP_ENABLE_SCROLL);
				inputMapping("Scroll Up",    IADF_MAP_SCROLL_UP);
				inputMapping("Scroll Down",  IADF_MAP_SCROLL_DN);
				inputMapping("Scroll Left",  IADF_MAP_SCROLL_LT);
				inputMapping("Scroll Right", IADF_MAP_SCROLL_RT);
				inputMapping("Layer Up",     IADF_MAP_LAYER_UP);
				inputMapping("Layer Down",   IADF_MAP_LAYER_DN);

				ImGui::Separator();

				ImGui::PushFont(s_dialogFont);
				ImGui::LabelText("##ConfigLabel", "Dark Forces - Player");
				ImGui::PopFont();

				inputMapping("Forward",           IADF_FORWARD);
				inputMapping("Backward",          IADF_BACKWARD);
				inputMapping("Strafe Left",       IADF_STRAFE_LT);
				inputMapping("Strafe Right",      IADF_STRAFE_RT);
				inputMapping("Turn Left",         IADF_TURN_LT);
				inputMapping("Turn Right",        IADF_TURN_RT);
				inputMapping("Look Up",           IADF_LOOK_UP);
				inputMapping("Look Down",         IADF_LOOK_DN);
				inputMapping("Center View",       IADF_CENTER_VIEW);
				inputMapping("Run",               IADF_RUN);
				inputMapping("Walk Slowly",       IADF_SLOW);
				inputMapping("Crouch",            IADF_CROUCH);
				inputMapping("Jump",              IADF_JUMP);
				inputMapping("Use",               IADF_USE);
				inputMapping("Primary Fire",      IADF_PRIMARY_FIRE);
				inputMapping("Secondary Fire",    IADF_SECONDARY_FIRE);
				inputMapping("Fists",             IADF_WEAPON_1);
				inputMapping("Bryar Pistol",      IADF_WEAPON_2);
				inputMapping("E-11 Blaster",      IADF_WEAPON_3);
				inputMapping("Thermal Detonator", IADF_WEAPON_4);
				inputMapping("Repeater Gun",      IADF_WEAPON_5);
				inputMapping("Fusion Cutter",     IADF_WEAPON_6);
				inputMapping("I.M. Mines",        IADF_WEAPON_7);
				inputMapping("Mortar Gun",        IADF_WEAPON_8);
				inputMapping("Concussion Rifle",  IADF_WEAPON_9);
				inputMapping("Assault Cannon",    IADF_WEAPON_10);
								
				if (ImGui::BeginPopupModal("##ChangeBinding", NULL, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::Text("Press a key, controller button or mouse button.");

					KeyboardCode key = TFE_Input::getKeyPressed();
					Button ctrlBtn = TFE_Input::getControllerButtonPressed();
					MouseButton mouseBtn = TFE_Input::getMouseButtonPressed();
					Axis ctrlAnalog = TFE_Input::getControllerAnalogDown();

					u32 indices[2];
					s32 count = (s32)inputMapping_getBindingsForAction(InputAction(s_keyIndex), indices, 2);

					InputBinding* binding;
					InputBinding newBinding;
					if (s_keySlot >= count)
					{
						binding = &newBinding;
					}
					else
					{
						binding = inputMapping_getBindindByIndex(indices[s_keySlot]);
					}

					bool setBinding = false;
					if (key != KEY_UNKNOWN)
					{
						setBinding = true;

						memset(binding, 0, sizeof(InputBinding));
						binding->action = InputAction(s_keyIndex);
						binding->keyCode = key;
						binding->keyMod = s_keyMod;
						binding->type = ITYPE_KEYBOARD;
					}
					else if (ctrlBtn != CONTROLLER_BUTTON_UNKNOWN)
					{
						setBinding = true;

						memset(binding, 0, sizeof(InputBinding));
						binding->action = InputAction(s_keyIndex);
						binding->ctrlBtn = ctrlBtn;
						binding->type = ITYPE_CONTROLLER;
					}
					else if (mouseBtn != MBUTTON_UNKNOWN)
					{
						setBinding = true;

						memset(binding, 0, sizeof(InputBinding));
						binding->action = InputAction(s_keyIndex);
						binding->mouseBtn = mouseBtn;
						binding->type = ITYPE_MOUSE;
					}
					else if (ctrlAnalog != AXIS_UNKNOWN)
					{
						setBinding = true;

						memset(binding, 0, sizeof(InputBinding));
						binding->action = InputAction(s_keyIndex);
						binding->axis = ctrlAnalog;
						binding->type = ITYPE_CONTROLLER_AXIS;
					}

					if (setBinding)
					{
						if (s_keySlot >= count)
						{
							inputMapping_addBinding(binding);
						}

						s_keyIndex = -1;
						s_keySlot = -1;
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}
			}
		}
		else
		{
			ImGui::PopStyleVar();
		}
		ImGui::End();
		ImGui::PopStyleVar();
		ImGui::SetCursorPosY(yNext + (s_inputMappingOpen ? inputMappingHeight : 29.0f));
	}

	void configGraphics()
	{
		TFE_Settings_Graphics* graphics = TFE_Settings::getGraphicsSettings();
		TFE_Settings_Window* window = TFE_Settings::getWindowSettings();

		//////////////////////////////////////////////////////
		// Window Settings.
		//////////////////////////////////////////////////////
		ImGui::PushFont(s_dialogFont);
		ImGui::LabelText("##ConfigLabel", "Window");
		ImGui::PopFont();

		bool fullscreen = window->fullscreen;
		bool windowed = !fullscreen;
		bool vsync = true;
		if (ImGui::Checkbox("Fullscreen", &fullscreen))
		{
			windowed = !fullscreen;
		}
		// TODO
		ImGui::SameLine();
		if (ImGui::Checkbox("Vsync", &vsync))
		{
		}
		if (ImGui::Checkbox("Windowed", &windowed))
		{
			fullscreen = !windowed;
		}
		if (fullscreen != window->fullscreen)
		{
			TFE_RenderBackend::enableFullscreen(fullscreen);
		}
		window->fullscreen = fullscreen;
		ImGui::Separator();

		//////////////////////////////////////////////////////
		// Resolution
		//////////////////////////////////////////////////////
		// TODO
		bool widescreen = graphics->widescreen;

		ImGui::PushFont(s_dialogFont);
		ImGui::LabelText("##ConfigLabel", "Virtual Resolution");
		ImGui::PopFont();

		ImGui::LabelText("##ConfigLabel", "Standard Resolution:"); ImGui::SameLine(150);
		ImGui::SetNextItemWidth(196);
		ImGui::Combo("##Resolution", &s_resIndex, widescreen ? c_resolutionsWide : c_resolutions, IM_ARRAYSIZE(c_resolutions));
		if (s_resIndex == TFE_ARRAYSIZE(c_resolutionDim))
		{
		}
		else if (s_resIndex == TFE_ARRAYSIZE(c_resolutionDim) + 1)
		{
			ImGui::LabelText("##ConfigLabel", "Custom:"); ImGui::SameLine(100);
			ImGui::SetNextItemWidth(196);
			ImGui::InputInt2("##CustomInput", graphics->gameResolution.m);
		}
		else
		{
			graphics->gameResolution = c_resolutionDim[s_resIndex];
			if (s_menuRetState != APP_STATE_MENU)
			{
				// TODO
				//TFE_GameLoop::changeResolution(graphics->gameResolution.x, graphics->gameResolution.z);
				//TFE_GameLoop::update(true);
				//TFE_GameLoop::draw();
			}
		}

		ImGui::Checkbox("Widescreen", &widescreen);
		if (widescreen != graphics->widescreen)
		{
			graphics->widescreen = widescreen;
			// TODO
			// TFE_GameLoop::changeResolution(graphics->gameResolution.x, graphics->gameResolution.z);
		}
		ImGui::Separator();

		//////////////////////////////////////////////////////
		// Renderer
		//////////////////////////////////////////////////////
		ImGui::PushFont(s_dialogFont);
		ImGui::LabelText("##ConfigLabel", "Rendering");
		ImGui::PopFont();

		ImGui::LabelText("##ConfigLabel", "Renderer:"); ImGui::SameLine(75);
		ImGui::SetNextItemWidth(196);
		ImGui::Combo("##Renderer", &s_rendererIndex, c_renderer, IM_ARRAYSIZE(c_renderer));
		if (s_rendererIndex == 0)
		{
			static bool s_perspectiveCorrect = false;

			bool prevAsync = graphics->asyncFramebuffer;
			bool prevColorConvert = graphics->gpuColorConvert;

			// Software
			ImGui::Checkbox("Async Framebuffer", &graphics->asyncFramebuffer);
			ImGui::Checkbox("GPU Color Conversion", &graphics->gpuColorConvert);
			ImGui::Checkbox("Perspective Correct 3DO Texturing", &graphics->perspectiveCorrectTexturing);

			if (prevAsync != graphics->asyncFramebuffer || prevColorConvert != graphics->gpuColorConvert)
			{
				// TODO
				// TFE_GameLoop::changeResolution(graphics->gameResolution.x, graphics->gameResolution.z);
			}
		}
		else if (s_rendererIndex == 1)
		{
			static const char* c_colorDepth[] = { "8-bit", "True color" };
			static const char* c_3d_object_sort[] = { "Software Sort", "Depth buffering" };
			static const char* c_near_tex_filter[] = { "None", "Bilinear", "Sharp Bilinear" };
			static const char* c_far_tex_filter[] = { "None", "Bilinear", "Trilinear", "Anisotropic" };
			static const char* c_colormap_interp[] = { "None", "Quantized", "Linear", "Smooth" };
			static const char* c_aa[] = { "None", "MSAA 2x", "MSAA 4x", "MSAA 8x" };

			static s32 s_colorDepth = 1;
			static s32 s_objectSort = 1;
			static s32 s_nearTexFilter = 2;
			static s32 s_farTexFilter = 3;
			static s32 s_colormapInterp = 3;
			static s32 s_msaa = 3;
			static f32 s_filterSharpness = 0.8f;

			const s32 comboOffset = 170;

			// Hardware
			ImGui::LabelText("##ConfigLabel", "Color Depth"); ImGui::SameLine(comboOffset);
			ImGui::SetNextItemWidth(196);
			if (ImGui::Combo("##ColorDepth", &s_colorDepth, c_colorDepth, IM_ARRAYSIZE(c_colorDepth)))
			{
				if (s_colorDepth == 0) { s_nearTexFilter = 0; s_farTexFilter = 0; }
				else { s_nearTexFilter = 2; s_farTexFilter = 3; }
			}

			if (s_colorDepth == 1)
			{
				ImGui::LabelText("##ConfigLabel", "Colormap Interpolation"); ImGui::SameLine(comboOffset);
				ImGui::SetNextItemWidth(196);
				ImGui::Combo("##ColormapInterp", &s_colormapInterp, c_colormap_interp, IM_ARRAYSIZE(c_colormap_interp));
			}

			ImGui::LabelText("##ConfigLabel", "3D Polygon Sorting"); ImGui::SameLine(comboOffset);
			ImGui::SetNextItemWidth(196);
			ImGui::Combo("##3DSort", &s_objectSort, c_3d_object_sort, IM_ARRAYSIZE(c_3d_object_sort));

			ImGui::LabelText("##ConfigLabel", "Texture Filter Near"); ImGui::SameLine(comboOffset);
			ImGui::SetNextItemWidth(196);
			ImGui::Combo("##TexFilterNear", &s_nearTexFilter, c_near_tex_filter, IM_ARRAYSIZE(c_near_tex_filter));

			if (s_nearTexFilter == 2)
			{
				ImGui::SetNextItemWidth(196);
				ImGui::SliderFloat("Sharpness", &s_filterSharpness, 0.0f, 1.0f);
				ImGui::Spacing();
			}

			ImGui::LabelText("##ConfigLabel", "Texture Filter Far"); ImGui::SameLine(comboOffset);
			ImGui::SetNextItemWidth(196);
			ImGui::Combo("##TexFilterFar", &s_farTexFilter, c_far_tex_filter, IM_ARRAYSIZE(c_far_tex_filter));

			ImGui::LabelText("##ConfigLabel", "Anti-aliasing"); ImGui::SameLine(comboOffset);
			ImGui::SetNextItemWidth(196);
			ImGui::Combo("##MSAA", &s_msaa, c_aa, IM_ARRAYSIZE(c_aa));
		}
		ImGui::Separator();

		//////////////////////////////////////////////////////
		// Color Correction
		//////////////////////////////////////////////////////
		ImGui::PushFont(s_dialogFont);
		ImGui::LabelText("##ConfigLabel", "Color Correction");
		ImGui::PopFont();

		ImGui::Checkbox("Enable", &graphics->colorCorrection);
		if (graphics->colorCorrection)
		{
			ImGui::SetNextItemWidth(196);
			ImGui::SliderFloat("Brightness", &graphics->brightness, 0.0f, 2.0f);

			ImGui::SetNextItemWidth(196);
			ImGui::SliderFloat("Contrast", &graphics->contrast, 0.0f, 2.0f);

			ImGui::SetNextItemWidth(196);
			ImGui::SliderFloat("Saturation", &graphics->saturation, 0.0f, 2.0f);

			ImGui::SetNextItemWidth(196);
			ImGui::SliderFloat("Gamma", &graphics->gamma, 0.0f, 2.0f);
		}
		else
		{
			// If color correction is disabled, reset values.
			graphics->brightness = 1.0f;
			graphics->contrast   = 1.0f;
			graphics->saturation = 1.0f;
			graphics->gamma      = 1.0f;
		}

		const ColorCorrection colorCorrection = { graphics->brightness, graphics->contrast, graphics->saturation, graphics->gamma };
		TFE_RenderBackend::setColorCorrection(graphics->colorCorrection, &colorCorrection);

		ImGui::Separator();

		//////////////////////////////////////////////////////
		// Post-FX
		//////////////////////////////////////////////////////
		ImGui::PushFont(s_dialogFont);
		ImGui::LabelText("##ConfigLabel", "Post-FX");
		ImGui::PopFont();

		static bool s_bloom = false;
		static f32  s_bloomSoftness = 0.5f;
		static f32  s_bloomIntensity = 0.5f;

		//ImGui::Checkbox("Bloom", &s_bloom);
		ImGui::TextColored({ 1.0f, 1.0f, 1.0f, 0.33f }, "Bloom [TODO]");
		if (s_bloom)
		{
			ImGui::SetNextItemWidth(196);
			ImGui::SliderFloat("Softness", &s_bloomSoftness, 0.0f, 1.0f);
			ImGui::SetNextItemWidth(196);
			ImGui::SliderFloat("Intensity", &s_bloomIntensity, 0.0f, 1.0f);
		}
	}

	void configHud()
	{
		TFE_Settings_Hud* hud = TFE_Settings::getHudSettings();
		TFE_Settings_Window* window = TFE_Settings::getWindowSettings();

		ImGui::LabelText("##ConfigLabel", "Hud Scale Type:"); ImGui::SameLine(150);
		ImGui::SetNextItemWidth(196);
		ImGui::Combo("##HudScaleType", (s32*)&hud->hudScale, c_tfeHudScaleStrings, IM_ARRAYSIZE(c_tfeHudScaleStrings));

		ImGui::LabelText("##ConfigLabel", "Hud Position Type:"); ImGui::SameLine(150);
		ImGui::SetNextItemWidth(196);
		ImGui::Combo("##HudPosType", (s32*)&hud->hudPos, c_tfeHudPosStrings, IM_ARRAYSIZE(c_tfeHudPosStrings));

		ImGui::Separator();

		ImGui::SetNextItemWidth(196);
		ImGui::SliderFloat("Scale", &hud->scale, 0.0f, 15.0f, "%.2f");

		ImGui::SetNextItemWidth(196);
		ImGui::SliderInt("Offset X", &hud->pixelOffset[0], -512, 512);

		ImGui::SetNextItemWidth(196);
		ImGui::SliderInt("Offset Y", &hud->pixelOffset[1], -512, 512);

		// TODO
		// TFE_GameLoop::update(true);
		// TFE_GameLoop::draw();
	}

	void configSound()
	{
		ImGui::PushFont(s_dialogFont);
		ImGui::LabelText("##ConfigLabel", "Under Construction");
		ImGui::PopFont();
	}

	void pickCurrentResolution()
	{
		TFE_Settings_Graphics* graphics = TFE_Settings::getGraphicsSettings();

		const size_t count = TFE_ARRAYSIZE(c_resolutionDim);
		for (size_t i = 0; i < count; i++)
		{
			if (graphics->gameResolution.x == c_resolutionDim[i].x && graphics->gameResolution.z == c_resolutionDim[i].z)
			{
				s_resIndex = s32(i);
				return;
			}
		}
		s_resIndex = count;
	}

	////////////////////////////////////////////////////////////////
	// Main menu callbacks
	////////////////////////////////////////////////////////////////
	void menuItem_Start()
	{
		s_appState = APP_STATE_GAME;
	}

	void menuItem_Manual()
	{
		s_subUI = FEUI_MANUAL;
	}

	void menuItem_Credits()
	{
		s_subUI = FEUI_CREDITS;
	}

	void menuItem_Settings()
	{
		s_subUI = FEUI_CONFIG;
		s_configTab = CONFIG_GAME;

		pickCurrentResolution();
	}

	void menuItem_Mods()
	{
	}

	void menuItem_Editor()
	{
		s_appState = APP_STATE_EDITOR;
	}

	void menuItem_Exit()
	{
		s_appState = APP_STATE_QUIT;
	}
}