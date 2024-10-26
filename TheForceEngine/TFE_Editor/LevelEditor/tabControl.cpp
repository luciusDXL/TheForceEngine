#include "tabControl.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "sharedState.h"
#include <TFE_Editor/editor.h>
#include <TFE_System/math.h>
#include <TFE_Ui/ui.h>
#include <algorithm>

namespace LevelEditor
{
	void setTabColor(bool isSelected)
	{
		if (isSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, { 0.0f, 0.25f, 0.5f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.0f, 0.25f, 0.5f, 1.0f });
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, { 0.25f, 0.25f, 0.25f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.5f, 0.5f, 0.5f, 1.0f });
		}
		// The active color must match to avoid flickering.
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.0f, 0.5f, 1.0f, 1.0f });
	}

	void restoreTabColor()
	{
		ImGui::PopStyleColor(3);
	}

	s32 handleTabs(s32 curTab, s32 offsetX, s32 offsetY, s32 count, const char** names, const char** tooltips, SwitchTabCallback switchCallback)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 4.0f, 1.0f });
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (f32)offsetX);

		for (s32 i = 0; i < count; i++)
		{
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (f32)offsetY);

			setTabColor(curTab == i);
			if (ImGui::Button(names[i], { 100.0f, 18.0f }))
			{
				curTab = i;
				if (switchCallback) { switchCallback(); }
			}
			TFE_Editor::setTooltip("%s", tooltips[i]);
			restoreTabColor();
			if (i < count - 1)
			{
				ImGui::SameLine(0.0f, 2.0f);
			}
		}
		ImGui::PopStyleVar(1);

		return curTab;
	}
}  // namespace LevelEditor