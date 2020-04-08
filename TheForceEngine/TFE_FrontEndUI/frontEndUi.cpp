#include "frontEndUi.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui_file_browser.h>
#include <TFE_Ui/imGUI/imgui.h>

namespace TFE_FrontEndUI
{
	static AppState s_appState;
	static ImFont* s_menuFont;

	void init()
	{
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_FrontEndUI::init");
		s_appState = APP_STATE_MENU;

		ImGuiIO& io = ImGui::GetIO();
		s_menuFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSansMono.ttf", 32);
	}

	void shutdown()
	{
	}

	AppState update()
	{
		return s_appState;
	}

	void setAppState(AppState state)
	{
		s_appState = state;
	}

	void draw()
	{
		DisplayInfo display;
		TFE_RenderBackend::getDisplayInfo(&display);
		u32 w = display.width;
		u32 h = display.height;
		u32 menuWidth = 173;
		u32 menuHeight = 224;

		ImGui::PushFont(s_menuFont);

		bool active = true;
		ImGui::SetNextWindowPos(ImVec2((w - menuWidth)/2, (h - menuHeight)/2));
		ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight));
		ImGui::Begin("##MainMenu", &active, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
											ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

		if (ImGui::Button("Start    "))
		{
			s_appState = APP_STATE_DARK_FORCES;
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
}