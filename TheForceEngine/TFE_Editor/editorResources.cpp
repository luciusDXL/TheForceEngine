#include "editorResources.h"
#include "editor.h"
#include "editorConfig.h"
#include <TFE_Settings/settings.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_System/iniParser.h>
#include <TFE_Editor/editorProject.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>

namespace TFE_Editor
{
	static std::vector<EditorResource> s_baseResources;
	static std::vector<EditorResource> s_extResources;
	static bool s_ignoreVanilla = false;
	static bool s_resChanged = false;
	static s32 s_curResource = -1;
	static GameID s_gameId = Game_Dark_Forces;
	static char s_selectPath[TFE_MAX_PATH] = "";

	// Dark Forces default game resources.
	// TODO: Make definitions external?
	const char* c_darkForcesArchives[] =
	{
		"DARK.GOB",
		"SOUNDS.GOB",
		"TEXTURES.GOB",
		"SPRITES.GOB",
	};
	// Archive filters.
	const std::vector<std::string> filters[] =
	{
		{ "GOB Archive", "*.GOB *.gob", "Zip", "*.ZIP *.zip" },  // Game_Dark_Forces
		{ "LAB Archive", "*.LAB *.lab", "Zip", "*.ZIP *.zip" },  // Game_Outlaws
	};
	
