#include "lighting.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "sharedState.h"
#include <TFE_Editor/editor.h>
#include <TFE_System/math.h>
#include <TFE_Ui/ui.h>
#include <algorithm>

namespace LevelEditor
{
	static f32 s_wallLightAngle = 0.78539816339744830962f;	// 45 degrees.
	static s32 s_wallLightRange[] = { -8, 8 };
	static bool s_wallBacklighting = true;
	static bool s_wallLightingScale = true;

	bool levelLighting()
	{
		TFE_Editor::pushFont(TFE_Editor::FONT_SMALL);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

		bool applyLighting = false;
		bool cancel = false;
		if (ImGui::BeginPopupModal("Level Lighting", nullptr, window_flags))
		{
			ImGui::Text("Directional Wall Lighting");
			ImGui::NewLine();

			ImGui::Text("Light Angle");
			ImGui::SetNextItemWidth(256.0f);
			ImGui::SameLine(0.0f, 8.0f);
			ImGui::SliderAngle("##Light Angle", &s_wallLightAngle);

			ImGui::Text("Wall Light Range");
			ImGui::SetNextItemWidth(256.0f);
			ImGui::SameLine(0.0f, 8.0f);
			ImGui::SliderInt2("##Light Range", s_wallLightRange, -16, 16);
			if (s_wallLightRange[0] > s_wallLightRange[1]) { s_wallLightRange[0] = s_wallLightRange[1]; }

			ImGui::Text("Enable Wrap Lighting");
			ImGui::SameLine(0.0f, 8.0f);
			ImGui::Checkbox("##Wraplighting", &s_wallBacklighting);

			ImGui::Text("Scale with Sector Ambient");
			ImGui::SameLine(0.0f, 8.0f);
			ImGui::Checkbox("##ScaleAmbient", &s_wallLightingScale);

			ImGui::Separator();

			if (ImGui::Button("Apply"))
			{
				applyLighting = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				cancel = true;
			}
			ImGui::EndPopup();
		}
		TFE_Editor::popFont();

		if (applyLighting)
		{
			Vec2f dir = { sinf(s_wallLightAngle), cosf(s_wallLightAngle) };
			f32 minOffset = (f32)s_wallLightRange[0];
			f32 maxOffset = (f32)s_wallLightRange[1];
			f32 delta = maxOffset - minOffset;

			const s32 sectorCount = (s32)s_level.sectors.size();
			EditorSector* sector = s_level.sectors.data();
			for (s32 s = 0; s < sectorCount; s++, sector++)
			{
				if (!sector_isInteractable(sector) || !sector_onActiveLayer(sector)) { continue; }

				const f32 ambientScale = std::min(1.0f, sector->ambient / 30.0f);
				const s32 wallCount = (s32)sector->walls.size();
				const Vec2f* vtx = sector->vtx.data();
				EditorWall* wall = sector->walls.data();
				for (s32 w = 0; w < wallCount; w++, wall++)
				{
					Vec2f v0 = vtx[wall->idx[0]];
					Vec2f v1 = vtx[wall->idx[1]];
					Vec2f nrm = { -(v1.z - v0.z), v1.x - v0.x };
					nrm = TFE_Math::normalize(&nrm);

					f32 dp = dir.x*nrm.x + dir.z*nrm.z;
					if (s_wallBacklighting)
					{
						dp = std::max(0.0f, std::min(1.0f, dp * 0.5f + 0.5f));
					}
					else
					{
						dp = std::max(0.0f, std::min(1.0f, dp));
					}

					f32 offset = minOffset + dp * delta;
					if (s_wallLightingScale)
					{
						offset *= ambientScale;
					}

					wall->wallLight = s32(offset);
				}
			}
		}
		return applyLighting || cancel;
	}
}  // namespace LevelEditor