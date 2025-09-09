#include "confirmDialogs.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "tabControl.h"
#include "sharedState.h"
#include <TFE_Editor/editor.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_System/math.h>
#include <TFE_System/utf8.h>
#include <TFE_Ui/ui.h>
#include <algorithm>

using namespace TFE_Editor;

namespace LevelEditor
{
	bool reloadConfirmation()
	{
		pushFont(FONT_SMALL);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

		bool exit = false;
		if (ImGui::BeginPopupModal("Reload Confirmation", nullptr, window_flags))
		{
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Do you wish to reload the level from the last save?");
			ImGui::Separator();
			if (ImGui::Button("YES"))
			{
				editor_reloadLevel();
				exit = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("NO"))
			{
				exit = true;
			}

			ImGui::EndPopup();
		}
		popFont();
		return exit;
	}

	bool exitSaveConfirmation()
	{
		pushFont(FONT_SMALL);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

		bool exit = false;
		if (ImGui::BeginPopupModal("Exit Without Saving", nullptr, window_flags))
		{
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "You have unsaved changes.");
			ImGui::Separator();
			if (ImGui::Button("Exit and Save Changes"))
			{
				editor_closeLevel(/*save*/true);
				exit = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Exit Without Saving"))
			{
				editor_closeLevel(/*save*/false);
				exit = true;
			}
			if (TFE_Editor::isInAssetEditor() && !TFE_Editor::isProgramExiting())
			{
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					exit = true;
				}
			}
			ImGui::EndPopup();
		}
		popFont();
		return exit;
	}
}  // namespace LevelEditor