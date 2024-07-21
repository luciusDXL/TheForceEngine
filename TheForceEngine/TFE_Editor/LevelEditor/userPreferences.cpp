#include "userPreferences.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "tabControl.h"
#include "sharedState.h"
#include <TFE_Editor/editor.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_System/math.h>
#include <TFE_Ui/ui.h>
#include <algorithm>

namespace LevelEditor
{
	enum PreferencesTab
	{
		PTAB_INTERFACE = 0,
		PTAB_EDITING,
		PTAB_INPUT,
		PTAB_THEME,
		PTAB_COUNT
	};
	const char* c_prefTabs[PTAB_COUNT] = { "Interface", "Editing", "Input", "Theme" };
	const char* c_prefToolTips[PTAB_COUNT] =
	{
		"Interface.\nLevel editor interface options.",
		"Editing.\nEditing options, such as undo and curve settings.",
		"Input.\nShortcuts and input settings.",
		"Theme.\nEditor theme, UI and viewport colors.",
	};
	static PreferencesTab s_prefTab = PTAB_INTERFACE;

	void commitCurChanges(void)
	{
	}

	bool userPreferences()
	{
		TFE_Editor::pushFont(TFE_Editor::FONT_SMALL);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

		bool applyChanges = false;
		bool cancel = false;
		if (ImGui::BeginPopupModal("User Preferences", nullptr, window_flags))
		{
			s_prefTab = (PreferencesTab)handleTabs(s_prefTab, 0, 0, PTAB_COUNT, c_prefTabs, c_prefToolTips, commitCurChanges);
			// Several tabs: Interface (Prompt quit, UI settings), Editing (undo settings, curve settings, etc.), Input (keyboard bindings), Theme (colors), 
			ImGui::Separator();

			switch (s_prefTab)
			{
				case PTAB_INTERFACE:
				{
					ImGui::LabelText("##Label", "Interface Placeholder");
				} break;
				case PTAB_EDITING:
				{
					ImGui::SetNextItemWidth(160);
					ImGui::LabelText("##Label", "Curve Segment Size"); ImGui::SameLine();
					ImGui::SliderFloat("##Curve Segment Size", &TFE_Editor::s_editorConfig.curve_segmentSize, 0.1f, 100.0f, "%.2f");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(128);
					ImGui::InputFloat("##CurveSegmentSizeInput", &TFE_Editor::s_editorConfig.curve_segmentSize, 0.1f, 1.0f, "%.2f");
				} break;
				case PTAB_INPUT:
				{
					ImGui::LabelText("##Label", "Input Placeholder");
				} break;
				case PTAB_THEME:
				{
					ImGui::LabelText("##Label", "Theme Placeholder");
				} break;
			}

			ImGui::Separator();
			if (ImGui::Button("Apply"))
			{
				TFE_Editor::saveConfig();
				applyChanges = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				TFE_Editor::loadConfig();
				cancel = true;
			}
			ImGui::EndPopup();
		}
		TFE_Editor::popFont();

		return applyChanges || cancel;
	}
}  // namespace LevelEditor