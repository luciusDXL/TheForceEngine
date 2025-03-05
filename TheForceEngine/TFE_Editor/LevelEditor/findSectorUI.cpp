#include "findSectorUI.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "tabControl.h"
#include "sharedState.h"
#include "infoPanel.h"
#include "groups.h"
#include <TFE_Editor/editor.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/editorComboBox.h>
#include <TFE_System/math.h>
#include <TFE_Ui/ui.h>
#include <algorithm>

using namespace TFE_Editor;

namespace LevelEditor
{
	std::vector<ItemInfo> s_sectorList;
	static char s_sectorName[256];

	void findSectorUI_Begin()
	{
		editor_comboBoxInit();
		s_sectorList.clear();

		const s32 sectorCount = (s32)s_level.sectors.size();
		const EditorSector* sector = s_level.sectors.data();
		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			if (!sector->name.empty())
			{
				s_sectorList.push_back({sector->name, s});
			}
		}

		memset(s_sectorName, 0, 256);
	}

	bool findSectorUI()
	{
		pushFont(FONT_SMALL);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

		bool applyChanges = false;
		bool cancel = false;
		if (ImGui::BeginPopupModal("Find Sector", nullptr, window_flags))
		{
			char targetBuffer[256];
			strcpy(targetBuffer, s_sectorName);
			ImGui::SetNextItemWidth(196.0f);
			if (editor_itemEditComboBox(1, targetBuffer, 256, (s32)s_sectorList.size(), s_sectorList.data()))
			{
				strcpy(s_sectorName, targetBuffer);
			}

			ImGui::Separator();
			if (ImGui::Button("Select"))
			{
				const s32 id = findSectorByName(s_sectorName);
				if (id >= 0 && id < (s32)s_level.sectors.size())
				{
					EditorSector* sector = &s_level.sectors[id];
					// Change to sector mode.
					edit_setEditMode(LEDIT_SECTOR);
					// Select the sector.
					selection_clear();
					selection_clearHovered();
					selection_action(SA_ADD, sector);
					// Scroll to the sector.
					scrollToSector(sector);
					// Select and open the group in the Info Panel.
					groups_select(sector->groupId);
					infoPanelOpenGroup(sector->groupId);
				}
				applyChanges = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				cancel = true;
			}
			editor_handleOverlayList();
			ImGui::EndPopup();
		}
		popFont();

		return applyChanges || cancel;
	}
}  // namespace LevelEditor