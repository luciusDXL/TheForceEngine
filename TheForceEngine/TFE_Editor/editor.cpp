#include "editor.h"
#include "editorConfig.h"
#include "editorLevel.h"
#include "editorResources.h"
#include "editorProject.h"
#include <TFE_Settings/settings.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Editor/LevelEditor/levelEditor.h>
#include <TFE_Editor/EditorAsset/editorAsset.h>
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
		EDIT_ASSET_BROWSER = 0,
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
		char id[512];
		char msg[TFE_MAX_PATH * 2] = "";
	};

	static std::vector<RecentProject> s_recents;
	
	static bool s_showPerf = true;
	static bool s_showEditor = true;
	static AssetType s_editorAssetType = TYPE_NOT_SET;
	static EditorMode s_editorMode = EDIT_ASSET_BROWSER;
	static EditorPopup s_editorPopup = POPUP_NONE;
	static u32 s_editorPopupUserData = 0;
	static void* s_editorPopupUserPtr = nullptr;
	static bool s_exitEditor = false;
	static bool s_configView = false;
	static WorkBuffer s_workBuffer;
	static char s_projectPath[TFE_MAX_PATH] = "";

	static bool s_menuActive = false;
	static MessageBox s_msgBox = {};
	static ImFont* s_fonts[FONT_COUNT * FONT_SIZE_COUNT] = { 0 };

	static f64 s_tooltipTime = 0.0;
	static Vec2i s_prevMousePos = { 0 };
	static const f64 c_tooltipDelay = 0.2;
	static bool s_canShowTooltips = false;
	
	void menu();
	void loadFonts();
	void updateTooltips();

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

	bool messageBoxUi()
	{
		bool finished = false;

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
				finished = true;
			}
			ImGui::EndPopup();
		}
		popFont();
		return finished;
	}

	bool isPopupOpen()
	{
		return s_editorPopup != POPUP_NONE;
	}

	void handlePopupBegin()
	{
		if (s_editorPopup == POPUP_NONE) { return; }
		switch (s_editorPopup)
		{
			case POPUP_MSG_BOX:
			{
				ImGui::OpenPopup(s_msgBox.id);
			} break;
			case POPUP_CONFIG:
			{
				ImGui::OpenPopup("Editor Config");
			} break;
			case POPUP_RESOURCES:
			{
				ImGui::OpenPopup("Editor Resources");
			} break;
			case POPUP_NEW_PROJECT:
			case POPUP_EDIT_PROJECT:
			{
				ImGui::OpenPopup("Project");
			} break;
			case POPUP_NEW_LEVEL:
			{
				ImGui::OpenPopup("New Level");
			} break;
			case POPUP_BROWSE:
			{
				ImGui::OpenPopup("Browse");
			} break;
		}
	}

	void handlePopupEnd()
	{
		if (s_editorPopup == POPUP_NONE) { return; }

		switch (s_editorPopup)
		{
			case POPUP_MSG_BOX:
			{
				if (messageBoxUi())
				{
					ImGui::CloseCurrentPopup();
					s_editorPopup = POPUP_NONE;
				}
			} break;
			case POPUP_CONFIG:
			{
				if (configUi())
				{
					ImGui::CloseCurrentPopup();
					s_editorPopup = POPUP_NONE;
				}
			} break;
			case POPUP_RESOURCES:
			{
				if (resources_ui())
				{
					ImGui::CloseCurrentPopup();
					s_editorPopup = POPUP_NONE;
				}
			} break;
			case POPUP_NEW_PROJECT:
			{
				if (project_editUi(true))
				{
					ImGui::CloseCurrentPopup();
					s_editorPopup = POPUP_NONE;
				}
			} break;
			case POPUP_EDIT_PROJECT:
			{
				if (project_editUi(false))
				{
					ImGui::CloseCurrentPopup();
					s_editorPopup = POPUP_NONE;
				}
			} break;
			case POPUP_NEW_LEVEL:
			{
				if (level_newLevelUi())
				{
					ImGui::CloseCurrentPopup();
					s_editorPopup = POPUP_NONE;
				}
			} break;
			case POPUP_BROWSE:
			{
				if (AssetBrowser::popup())
				{
					ImGui::CloseCurrentPopup();
					s_editorPopup = POPUP_NONE;
				}
			} break;
		}

		if (TFE_Input::keyPressed(KEY_ESCAPE))
		{
			if (s_editorPopup != POPUP_NONE)
			{
				// Make sure the config is saved.
				if (s_editorPopup == POPUP_CONFIG)
				{
					saveConfig();
				}

				s_editorPopup = POPUP_NONE;
				ImGui::CloseCurrentPopup();
			}
			else
			{
				s_exitEditor = true;
			}
		}
	}

	bool update(bool consoleOpen)
	{
		TFE_RenderBackend::clearWindow();

		updateTooltips();
		handlePopupBegin();
		menu();

		if (s_editorMode == EDIT_ASSET_BROWSER)
		{
			AssetBrowser::update();
		}
		else if (s_editorMode == EDIT_ASSET)
		{
			switch (s_editorAssetType)
			{
				case TYPE_LEVEL:
				{
					LevelEditor::update();
				} break;
				default:
				{
					// Invalid mode, this code should never be reached.
					assert(0);
					s_editorMode = EDIT_ASSET_BROWSER;
				}
			}
		}

		handlePopupEnd();
		
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

	bool getMenuActive()
	{
		return s_menuActive;
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
		const char* title = project->active ? project->name : c_readOnly;
		const s32 titleWidth = (s32)ImGui::CalcTextSize(title).x;

		const ImVec4 titleColor = getTextColor(project->active ? TEXTCLR_TITLE_ACTIVE : TEXTCLR_TITLE_INACTIVE);
		ImGui::SameLine(f32((winWidth - titleWidth)/2));
		ImGui::TextColored(titleColor, title);
	}

	void menu()
	{
		pushFont(FONT_SMALL);

		s_menuActive = false;
		beginMenuBar();
		{
			// General menu items.
			if (ImGui::BeginMenu("Editor"))
			{
				s_menuActive = true;
				if (ImGui::MenuItem("Editor Config", NULL, (bool*)NULL))
				{
					s_editorPopup = POPUP_CONFIG;
				}
				if (ImGui::MenuItem("Resources", NULL, (bool*)NULL))
				{
					s_editorPopup = POPUP_RESOURCES;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Asset Browser", NULL, s_editorMode == EDIT_ASSET_BROWSER))
				{
					s_editorMode = EDIT_ASSET_BROWSER;
				}

				// Disable the Asset Editor menu item, unless actually in an Asset Editor.
				// This allows the item to be highlighted, and to be able to switch back to
				// the Asset Browser using the menu.
				const bool disable = s_editorMode != EDIT_ASSET;
				if (disable) { disableNextItem(); }
				if (ImGui::MenuItem("Asset Editor", NULL, s_editorMode == EDIT_ASSET))
				{
					s_editorMode = EDIT_ASSET;
				}
				if (disable) { enableNextItem(); }

				ImGui::Separator();
				if (ImGui::MenuItem("Return to Game", NULL, (bool*)NULL))
				{
					s_exitEditor = true;
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Select"))
			{
				s_menuActive = true;
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
					else if (s_editorMode == EDIT_ASSET)
					{
						if (s_editorAssetType == TYPE_LEVEL)
						{
							LevelEditor::selectNone();
						}
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
				s_menuActive = true;
				if (ImGui::MenuItem("New", NULL, (bool*)NULL))
				{
					s_editorPopup = POPUP_NEW_PROJECT;
					project_prepareNew();
				}
				if (ImGui::MenuItem("Open", NULL, (bool*)NULL))
				{
					FileResult res = TFE_Ui::openFileDialog("Open Project", s_projectPath, { "Project", "*.INI *.ini" });
					if (!res.empty())
					{
						char filePath[TFE_MAX_PATH];
						strcpy(filePath, res[0].c_str());
						FileUtil::fixupPath(filePath);
						project_load(filePath);
					}
				}
				ImGui::Separator();
				bool projectActive = project_get()->active;
				if (!projectActive) { disableNextItem(); }
				if (ImGui::MenuItem("Edit", NULL, (bool*)NULL))
				{
					s_editorPopup = POPUP_EDIT_PROJECT;
					project_prepareEdit();
				}
				if (ImGui::MenuItem("Close", NULL, (bool*)NULL))
				{
					project_close();
				}
				if (!projectActive) { enableNextItem(); }
				ImGui::Separator();
				disableNextItem();  // Disable until it does something...
				if (ImGui::MenuItem("Export", NULL, (bool*)NULL))
				{
				}
				enableNextItem();
				ImGui::Separator();
				if (s_recents.empty()) { disableNextItem(); }
				if (ImGui::BeginMenu("Recent Projects"))
				{
					const size_t count = s_recents.size();
					const RecentProject* recents = s_recents.data();
					s32 removeId = -1;
					for (size_t i = 0; i < count; i++)
					{
						char item[TFE_MAX_PATH];
						sprintf(item, "%d %s", (s32)i + 1, recents[i].name.c_str());
						if (ImGui::MenuItem(item, NULL, (bool*)NULL))
						{
							if (!project_load(recents[i].path.c_str()))
							{
								// Remove from recents if it is no longer valid.
								removeId = s32(i);
							}
						}
					}
					ImGui::EndMenu();

					if (removeId >= 0)
					{
						s_recents.erase(s_recents.begin() + removeId);
						saveConfig();
					}
				}
				if (s_recents.empty()) { enableNextItem(); }
				ImGui::EndMenu();
			}
			// Allow the current Asset Editor to add its own menus.
			if (s_editorMode == EDIT_ASSET)
			{
				switch (s_editorAssetType)
				{
					case TYPE_LEVEL:
					{
						if (LevelEditor::menu())
						{
							s_menuActive = true;
						}
					} break;
					default:
					{
						// Invalid mode, this code should never be reached.
						assert(0);
					}
				}
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

		s_editorPopup = POPUP_MSG_BOX;
		strcpy(s_msgBox.msg, fullStr);
		sprintf(s_msgBox.id, "%s##MessageBox", type);
	}

	void openEditorPopup(EditorPopup popup, u32 userData, void* userPtr)
	{
		s_editorPopup = popup;
		s_editorPopupUserData = userData;
		s_editorPopupUserPtr = userPtr;

		// Initialization if needed.
		switch (popup)
		{
			case POPUP_BROWSE:
			{
				AssetBrowser::initPopup(AssetType(userData), (char*)userPtr);
			} break;
		}
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

	void clearRecents()
	{
		s_recents.clear();
	}
		
	void addToRecents(const char* path)
	{
		char name[256];
		FileUtil::getFileNameFromPath(path, name);

		// Does it already exist?
		const size_t count = s_recents.size();
		s32 foundId = -1;
		for (size_t i = 0; i < count; i++)
		{
			if (strcasecmp(s_recents[i].path.c_str(), path) == 0)
			{
				foundId = s32(i);
				break;
			}
		}
		if (foundId > 0)
		{
			// Move it to the front.
			RecentProject curRecent = s_recents[foundId];
			s_recents.erase(s_recents.begin() + foundId);
			s_recents.insert(s_recents.begin(), curRecent);
		}
		else if (foundId < 0 && count < 10)	// Only have up to 10.
		{
			// Otherwise add it.
			s_recents.push_back({name, path});
		}
		saveConfig();
	}

	void removeFromRecents(const char* path)
	{
		// Does it already exist?
		const size_t count = s_recents.size();
		s32 foundId = -1;
		for (size_t i = 0; i < count; i++)
		{
			if (strcasecmp(s_recents[i].path.c_str(), path) == 0)
			{
				foundId = s32(i);
				break;
			}
		}
		// Remove if it exists.
		if (foundId >= 0)
		{
			s_recents.erase(s_recents.begin() + foundId);
		}
	}

	std::vector<RecentProject>* getRecentProjects()
	{
		return &s_recents;
	}

	void listSelection(const char* labelText, const char** listValues, size_t listLen, s32* index, s32 comboOffset/*=96*/, s32 comboWidth/*=0*/)
	{
		ImGui::SetNextItemWidth(UI_SCALE(256));
		ImGui::LabelText("##Label", "%s", labelText); ImGui::SameLine(UI_SCALE(comboOffset));

		char comboId[256];
		sprintf(comboId, "##%s", labelText);
		if (comboWidth) { ImGui::SetNextItemWidth(UI_SCALE(comboWidth)); }
		ImGui::Combo(comboId, index, listValues, (s32)listLen);
	}

	void enableAssetEditor(Asset* asset)
	{
		if (!asset) { return; }

		// For now, only the levels have an asset editor.
		// TODO: Other asset editors.
		if (asset->type == TYPE_LEVEL)
		{
			s_editorMode = EDIT_ASSET;
			s_editorAssetType = asset->type;
			LevelEditor::init(asset);
		}
		else
		{
			showMessageBox("Warning", "The selected asset '%s' does not yet have an Asset Editor.", asset->name.c_str());
		}
	}

	void disableAssetEditor()
	{
		s_editorMode = EDIT_ASSET_BROWSER;
	}

	void updateTooltips()
	{
		Vec2i mouse;
		TFE_Input::getMousePos(&mouse.x, &mouse.z);
		bool mouseStationary = mouse.x == s_prevMousePos.x && mouse.z == s_prevMousePos.z;
		s_canShowTooltips = mouseStationary && s_tooltipTime >= c_tooltipDelay;
		if (mouseStationary)
		{
			s_tooltipTime += TFE_System::getDeltaTime();
		}
		else
		{
			s_tooltipTime = 0.0;
		}
		s_prevMousePos = mouse;
	}

	void setTooltip(const char* msg, ...)
	{
		if (!ImGui::IsItemHovered() || !s_canShowTooltips) { return; }

		char fullStr[TFE_MAX_PATH];
		va_list arg;
		va_start(arg, msg);
		vsprintf(fullStr, msg, arg);
		va_end(arg);

		ImGui::SetTooltip("%s", fullStr);
	}

	// Return true if 'str' matches the 'filter', taking into account special symbols.
	bool editorStringFilter(const char* str, const char* filter, size_t filterLength)
	{
		for (size_t i = 0; i < filterLength; i++)
		{
			if (filter[i] == '?') { continue; }
			if (tolower(str[i]) != tolower(filter[i])) { return false; }
		}
		return true;
	}
}
