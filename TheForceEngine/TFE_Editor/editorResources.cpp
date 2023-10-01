#include "editorResources.h"
#include "editor.h"
#include "editorConfig.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>

namespace TFE_Editor
{
	static std::vector<EditorResource> s_res;
	static bool s_ignoreVanilla = false;
	static bool s_resChanged = false;
	static s32 s_curResource = -1;
	static GameID s_gameId = Game_Dark_Forces;
	static char s_selectPath[TFE_MAX_PATH] = "";

	const std::vector<std::string> filters[] =
	{
		{ "GOB Archive", "*.GOB *.gob", "Zip", "*.ZIP *.zip" },  // Game_Dark_Forces
		{ "LAB Archive", "*.LAB *.lab", "Zip", "*.ZIP *.zip" },  // Game_Outlaws
	};

	void addSubArchives(const char* filepath, const char* name, ArchiveType type);

	bool resources_ui()
	{
		pushFont(TFE_Editor::FONT_SMALL);
		s32 menuHeight = 6 + (s32)ImGui::GetFontSize();

		bool active = true;
		bool finished = false;
		ImGui::SetWindowPos("Editor Resources", { 0.0f, f32(menuHeight) });
		ImGui::SetWindowSize("Editor Resources", { UI_SCALE(550), 70.0f + UI_SCALE(240) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

		ImGui::Begin("Editor Resources", &active, window_flags);
		{
			if (ImGui::Checkbox("Ignore Vanilla Assets", &s_ignoreVanilla))
			{
				s_resChanged = true;
			}

			ImGui::ListBoxHeader("##ResourceList", ImVec2(UI_SCALE(480), UI_SCALE(200)));
			const size_t count = s_res.size();
			const EditorResource* res = s_res.data();
			for (size_t i = 0; i < count; i++, res++)
			{
				bool isSelected = s_curResource == i;
				if (ImGui::Selectable(res->name, &isSelected))
				{
					s_curResource = s32(i);
				}
			}
			ImGui::ListBoxFooter();

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
					bool tryAddSubArchives = false;
					if (strcasecmp(ext, "GOB") == 0)
					{
						eRes.archive = Archive::getArchive(ARCHIVE_GOB, eRes.name, filepath);
					}
					else if (strcasecmp(ext, "ZIP") == 0)
					{
						eRes.archive = Archive::getArchive(ARCHIVE_ZIP, eRes.name, filepath);
						tryAddSubArchives = true;
					}
					s_res.push_back(eRes);

					if (tryAddSubArchives)
					{
						addSubArchives(filepath, eRes.name, ARCHIVE_ZIP);
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
					FileUtil::getFileNameFromPath(res[0].c_str(), eRes.name);
					strcpy(eRes.path, res[0].c_str());
					s_res.push_back(eRes);
				}
				s_resChanged = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove Resource"))
			{
				if (s_curResource >= 0)
				{
					s_res.erase(s_res.begin() + s_curResource);
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
		}
		ImGui::End();
		popFont();

		return false;
	}

	void resources_clear()
	{
		s_res.clear();
	}
		
	void resources_setGame(GameID gameId)
	{
		s_gameId = gameId;
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

	EditorResource* resources_get(u32& count)
	{
		count = (u32)s_res.size();
		return s_res.data();
	}

	/////////////////////////////////////////////
	// Internal
	/////////////////////////////////////////////
	void addSubArchives(const char* filepath, const char* name, ArchiveType type)
	{
		Archive* archive = Archive::getArchive(type, name, filepath);
		if (!archive) { return; }

		// TODO: Change based on game.
		const char* gameArchive = "GOB";
		ArchiveType newType = ARCHIVE_GOB;

		// TODO: Pull out temporary directory stuff, make sure to clear on exit.
		char tmpDir[TFE_MAX_PATH];
		sprintf(tmpDir, "%s/Temp", s_editorConfig.editorPath);
		FileUtil::fixupPath(tmpDir);
		if (!FileUtil::directoryExits(tmpDir))
		{
			FileUtil::makeDirectory(tmpDir);
		}

		const u32 count = archive->getFileCount();
		for (u32 i = 0; i < count; i++)
		{
			const char* filename = archive->getFileName(i);
			char ext[16];
			FileUtil::getFileExtension(filename, ext);
			if (strcasecmp(ext, gameArchive) == 0)
			{
				if (!archive->openFile(i)) { continue; }

				// Copy the file into a temporary location and then add it.
				char newPath[TFE_MAX_PATH];
				sprintf(newPath, "%s/%s", tmpDir, filename);

				WorkBuffer& buffer = getWorkBuffer();
				const size_t len = archive->getFileLength();
				buffer.resize(len);
				archive->readFile(buffer.data(), len);

				FileStream newArchive;
				if (newArchive.open(newPath, Stream::MODE_WRITE))
				{
					newArchive.writeBuffer(buffer.data(), len);
					newArchive.close();
				}
				archive->closeFile();

				// Now add the archive itself.
				EditorResource eRes;
				strcpy(eRes.name, filename);
				eRes.type = RES_ARCHIVE;
				eRes.archive = Archive::getArchive(newType, filename, newPath);
				s_res.push_back(eRes);
			}
		}
	}
}