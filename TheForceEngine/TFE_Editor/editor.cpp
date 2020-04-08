#include "editor.h"
#include "perfWindow.h"
#include "archiveViewer.h"
#include "levelEditor.h"
#include "Help/helpWindow.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui_file_browser.h>
#include <TFE_Ui/imGUI/imgui.h>

namespace TFE_Editor
{
	enum EditorMode
	{
		EDIT_ASSET = 0,
		EDIT_LEVEL,
	};
	
	static bool s_showPerf = true;
	static bool s_showEditor = true;
	static TFE_Renderer* s_renderer = nullptr;
	static EditorMode s_editorMode = EDIT_ASSET;
	static Help s_showHelp = HELP_NONE;
	static bool s_exitEditor = false;
	
	void menu();

	void enable(TFE_Renderer* renderer)
	{
		// Ui begin/render is called so we have a "UI Frame" in which to setup UI state.
		TFE_Ui::begin();

		ArchiveViewer::init(renderer);
		LevelEditor::init(renderer);
		HelpWindow::init();

		TFE_Ui::render();
		s_exitEditor = false;
	}

	void disable()
	{
		LevelEditor::disable();
	}

	bool update()
	{
		bool fullscreen = s_editorMode == EDIT_ASSET ? ArchiveViewer::isFullscreen() : LevelEditor::isFullscreen();
		if (!fullscreen) { menu(); }

		if (s_editorMode == EDIT_ASSET)
			ArchiveViewer::draw(&s_showEditor);
		else if (s_editorMode == EDIT_LEVEL)
			LevelEditor::draw(&s_showEditor);

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);
		if (s_showHelp != HELP_NONE)
		{
			if (!HelpWindow::show(s_showHelp, displayInfo.width / 2, displayInfo.height - 200))
			{
				s_showHelp = HELP_NONE;
			}
		}

		if (s_exitEditor)
		{
			disable();
		}

		return s_exitEditor;
	}

	bool render()
	{
		if (s_editorMode == EDIT_ASSET)
			return ArchiveViewer::render3dView();
		else if (s_editorMode == EDIT_LEVEL)
			return LevelEditor::render3dView();

		return false;
	}

	void showPerf(u32 frame)
	{
		const f64 dt = TFE_System::getDeltaTime();
		PerfWindow::setCurrentDt(dt, frame);
		PerfWindow::draw(&s_showPerf);
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
		beginMenuBar();
		{
			// General menu items.
			if (ImGui::BeginMenu("Editor"))
			{
				if (ImGui::MenuItem("Asset Viewer", NULL, s_editorMode == EDIT_ASSET))
				{
					s_editorMode = EDIT_ASSET;
				}
				if (ImGui::MenuItem("Level Editor", NULL, s_editorMode == EDIT_LEVEL))
				{
					s_editorMode = EDIT_LEVEL;
				}
				if (ImGui::MenuItem("Return to Game", NULL, (bool*)NULL))
				{
					s_exitEditor = true;
				}
				if (ImGui::MenuItem("Exit", NULL, (bool*)NULL))
				{
				}
				ImGui::EndMenu();
			}
			// Level Editor specific menu items.
			if (s_editorMode == EDIT_LEVEL)
			{
				LevelEditor::menu();
			}
			// Help
			if (ImGui::BeginMenu("Help"))
			{
				if (ImGui::MenuItem("About", "", (bool*)NULL))
				{
					s_showHelp = HELP_ABOUT;
				}
				if (ImGui::MenuItem("Manual", "", (bool*)NULL))
				{
					s_showHelp = HELP_MANUAL;
				}
				ImGui::EndMenu();
			}
		}
		endMenuBar();
	}
}
