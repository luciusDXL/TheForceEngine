#include "snapshotUI.h"
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
	void snapshotUI_Begin()
	{
	}

	bool snapshotUI()
	{
		pushFont(FONT_SMALL);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
		bool exit = false;
		if (ImGui::BeginPopupModal("Snapshots", nullptr, window_flags))
		{
			ImGui::Separator();
			if (ImGui::Button("Exit"))
			{
				saveConfig();
				exit = true;
			}
			ImGui::EndPopup();
		}
		popFont();

		return exit;
	}
}  // namespace LevelEditor