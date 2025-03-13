#include "editorLevel.h"
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/editorProject.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_System/iniParser.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Game/igame.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>

#include <algorithm>

namespace TFE_Editor
{
	struct NewLevel
	{
		char name[256] = "Secbase";
		s32 slotId = 0;
	};

	const char* c_darkForcesSlots[] =
	{
		"SECBASE",
		"TALAY",
		"SEWERS",
		"TESTBASE",
		"GROMAS",
		"DTENTION",
		"RAMSHED",
		"ROBOTICS",
		"NARSHADA",
		"JABSHIP",
		"IMPCITY",
		"FUELSTAT",
		"EXECUTOR",
		"ARC",
	};
		
	static NewLevel s_newLevel = {};

	const char* level_getDarkForcesSlotName(s32 index)
	{
		return c_darkForcesSlots[index];
	}

	s32 level_getDarkForcesSlotCount()
	{
		return TFE_ARRAYSIZE(c_darkForcesSlots);
	}

	void level_prepareNew()
	{
		s_newLevel = NewLevel{};
	}

	bool level_createEmpty(NewLevel& level)
	{
		char levelPath[TFE_MAX_PATH];
		Project* project = project_get();
		assert(project);

		sprintf(levelPath, "%s/%s.LEV", project->path, c_darkForcesSlots[level.slotId]);
		FileStream file;
		if (!file.open(levelPath, Stream::MODE_WRITE))
		{
			return false;
		}

		char buffer[256];
		strcpy(buffer, "LEV 2.1\r\n");
		file.writeBuffer(buffer, (u32)strlen(buffer));

		sprintf(buffer, "LEVELNAME %s\r\n", level.name);
		file.writeBuffer(buffer, (u32)strlen(buffer));

		sprintf(buffer, "PALETTE %s.PAL\r\n", c_darkForcesSlots[level.slotId]);
		file.writeBuffer(buffer, (u32)strlen(buffer));

		strcpy(buffer, "MUSIC surfin.GMD\r\n");
		file.writeBuffer(buffer, (u32)strlen(buffer));

		strcpy(buffer, "PARALLAX 1024.0 1024.0\r\n");
		file.writeBuffer(buffer, (u32)strlen(buffer));

		strcpy(buffer, "TEXTURES 0\r\n");
		file.writeBuffer(buffer, (u32)strlen(buffer));

		strcpy(buffer, "NUMSECTORS 0\r\n");
		file.writeBuffer(buffer, (u32)strlen(buffer));
				
		file.close();
		return true;
	}

	bool level_newLevelUi()
	{
		pushFont(TFE_Editor::FONT_SMALL);

		DisplayInfo info;
		TFE_RenderBackend::getDisplayInfo(&info);
		f32 width = std::min((f32)info.width - 64.0f, UI_SCALE(512));
		f32 height = std::min((f32)info.height - 64.0f, 32.0f + UI_SCALE(96));

		bool finished = false;
		ImGui::SetWindowSize("New Level", { width, height });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize;

		if (ImGui::BeginPopupModal("New Level", nullptr, window_flags))
		{
			ImGui::Text("Name"); ImGui::SameLine();
			ImGui::InputText("##NameLevel", s_newLevel.name, TFE_ARRAYSIZE(s_newLevel.name));

			ImGui::Text("Slot"); ImGui::SameLine();
			ImGui::Combo("##SlotLevel", &s_newLevel.slotId, c_darkForcesSlots, TFE_ARRAYSIZE(c_darkForcesSlots));

			ImGui::Separator();

			if (ImGui::Button("Create Level##Level"))
			{
				level_createEmpty(s_newLevel);
				AssetBrowser::rebuildAssets();
				finished = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel##Level"))
			{
				finished = true;
			}
			ImGui::EndPopup();
		}
		popFont();

		return finished;
	}
}
