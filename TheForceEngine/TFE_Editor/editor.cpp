#include "editor.h"
#include "editorConfig.h"
#include "editorResources.h"
#include "editorProject.h"
#include <TFE_Settings/settings.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <TFE_Ui/imGUI/imgui_internal.h>

namespace TFE_Editor
{
	enum EditorMode
	{
		EDIT_CONFIG = 0,
		EDIT_RESOURCES,
		EDIT_NEW_PROJECT,
		EDIT_EDIT_PROJECT,
		EDIT_ASSET_BROWSER,
		EDIT_ASSET,
	};

	const ImVec4 c_textColors[] =
	{
		ImVec4(1.0f, 1.0f, 1.0f, 1.0f), // TEXTCLR_NORMAL
		ImVec4(0.5f, 1.0f, 0.5f, 1.0f), // TEXTCLR_TITLE_ACTIVE
		ImVec4(1.0f, 0.8f, 0.5f, 1.0f), // TEXTCLR_TITLE_INACTIVE
	};

	const char* c_readOnly = "<Read Only Mode>";

	struct MessageBox
	{
		bool active = false;
		char id[512];
		char msg[TFE_MAX_PATH * 2] = "";
	};
	
	static bool s_showPerf = true;
	static bool s_showEditor = true;
	static EditorMode s_editorMode = EDIT_ASSET_BROWSER;
	static EditorMode s_prevEditorMode = EDIT_ASSET_BROWSER;
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
		s_editorMode = EDIT_ASSET_BROWSER;
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

			ImGui::Text("%s", s_msgBox.msg);
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
		else if (s_editorMode == EDIT_RESOURCES)
		{
			resources_ui();
		}
		else if (s_editorMode == EDIT_ASSET_BROWSER)
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
		if (s_editorMode == EDIT_ASSET_BROWSER)
		{
			AssetBrowser::render();
		}
		return true;
	}

	ImVec4 getTextColor(EditorTextColor color)
	{
		return c_textColors[color];
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

	void disableNextItem()
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	void enableNextItem()
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}
		
	void drawTitle()
	{
		DisplayInfo info;
		TFE_RenderBackend::getDisplayInfo(&info);
		const s32 winWidth = info.width;

		const Project* project = project_get();
		const char* title = project->active ? project->name.c_str() : c_readOnly;
		const s32 titleWidth = (s32)ImGui::CalcTextSize(title).x;

		const ImVec4 titleColor = getTextColor(project->active ? TEXTCLR_TITLE_ACTIVE : TEXTCLR_TITLE_INACTIVE);
		ImGui::SameLine(f32((winWidth - titleWidth)/2));
		ImGui::TextColored(titleColor, title);
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
						s_prevEditorMode = EDIT_ASSET_BROWSER;
					}
					s_editorMode = EDIT_CONFIG;
				}
				if (ImGui::MenuItem("Resources", NULL, s_editorMode == EDIT_RESOURCES))
				{
					if (s_editorMode == EDIT_CONFIG) { saveConfig(); }
					s_editorMode = EDIT_RESOURCES;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Asset Browser", NULL, s_editorMode == EDIT_ASSET_BROWSER))
				{
					if (s_editorMode == EDIT_CONFIG) { saveConfig(); }
					s_editorMode = EDIT_ASSET_BROWSER;
				}

				// Disable the Asset Editor menu item, unless actually in an Asset Editor.
				// This allows the item to be highlighted, and to be able to switch back to
				// the Asset Browser using the menu.
				const bool disable = s_editorMode != EDIT_ASSET;
				if (disable) { disableNextItem(); }
				if (ImGui::MenuItem("Asset Editor", NULL, s_editorMode == EDIT_ASSET))
				{
					if (s_editorMode == EDIT_CONFIG) { saveConfig(); }
					s_editorMode = EDIT_ASSET;
				}
				if (disable) { enableNextItem(); }

				ImGui::Separator();
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
					if (s_editorMode == EDIT_ASSET_BROWSER)
					{
						AssetBrowser::selectAll();
					}
				}
				if (ImGui::MenuItem("Select None", NULL, (bool*)NULL))
				{
					if (s_editorMode == EDIT_ASSET_BROWSER)
					{
						AssetBrowser::selectNone();
					}
				}
				if (ImGui::MenuItem("Invert Selection", NULL, (bool*)NULL))
				{
					if (s_editorMode == EDIT_ASSET_BROWSER)
					{
						AssetBrowser::invertSelection();
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Project"))
			{
				if (ImGui::MenuItem("New", NULL, (bool*)NULL))
				{
					if (s_editorMode == EDIT_CONFIG) { saveConfig(); }
					//s_editorMode = EDIT_NEW_PROJECT;
				}
				if (ImGui::MenuItem("Open", NULL, (bool*)NULL))
				{
					if (s_editorMode == EDIT_CONFIG) { saveConfig(); }
					// TODO
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Edit", NULL, (bool*)NULL))
				{
					if (s_editorMode == EDIT_CONFIG) { saveConfig(); }
					//s_editorMode = EDIT_EDIT_PROJECT;
				}
				if (ImGui::MenuItem("Close", NULL, (bool*)NULL))
				{
					project_close();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Export", NULL, (bool*)NULL))
				{
				}
				ImGui::Separator();
				if (ImGui::BeginMenu("Recents"))
				{
					if (ImGui::MenuItem("1 Mt. Kurek", NULL, (bool*)NULL))
					{
					}
					if (ImGui::MenuItem("2 Test Project", NULL, (bool*)NULL))
					{
					}
					if (ImGui::MenuItem("3 Dark Tide 2", NULL, (bool*)NULL))
					{
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}

			drawTitle();
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
		
	ArchiveType getArchiveType(const char* filename)
	{
		ArchiveType type = ARCHIVE_UNKNOWN;

		char ext[16];
		FileUtil::getFileExtension(filename, ext);
		if (strcasecmp(ext, "GOB") == 0)
		{
			type = ARCHIVE_GOB;
		}
		else if (strcasecmp(ext, "LAB") == 0)
		{
			type = ARCHIVE_LAB;
		}
		else if (strcasecmp(ext, "LFD") == 0)
		{
			type = ARCHIVE_LFD;
		}
		else if (strcasecmp(ext, "ZIP") == 0)
		{
			type = ARCHIVE_ZIP;
		}

		return type;
	}

	Archive* getArchive(const char* name, GameID gameId)
	{
		TFE_Settings_Game* game = TFE_Settings::getGameSettings();
		char filePath[TFE_MAX_PATH];
		sprintf(filePath, "%s%s", game->header[gameId].sourcePath, name);

		ArchiveType type = getArchiveType(name);
		return type != ARCHIVE_UNKNOWN ? Archive::getArchive(type, name, filePath) : nullptr;
	}

	void getTempDirectory(char* tmpDir)
	{
		sprintf(tmpDir, "%s/Temp", s_editorConfig.editorPath);
		FileUtil::fixupPath(tmpDir);
		if (!FileUtil::directoryExits(tmpDir))
		{
			FileUtil::makeDirectory(tmpDir);
		}
	}
}