	// TODO: Add drag and drop functionality.
	bool resources_ui()
	{
		pushFont(TFE_Editor::FONT_SMALL);
		s32 menuHeight = 6 + (s32)ImGui::GetFontSize();

		bool finished = false;
		f32 winWidth = UI_SCALE(550);
		ImGui::SetWindowSize("Editor Resources", { winWidth, 70.0f + UI_SCALE(260) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize;

		if (ImGui::BeginPopupModal("Editor Resources", nullptr, window_flags))
		{
			if (ImGui::Checkbox("Ignore Vanilla Assets", &s_ignoreVanilla))
			{
				s_resChanged = true;
			}

			if (ImGui::BeginListBox("##ResourceList", ImVec2(UI_SCALE(480), UI_SCALE(200))))
			{
				const size_t count = s_extResources.size();
				const EditorResource* res = s_extResources.data();
				for (size_t i = 0; i < count; i++, res++)
				{
					bool isSelected = s_curResource == i;
					if (ImGui::Selectable(res->name, &isSelected))
					{
						s_curResource = s32(i);
					}
				}
				ImGui::EndListBox();
			}

			if (ImGui::Button("Add Archive"))
			{
				FileResult res = TFE_Ui::openFileDialog("Open Archive", s_selectPath, filters[s_gameId]);
				if (!res.empty())
				{
					EditorResource eRes;
					eRes.type = RES_ARCHIVE;
					const char* filepath = res[0].c_str();
					FileUtil::getFileNameFromPath(filepath, eRes.name, true);

					char ext[16];
					FileUtil::getFileExtension(filepath, ext);

					eRes.archive = nullptr;
					if (strcasecmp(ext, "GOB") == 0)
					{
						eRes.archive = Archive::getArchive(ARCHIVE_GOB, eRes.name, filepath);
					}
					else if (strcasecmp(ext, "ZIP") == 0)
					{
						eRes.archive = Archive::getArchive(ARCHIVE_ZIP, eRes.name, filepath);
					}
					if (eRes.archive)
					{
						s_extResources.push_back(eRes);
					}
				}

				s_resChanged = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Add Directory"))
			{
				FileResult res = TFE_Ui::directorySelectDialog("Path", s_selectPath);
				if (!res.empty())
				{
					EditorResource eRes;
					eRes.type = RES_DIRECTORY;
					FileUtil::getFileNameFromPath(res[0].c_str(), eRes.name, true);
					if (!res[0].empty())
					{
						strcpy(eRes.path, res[0].c_str());
						s_extResources.push_back(eRes);
					}
				}
				s_resChanged = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove Resource"))
			{
				if (s_curResource >= 0)
				{
					s_extResources.erase(s_extResources.begin() + s_curResource);
				}
				s_curResource = -1;
				s_resChanged = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Resources"))
			{
				resources_clear();
				s_resChanged = true;
			}
			ImGui::Separator();

			if (ImGui::Button("Close"))
			{
				finished = true;
			}
			ImGui::EndPopup();
		}
		popFont();

		if (finished)
		{
			project_save();
		}

		return finished;
	}

	void resources_clear()
	{
		s_extResources.clear();
	}
						
	void resources_setGame(GameID gameId)
	{
		s_gameId = gameId;
		s_baseResources.clear();

		const char** defaultResList = nullptr;
		s32 count = 0;
		if (gameId == Game_Dark_Forces)
		{
			defaultResList = c_darkForcesArchives;
			count = (s32)TFE_ARRAYSIZE(c_darkForcesArchives);
		}

		if (defaultResList)
		{
			s_baseResources.resize(count);
			for (s32 i = 0; i < count; i++)
			{
				EditorResource res;
				strcpy(res.name, defaultResList[i]);
				res.archive = getArchive(res.name, gameId);
				res.type = RES_ARCHIVE;
				s_baseResources[i] = res;
			}
		}
	}

	void resources_dirty()
	{
		s_resChanged = true;
	}

	bool resources_listChanged()
	{
		bool changed = s_resChanged;
		s_resChanged = false;

		return changed;
	}

	bool resources_ignoreVanillaAssets()
	{
		return s_ignoreVanilla;
	}

	EditorResource* resources_getExternal(u32& count)
	{
		count = (u32)s_extResources.size();
		return s_extResources.data();
	}

	EditorResource* resources_getBaseGame(u32& count)
	{
		count = (u32)s_baseResources.size();
		return s_baseResources.data();
	}

	const char* c_resTypeStr[] =
	{
		"Archive",   // RES_ARCHIVE
		"Directory", // RES_DIRECTORY
	};
	static ArchiveType s_curArchiveType;

	void resources_save(FileStream& outFile)
	{
		const s32 count = (s32)s_extResources.size();
		const EditorResource* res = s_extResources.data();
		for (s32 i = 0; i < count; i++)
		{
			TFE_IniParser::writeHeader(outFile, "Resource");
			TFE_IniParser::writeKeyValue_String(outFile, "ResName", res[i].name);
			TFE_IniParser::writeKeyValue_String(outFile, "ResType", c_resTypeStr[res[i].type]);
			if (res[i].type == RES_ARCHIVE)
			{
				TFE_IniParser::writeKeyValue_Int(outFile, "ResArchiveType", (s32)res[i].archive->getType());
				TFE_IniParser::writeKeyValue_String(outFile, "ResPath", res[i].archive->getPath());
			}
			else if (res[i].type == RES_DIRECTORY)
			{
				TFE_IniParser::writeKeyValue_String(outFile, "ResPath", res[i].path);
			}
		}
	}
		
	void resources_createExternalEmpty()
	{
		s_extResources.push_back({});
	}

	void resources_parse(const char* key, const char* value)
	{
		EditorResource& res = s_extResources.back();

		if (strcasecmp(key, "ResName") == 0)
		{
			strcpy(res.name, value);
		}
		else if (strcasecmp(key, "ResType") == 0)
		{
			const size_t count = TFE_ARRAYSIZE(c_resTypeStr);
			for (size_t i = 0; i < count; i++)
			{
				if (strcasecmp(value, c_resTypeStr[i]) == 0)
				{
					res.type = ResourceType(i);
					break;
				}
			}
		}
		else if (strcasecmp(key, "ResArchiveType") == 0)
		{
			s_curArchiveType = (ArchiveType)TFE_IniParser::parseInt(value);
		}
		else if (strcasecmp(key, "ResPath") == 0)
		{
			if (res.type == RES_ARCHIVE)
			{
				res.archive = Archive::getArchive(s_curArchiveType, res.name, value);
			}
			else
			{
				strcpy(res.path, value);
			}
		}
	}
}