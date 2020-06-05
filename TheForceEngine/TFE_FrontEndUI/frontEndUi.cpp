#include "frontEndUi.h"
#include "console.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Archive/archive.h>
#include <TFE_Settings/settings.h>
#include <TFE_Input/input.h>
#include <TFE_Ui/ui.h>
#include <TFE_Ui/markdown.h>
#include <TFE_Game/gameLoop.h>

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
		CONFIG_ABOUT=0,
		CONFIG_GAME,
		CONFIG_GRAPHICS,
		CONFIG_SOUND,
		CONFIG_COUNT
	};

	const char* c_configLabels[]=
	{
		"About",
		"Game Settings",
		"Graphics",
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
		{2048,1536}
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
		"Match Window",
		"Custom",
	};

	static const char* c_renderer[] =
	{
		"Classic (Software)",
		"Classic (Hardware)",
	};

	static s32 s_resIndex = 0;
	static s32 s_rendererIndex = 0;
	static char* s_aboutDisplayStr;
	
	static AppState s_appState;
	static AppState s_menuRetState;
	static ImFont* s_menuFont;
	static ImFont* s_titleFont;
	static ImFont* s_dialogFont;
	static SubUI s_subUI;
	static ConfigTab s_configTab;

	static bool s_consoleActive = false;
	static imgui_addons::ImGuiFileBrowser s_fileDialog;
	static bool s_relativeMode;
	static bool s_restartGame;

	void configAbout();
	void configGame();
	void configGraphics();
	void configSound();
	void pickCurrentResolution();
	
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
		s_titleFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 48);
		s_dialogFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 20);

		s_fileDialog.setCurrentPath(TFE_Paths::getPath(PATH_PROGRAM));

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
		
	void enableConfigMenu()
	{
		s_appState = APP_STATE_MENU;
		s_subUI = FEUI_CONFIG;
		s_relativeMode = TFE_Input::relativeModeEnabled();
		TFE_Input::enableRelativeMode(false);
		pickCurrentResolution();
	}

	void setMenuReturnState(AppState state)
	{
		s_menuRetState = state;
	}

	bool restartGame()
	{
		return s_restartGame;
	}

	bool shouldClearScreen()
	{
		return s_appState == APP_STATE_MENU && s_subUI == FEUI_NONE;
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
		u32 menuHeight = 264;

		if (TFE_Console::isOpen())
		{
			TFE_Console::update();
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

			if (ImGui::Button("Start   "))
			{
				s_appState = APP_STATE_DARK_FORCES;
			}
			if (ImGui::Button("Manual  "))
			{
				//s_subUI = FEUI_MANUAL;
			}
			if (ImGui::Button("Settings"))
			{
				s_subUI = FEUI_CONFIG;
				s_configTab = CONFIG_GAME;

				if (TFE_Paths::hasPath(PATH_SOURCE_DATA))
				{
					s_fileDialog.setCurrentPath(TFE_Paths::getPath(PATH_SOURCE_DATA));
				}
				else
				{
					s_fileDialog.setCurrentPath(TFE_Paths::getPath(PATH_PROGRAM));
				}
				pickCurrentResolution();
			}
			if (ImGui::Button("Mods    "))
			{
			}
			if (ImGui::Button("Editor  "))
			{
				s_appState = APP_STATE_EDITOR;
			}
			if (ImGui::Button("Exit    "))
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
			bool active = true;
			const u32 window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(160.0f, h));
			ImGui::Begin("##Sidebar", &active, window_flags);

			ImVec2 sideBarButtonSize(144, 24);
			ImGui::PushFont(s_dialogFont);
			if (ImGui::Button("About", sideBarButtonSize))
			{
				s_configTab = CONFIG_ABOUT;
				TFE_Settings::writeToDisk();
			}
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
				s_restartGame = s_menuRetState == APP_STATE_MENU;

				s_subUI = FEUI_NONE;
				s_appState = s_menuRetState;
				TFE_Settings::writeToDisk();
				TFE_Input::enableRelativeMode(s_relativeMode);
			}
			if (s_menuRetState != APP_STATE_MENU && ImGui::Button("Exit to Menu", sideBarButtonSize))
			{
				s_restartGame = true;
				s_menuRetState = APP_STATE_MENU;

				s_subUI = FEUI_NONE;
				s_appState = APP_STATE_MENU;
				TFE_Settings::writeToDisk();

				// End the current game.
				TFE_GameLoop::endLevel();
			}
			ImGui::PopFont();

			ImGui::End();

			// adjust the width based on tab.
			s32 tabWidth = w - 160;
			if (s_configTab == CONFIG_GRAPHICS)
			{
				tabWidth = 400;
			}

			ImGui::SetNextWindowPos(ImVec2(160.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(f32(tabWidth), h));
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
		ImGui::Combo("##Current Game", &s_gameIndex, c_games, IM_ARRAYSIZE(c_games));
		
		// File dialogs...
		char exePath[TFE_MAX_PATH];
		char filePath[TFE_MAX_PATH];
		if (s_fileDialog.showOpenFileDialog("Select DARK.EXE or DARK.GOB", ImVec2(600, 300), ".EXE,.exe,.GOB,.gob"))
		{
			strcpy(exePath, s_fileDialog.selected_fn.c_str());
			FileUtil::getFilePath(exePath, filePath);

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
		else if (s_fileDialog.showOpenFileDialog("Select OUTLAWS.EXE or OUTLAWS.LAB", ImVec2(600, 300), ".EXE,.exe,.LAB,.lab"))
		{
			strcpy(exePath, s_fileDialog.selected_fn.c_str());
			FileUtil::getFilePath(exePath, filePath);

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

		if (ImGui::BeginPopupModal("Invalid Source Data", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Invalid source data path!");
			ImGui::Text("Please select the source game executable or source asset (GOB or LAB).");

			if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
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
		static bool s_widescreen = false;

		ImGui::PushFont(s_dialogFont);
		ImGui::LabelText("##ConfigLabel", "Virtual Resolution");
		ImGui::PopFont();

		ImGui::LabelText("##ConfigLabel", "Standard Resolution:"); ImGui::SameLine(150);
		ImGui::SetNextItemWidth(196);
		ImGui::Combo("##Resolution", &s_resIndex, s_widescreen ? c_resolutionsWide : c_resolutions, IM_ARRAYSIZE(c_resolutions));
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
				TFE_GameLoop::changeResolution(graphics->gameResolution.x, graphics->gameResolution.z);
				TFE_GameLoop::update(true);
				TFE_GameLoop::draw();
			}
		}

		ImGui::Checkbox("Widescreen", &s_widescreen);
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
			static bool s_asyncFramebuffer = false;
			static bool s_gpuColorConv = true;
			static bool s_perspectiveCorrect = false;

			// Software
			ImGui::Checkbox("Async Framebuffer", &s_asyncFramebuffer);
			ImGui::Checkbox("GPU Color Conversion", &s_gpuColorConv);
			ImGui::Checkbox("Perspective Correct 3DO Texturing", &s_perspectiveCorrect);
		}
		else if (s_rendererIndex == 1)
		{
			static const char* c_colorDepth[]      = { "8-bit", "True color" };
			static const char* c_3d_object_sort[]  = { "Software Sort", "Depth buffering" };
			static const char* c_near_tex_filter[] = { "None", "Bilinear", "Sharp Bilinear" };
			static const char* c_far_tex_filter[]  = { "None", "Bilinear", "Trilinear", "Anisotropic" };
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
		static bool s_cgEnable = true;
		static f32 s_cgBrightness = 1.0f;
		static f32 s_cgContrast = 1.0f;
		static f32 s_cgSaturation = 1.0f;
		static f32 s_cgGamma = 1.0f;

		ImGui::PushFont(s_dialogFont);
		ImGui::LabelText("##ConfigLabel", "Color Correction");
		ImGui::PopFont();

		ImGui::Checkbox("Enable", &s_cgEnable);
		if (s_cgEnable)
		{
			ImGui::SetNextItemWidth(196);
			ImGui::SliderFloat("Brightness", &s_cgBrightness, 0.0f, 2.0f);

			ImGui::SetNextItemWidth(196);
			ImGui::SliderFloat("Contrast", &s_cgContrast, 0.0f, 2.0f);

			ImGui::SetNextItemWidth(196);
			ImGui::SliderFloat("Saturation", &s_cgSaturation, 0.0f, 2.0f);

			ImGui::SetNextItemWidth(196);
			ImGui::SliderFloat("Gamma", &s_cgGamma, 0.0f, 2.0f);
		}
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

		ImGui::Checkbox("Bloom", &s_bloom);
		if (s_bloom)
		{
			ImGui::SetNextItemWidth(196);
			ImGui::SliderFloat("Softness", &s_bloomSoftness, 0.0f, 1.0f);
			ImGui::SetNextItemWidth(196);
			ImGui::SliderFloat("Intensity", &s_bloomIntensity, 0.0f, 1.0f);
		}
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
				s_resIndex = i;
				return;
			}
		}
		s_resIndex = count;
	}
}