#include "frontEndUi.h"
#include "console.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
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
		FEUI_COUNT
	};

	static AppState s_appState;
	static ImFont* s_menuFont;
	static ImFont* s_titleFont;
	static SubUI s_subUI;

	static bool s_consoleActive = false;
	
	void init()
	{
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_FrontEndUI::init");
		s_appState = APP_STATE_MENU;
		s_subUI = FEUI_NONE;

		ImGuiIO& io = ImGui::GetIO();
		s_menuFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 32);
		s_titleFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 48);

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

	void draw(bool drawFrontEnd)
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
	}
}