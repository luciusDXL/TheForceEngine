#include "testOptions.h"
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
	bool testOptions()
	{
		pushFont(FONT_SMALL);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

		bool applyChanges = false;
		bool cancel = false;
		if (ImGui::BeginPopupModal("Test Options", nullptr, window_flags))
		{
			s32 browseWinOpen = -1;
			ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "Ports");

			char utf8Path[TFE_MAX_PATH];
			ImGui::Text("Dark Forces:"); ImGui::SameLine(UI_SCALE(120));
			convertExtendedAsciiToUtf8(s_editorConfig.darkForcesPort, utf8Path);
			ImGui::InputText("##DarkForces", utf8Path, TFE_MAX_PATH);
			ImGui::SameLine();
			if (ImGui::Button("Browse##DarkForces"))
			{
				browseWinOpen = 0;
			}

			ImGui::Text("Outlaws:"); ImGui::SameLine(UI_SCALE(120));
			convertExtendedAsciiToUtf8(s_editorConfig.outlawsPort, utf8Path);
			ImGui::InputText("##Outlaws", utf8Path, TFE_MAX_PATH);
			ImGui::SameLine();
			if (ImGui::Button("Browse##Outlaws"))
			{
				browseWinOpen = 1;
			}
			ImGui::Separator();

			ImGui::CheckboxFlags("Dark Forces: Use TFE", &s_editorConfig.levelEditorFlags, LEVEDITOR_FLAG_RUN_TFE);

			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "Additional Command Line Arguments");
			ImGui::Text("Dark Forces:"); ImGui::SameLine(UI_SCALE(120));
			ImGui::InputText("##ArgumentsDarkForces", s_editorConfig.darkForcesAddCmdLine, TFE_MAX_PATH);
			ImGui::Text("Outlaws:"); ImGui::SameLine(UI_SCALE(120));
			ImGui::InputText("##ArgumentsOutlaws", s_editorConfig.outlawsAddCmdLine, TFE_MAX_PATH);
			ImGui::Separator();

			ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "Test Options");
			ImGui::CheckboxFlags("No Enemies", &s_editorConfig.levelEditorFlags, LEVEDITOR_FLAG_NO_ENEMIES);
			if (ImGui::CheckboxFlags("Easy", &s_editorConfig.levelEditorFlags, LEVEDITOR_FLAG_EASY))
			{
				if (s_editorConfig.levelEditorFlags&LEVEDITOR_FLAG_EASY) { s_editorConfig.levelEditorFlags &= ~LEVEDITOR_FLAG_HARD; }
			}
			bool medium = !(s_editorConfig.levelEditorFlags&LEVEDITOR_FLAG_EASY) && !(s_editorConfig.levelEditorFlags&LEVEDITOR_FLAG_HARD);
			if (ImGui::Checkbox("Medium", &medium))
			{
				if (medium)
				{
					s_editorConfig.levelEditorFlags &= ~LEVEDITOR_FLAG_EASY;
					s_editorConfig.levelEditorFlags &= ~LEVEDITOR_FLAG_HARD;
				}
			}
			if (ImGui::CheckboxFlags("Hard", &s_editorConfig.levelEditorFlags, LEVEDITOR_FLAG_HARD))
			{
				if (s_editorConfig.levelEditorFlags&LEVEDITOR_FLAG_HARD) { s_editorConfig.levelEditorFlags &= ~LEVEDITOR_FLAG_EASY; }
			}

			ImGui::Separator();
			if (ImGui::Button("Apply"))
			{
				saveConfig();
				applyChanges = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				loadConfig();
				cancel = true;
			}

			// File dialogs...
			if (browseWinOpen >= 0)
			{
				char exePath[TFE_MAX_PATH];
				const char* games[] =
				{
					"Select Dark Forces Port EXE",
					"Select Outlaws Port EXE"
				};
				const std::vector<std::string> filters[] =
				{
					{ "Executable", "*.EXE *.exe" },
					{ "Executable", "*.EXE *.exe" },
				};

				FileResult res = TFE_Ui::openFileDialog(games[browseWinOpen], DEFAULT_PATH, filters[browseWinOpen]);
				if (!res.empty() && !res[0].empty())
				{
					convertUtf8ToExtendedAscii(res[0].c_str(), exePath);
					FileUtil::fixupPath(exePath);

					if (browseWinOpen == 0)
					{
						strcpy(s_editorConfig.darkForcesPort, exePath);
					}
					else if (browseWinOpen == 1)
					{
						strcpy(s_editorConfig.outlawsPort, exePath);
					}
				}
			}

			ImGui::EndPopup();
		}
		popFont();

		return applyChanges || cancel;
	}
}  // namespace LevelEditor