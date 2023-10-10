#include "levelEditor.h"
#include "levelEditorData.h"
#include "infoPanel.h"
#include "sharedState.h"
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_System/math.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>

using namespace TFE_Editor;

namespace LevelEditor
{
	static s32 s_infoHeight;
		
	s32 infoPanelGetHeight()
	{
		return s_infoHeight;
	}

	void infoToolBegin(s32 height)
	{
		bool infoTool = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("Info Panel", { (f32)displayInfo.width - 480.0f, 22.0f });
		ImGui::SetWindowSize("Info Panel", { 480.0f, f32(height) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("Info Panel", &infoTool, window_flags); ImGui::SameLine();
	}

	void infoPanelMap()
	{
		char name[64];
		strcpy(name, s_level.name.c_str());

		ImGui::Text("Level Name:"); ImGui::SameLine();
		if (ImGui::InputText("", name, 64))
		{
			s_level.name = name;
		}
		ImGui::Text("Sector Count: %u", (u32)s_level.sectors.size());
		ImGui::Text("Bounds: (%0.3f, %0.3f, %0.3f)\n        (%0.3f, %0.3f, %0.3f)", s_level.bounds[0].x, s_level.bounds[0].y, s_level.bounds[0].z,
			s_level.bounds[1].x, s_level.bounds[1].y, s_level.bounds[1].z);
		ImGui::Text("Layer Range: [%d, %d]", s_level.layerRange[0], s_level.layerRange[1]);
		ImGui::Separator();
		//ImGui::LabelText("##GridLabel", "Grid Height");
		//ImGui::SetNextItemWidth(196.0f);
		//ImGui::InputFloat("##GridHeight", &s_gridHeight, 0.0f, 0.0f, "%0.2f", ImGuiInputTextFlags_CharsDecimal);
		//ImGui::Checkbox("Grid Auto Adjust", &s_gridAutoAdjust);
		//ImGui::Checkbox("Show Grid When Camera Is Inside a Sector", &s_showGridInSector);
	}

	void infoPanelVertex()
	{
		EditorSector* sector = (s_selectedVtxId >= 0) ? s_selectedVtxSector : s_hoveredVtxSector;
		s32 index = s_selectedVtxId >= 0 ? s_selectedVtxId : s_hoveredVtxId;
		if (index < 0 || !sector) { return; }

		Vec2f* vtx = sector->vtx.data() + index;

		ImGui::Text("Vertex %d of Sector %d", index, sector->id);

		ImGui::NewLine();
		ImGui::PushItemWidth(UI_SCALE(80));
		ImGui::LabelText("##PositionLabel", "Position");
		ImGui::PopItemWidth();

		ImGui::SameLine();
		ImGui::PushItemWidth(196.0f);
		ImGui::InputFloat2("##VertexPosition", &vtx->x, "%0.3f", ImGuiInputTextFlags_CharsDecimal);
		ImGui::PopItemWidth();
	}

	void infoLabel(const char* labelId, const char* labelText, u32 width)
	{
		ImGui::PushItemWidth(f32(width));
		ImGui::LabelText(labelId, labelText);
		ImGui::PopItemWidth();
		ImGui::SameLine();
	}

	void infoIntInput(const char* labelId, u32 width, s32* value)
	{
		ImGui::PushItemWidth(f32(width));
		ImGui::InputInt(labelId, value);
		ImGui::PopItemWidth();
	}

	void infoUIntInput(const char* labelId, u32 width, u32* value)
	{
		ImGui::PushItemWidth(f32(width));
		ImGui::InputUInt(labelId, value);
		ImGui::PopItemWidth();
	}

	void infoFloatInput(const char* labelId, u32 width, f32* value)
	{
		ImGui::PushItemWidth(f32(width));
		ImGui::InputFloat(labelId, value, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
	}

	void infoPanelWall()
	{
		s32 wallId;
		EditorSector* sector;
		if (s_selectedWallId >= 0) { sector = s_selectedWallSector; wallId = s_selectedWallId; }
		else if (s_hoveredWallId >= 0) { sector = s_hoveredWallSector; wallId = s_hoveredWallId; }
		else { return; }

		EditorWall* wall = sector->walls.data() + wallId;
		Vec2f* vertices = sector->vtx.data();
		f32 len = TFE_Math::distance(&vertices[wall->idx[0]], &vertices[wall->idx[1]]);

		ImGui::Text("Wall ID: %d  Sector ID: %d  Length: %2.2f", wallId, sector->id, len);
		ImGui::Text("Vertices: [%d](%2.2f, %2.2f), [%d](%2.2f, %2.2f)", wall->idx[0], vertices[wall->idx[0]].x, vertices[wall->idx[0]].z, wall->idx[1], vertices[wall->idx[1]].x, vertices[wall->idx[1]].z);
		ImGui::Separator();

		// Adjoin data (should be carefully and rarely edited directly).
		ImGui::LabelText("##SectorAdjoin", "Adjoin"); ImGui::SameLine(64.0f);
		infoIntInput("##SectorAdjoinInput", 96, &wall->adjoinId); ImGui::SameLine(180.0f);

		ImGui::LabelText("##SectorMirror", "Mirror"); ImGui::SameLine(240);
		infoIntInput("##SectorMirrorInput", 96, &wall->mirrorId);

		s32 light = wall->wallLight;
		ImGui::LabelText("##SectorLight", "Light Adjustment"); ImGui::SameLine(148.0f);
		infoIntInput("##SectorLightInput", 96, &light);
		wall->wallLight = (s16)light;

		ImGui::Separator();

		// Flags
		infoLabel("##Flags1Label", "Flags1", 48);
		infoUIntInput("##Flags1", 128, &wall->flags[0]);
		ImGui::SameLine();

		infoLabel("##Flags3Label", "Flags3", 48);
		infoUIntInput("##Flags3", 128, &wall->flags[2]);

		const f32 column[] = { 0.0f, 174.0f, 324.0f };

		ImGui::CheckboxFlags("Mask Wall##WallFlag", &wall->flags[0], WF1_ADJ_MID_TEX); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Illum Sign##WallFlag", &wall->flags[0], WF1_ILLUM_SIGN); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Horz Flip Tex##SectorFlag", &wall->flags[0], WF1_FLIP_HORIZ);

		ImGui::CheckboxFlags("Change WallLight##WallFlag", &wall->flags[0], WF1_CHANGE_WALL_LIGHT); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Tex Anchored##WallFlag", &wall->flags[0], WF1_TEX_ANCHORED); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Wall Morphs##SectorFlag", &wall->flags[0], WF1_WALL_MORPHS);

		ImGui::CheckboxFlags("Scroll Top Tex##WallFlag", &wall->flags[0], WF1_SCROLL_TOP_TEX); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Scroll Mid Tex##WallFlag", &wall->flags[0], WF1_SCROLL_MID_TEX); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Scroll Bottom##SectorFlag", &wall->flags[0], WF1_SCROLL_BOT_TEX);

		ImGui::CheckboxFlags("Scroll Sign##WallFlag", &wall->flags[0], WF1_SCROLL_SIGN_TEX); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Hide On Map##WallFlag", &wall->flags[0], WF1_HIDE_ON_MAP); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Show On Map##SectorFlag", &wall->flags[0], WF1_SHOW_NORMAL_ON_MAP);

		ImGui::CheckboxFlags("Sign Anchored##WallFlag", &wall->flags[0], WF1_SIGN_ANCHORED); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Damage Wall##WallFlag", &wall->flags[0], WF1_DAMAGE_WALL); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Show As Ledge##SectorFlag", &wall->flags[0], WF1_SHOW_AS_LEDGE_ON_MAP);

		ImGui::CheckboxFlags("Show As Door##WallFlag", &wall->flags[0], WF1_SHOW_AS_DOOR_ON_MAP); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Always Walk##WallFlag", &wall->flags[2], WF3_ALWAYS_WALK); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Solid Wall##SectorFlag", &wall->flags[2], WF3_SOLID_WALL);

		ImGui::CheckboxFlags("Player Walk Only##WallFlag", &wall->flags[2], WF3_PLAYER_WALK_ONLY); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Cannot Shoot Thru##WallFlag", &wall->flags[2], WF3_CANNOT_FIRE_THROUGH);

		ImGui::Separator();

		EditorTexture* midTex = (EditorTexture*)getAssetData(wall->tex[WP_MID].handle);
		EditorTexture* topTex = (EditorTexture*)getAssetData(wall->tex[WP_TOP].handle);
		EditorTexture* botTex = (EditorTexture*)getAssetData(wall->tex[WP_BOT].handle);
		EditorTexture* sgnTex = (EditorTexture*)getAssetData(wall->tex[WP_SIGN].handle);

		const f32 texCol = 150.0f;
		// Labels
		ImGui::Text("Mid Texture"); ImGui::SameLine(texCol);
		ImGui::Text("Sign Texture");

		// Textures - Mid / Sign
		const f32 midScale = midTex ? 1.0f / (f32)std::max(midTex->width, midTex->height) : 0.0f;
		const f32 sgnScale = sgnTex ? 1.0f / (f32)std::max(sgnTex->width, sgnTex->height) : 0.0f;
		const f32 aspectMid[] = { midTex ? f32(midTex->width) * midScale : 1.0f, midTex ? f32(midTex->height) * midScale : 1.0f };
		const f32 aspectSgn[] = { sgnTex ? f32(sgnTex->width) * sgnScale : 1.0f, sgnTex ? f32(sgnTex->height) * sgnScale : 1.0f };

		ImGui::ImageButton(midTex ? TFE_RenderBackend::getGpuPtr(midTex->frames[0]) : nullptr, { 128.0f * aspectMid[0], 128.0f * aspectMid[1] });
		ImGui::SameLine(texCol);
		ImGui::ImageButton(sgnTex ? TFE_RenderBackend::getGpuPtr(sgnTex->frames[0]) : nullptr, { 128.0f * aspectSgn[0], 128.0f * aspectSgn[1] });
		const ImVec2 imageLeft0 = ImGui::GetItemRectMin();
		const ImVec2 imageRight0 = ImGui::GetItemRectMax();

		// Names
		ImGui::Text("%s", midTex ? midTex->name : "NONE"); ImGui::SameLine(texCol);
		ImGui::Text("%s", sgnTex ? sgnTex->name : "NONE");

		ImVec2 imageLeft1, imageRight1;
		if (wall->adjoinId >= 0)
		{
			ImGui::NewLine();

			// Textures - Top / Bottom
			// Labels
			ImGui::Text("Top Texture"); ImGui::SameLine(texCol); ImGui::Text("Bottom Texture");

			const f32 topScale = topTex ? 1.0f / (f32)std::max(topTex->width, topTex->height) : 0.0f;
			const f32 botScale = botTex ? 1.0f / (f32)std::max(botTex->width, botTex->height) : 0.0f;
			const f32 aspectTop[] = { topTex ? f32(topTex->width) * topScale : 1.0f, topTex ? f32(topTex->height) * topScale : 1.0f };
			const f32 aspectBot[] = { botTex ? f32(botTex->width) * botScale : 1.0f, botTex ? f32(botTex->height) * botScale : 1.0f };

			ImGui::ImageButton(topTex ? TFE_RenderBackend::getGpuPtr(topTex->frames[0]) : nullptr, { 128.0f * aspectTop[0], 128.0f * aspectTop[1] });
			ImGui::SameLine(texCol);
			ImGui::ImageButton(botTex ? TFE_RenderBackend::getGpuPtr(botTex->frames[0]) : nullptr, { 128.0f * aspectBot[0], 128.0f * aspectBot[1] });
			imageLeft1 = ImGui::GetItemRectMin();
			imageRight1 = ImGui::GetItemRectMax();

			// Names
			ImGui::Text("%s", topTex ? topTex->name : "NONE"); ImGui::SameLine(texCol);
			ImGui::Text("%s", botTex ? botTex->name : "NONE");
		}

		// Texture Offsets
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		// Set 0
		ImVec2 offsetLeft = { imageLeft0.x + texCol + 8.0f, imageLeft0.y + 8.0f };
		ImVec2 offsetSize = { displayInfo.width - (imageLeft0.x + texCol + 16.0f), 128.0f };
		ImGui::SetNextWindowPos(offsetLeft);
		// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
		ImGui::BeginChild("##TextureOffsets0Wall", offsetSize);
		{
			ImGui::Text("Mid Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##MidOffsetInput", &wall->tex[WP_MID].offset.x, "%.2f");
			ImGui::PopItemWidth();

			ImGui::NewLine();

			ImGui::Text("Sign Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##SignOffsetInput", &wall->tex[WP_SIGN].offset.x, "%.2f");
			ImGui::PopItemWidth();
		}
		ImGui::EndChild();

		// Set 1
		if (wall->adjoinId >= 0)
		{
			offsetLeft = { imageLeft1.x + texCol + 8.0f, imageLeft1.y + 8.0f };
			offsetSize = { displayInfo.width - (imageLeft1.x + texCol + 16.0f), 128.0f };
			ImGui::SetNextWindowPos(offsetLeft);
			// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
			ImGui::BeginChild("##TextureOffsets1Wall", offsetSize);
			{
				ImGui::Text("Top Offset");
				ImGui::PushItemWidth(128.0f);
				ImGui::InputFloat2("##TopOffsetInput", &wall->tex[WP_TOP].offset.x, "%.2f");
				ImGui::PopItemWidth();

				ImGui::NewLine();

				ImGui::Text("Bottom Offset");
				ImGui::PushItemWidth(128.0f);
				ImGui::InputFloat2("##BotOffsetInput", &wall->tex[WP_BOT].offset.x, "%.2f");
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();
		}
	}

	void infoPanelSector()
	{
		EditorSector* sector = s_selectedSector ? s_selectedSector : s_hoveredSector;
		ImGui::Text("Sector ID: %d      Wall Count: %u", sector->id, (u32)sector->walls.size());
		ImGui::Separator();

		// Sector Name (optional, used by INF)
		char sectorName[64];
		strcpy(sectorName, sector->name.c_str());

		infoLabel("##NameLabel", "Name", 32);
		ImGui::PushItemWidth(240.0f);
		if (ImGui::InputText("##NameSector", sectorName, 64))
		{
			sector->name = sectorName;
		}
		ImGui::PopItemWidth();

		// Layer and Ambient
		s32 layer = sector->layer;
		infoLabel("##LayerLabel", "Layer", 42);
		infoIntInput("##LayerSector", 96, &layer);
		if (layer != sector->layer)
		{
			sector->layer = layer;
			// Adjust layer range.
			s_level.layerRange[0] = std::min(s_level.layerRange[0], (s32)layer);
			s_level.layerRange[1] = std::max(s_level.layerRange[1], (s32)layer);
			// Change the current layer so we can still see the sector.
			s_curLayer = layer;
		}

		ImGui::SameLine(0.0f, 16.0f);

		s32 ambient = (s32)sector->ambient;
		infoLabel("##AmbientLabel", "Ambient", 56);
		infoIntInput("##AmbientSector", 96, &ambient);
		sector->ambient = std::max(0, std::min(31, ambient));

		// Heights
		infoLabel("##FloorHeightLabel", "Floor Ht", 66);
		infoFloatInput("##FloorHeight", 64, &sector->floorHeight);
		ImGui::SameLine();

		infoLabel("##SecondHeightLabel", "Second Ht", 72);
		infoFloatInput("##SecondHeight", 64, &sector->secHeight);
		ImGui::SameLine();

		infoLabel("##CeilHeightLabel", "Ceiling Ht", 80);
		infoFloatInput("##CeilHeight", 64, &sector->ceilHeight);

		ImGui::Separator();

		// Flags
		infoLabel("##Flags1Label", "Flags1", 48);
		infoUIntInput("##Flags1", 128, &sector->flags[0]);
		ImGui::SameLine();

		infoLabel("##Flags2Label", "Flags2", 48);
		infoUIntInput("##Flags2", 128, &sector->flags[1]);

		infoLabel("##Flags3Label", "Flags3", 48);
		infoUIntInput("##Flags3", 128, &sector->flags[2]);

		const f32 column[] = { 0.0f, 160.0f, 320.0f };

		ImGui::CheckboxFlags("Exterior##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXTERIOR); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Pit##SectorFlag", &sector->flags[0], SEC_FLAGS1_PIT); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("No Walls##SectorFlag", &sector->flags[0], SEC_FLAGS1_NOWALL_DRAW);

		ImGui::CheckboxFlags("Ext Ceil Adj##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXT_ADJ); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Ext Floor Adj##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXT_FLOOR_ADJ);  ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Secret##SectorFlag", &sector->flags[0], SEC_FLAGS1_SECRET);

		ImGui::CheckboxFlags("Door##SectorFlag", &sector->flags[0], SEC_FLAGS1_DOOR); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Ice Floor##SectorFlag", &sector->flags[0], SEC_FLAGS1_ICE_FLOOR); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Snow Floor##SectorFlag", &sector->flags[0], SEC_FLAGS1_SNOW_FLOOR);

		ImGui::CheckboxFlags("Crushing##SectorFlag", &sector->flags[0], SEC_FLAGS1_CRUSHING); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Low Damage##SectorFlag", &sector->flags[0], SEC_FLAGS1_LOW_DAMAGE); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("High Damage##SectorFlag", &sector->flags[0], SEC_FLAGS1_HIGH_DAMAGE);

		ImGui::CheckboxFlags("No Smart Obj##SectorFlag", &sector->flags[0], SEC_FLAGS1_NO_SMART_OBJ); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Smart Obj##SectorFlag", &sector->flags[0], SEC_FLAGS1_SMART_OBJ); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Safe Sector##SectorFlag", &sector->flags[0], SEC_FLAGS1_SAFESECTOR);

		ImGui::CheckboxFlags("Mag Seal##SectorFlag", &sector->flags[0], SEC_FLAGS1_MAG_SEAL); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Exploding Wall##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXP_WALL);

		ImGui::Separator();

		// Textures
		EditorTexture* floorTex = (EditorTexture*)getAssetData(sector->floorTex.handle);
		EditorTexture* ceilTex  = (EditorTexture*)getAssetData(sector->ceilTex.handle);

		void* floorPtr = floorTex ? TFE_RenderBackend::getGpuPtr(floorTex->frames[0]) : nullptr;
		void* ceilPtr = ceilTex ? TFE_RenderBackend::getGpuPtr(ceilTex->frames[0]) : nullptr;

		const f32 texCol = 150.0f;
		// Labels
		ImGui::Text("Floor Texture"); ImGui::SameLine(texCol);
		ImGui::Text("Ceiling Texture");

		// Textures
		const f32 floorScale = floorTex ? 1.0f / (f32)std::max(floorTex->width, floorTex->height) : 1.0f;
		const f32 ceilScale = ceilTex ? 1.0f / (f32)std::max(ceilTex->width, ceilTex->height) : 1.0f;
		const f32 aspectFloor[] = { floorTex ? f32(floorTex->width) * floorScale : 1.0f, floorTex ? f32(floorTex->height) * floorScale : 1.0f };
		const f32 aspectCeil[] = { ceilTex ? f32(ceilTex->width) * ceilScale : 1.0f, ceilTex ? f32(ceilTex->height) * ceilScale : 1.0f };

		ImGui::ImageButton(floorPtr, { 128.0f * aspectFloor[0], 128.0f * aspectFloor[1] }, { 0.0f, 1.0f }, { 1.0f, 0.0f });
		ImGui::SameLine(texCol);
		ImGui::ImageButton(ceilPtr, { 128.0f * aspectCeil[0], 128.0f * aspectCeil[1] }, { 0.0f, 1.0f }, { 1.0f, 0.0f });
		const ImVec2 imageLeft = ImGui::GetItemRectMin();
		const ImVec2 imageRight = ImGui::GetItemRectMax();

		// Names
		if (floorTex)
		{
			ImGui::Text("%s", floorTex->name); ImGui::SameLine(texCol);
		}
		if (ceilTex)
		{
			ImGui::Text("%s", ceilTex->name); ImGui::SameLine();
		}

		// Texture Offsets
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImVec2 offsetLeft = { imageLeft.x + texCol + 8.0f, imageLeft.y + 8.0f };
		ImVec2 offsetSize = { displayInfo.width - (imageLeft.x + texCol + 16.0f), 128.0f };
		ImGui::SetNextWindowPos(offsetLeft);
		// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
		ImGui::BeginChild("##TextureOffsetsSector", offsetSize);
		{
			ImGui::Text("Floor Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##FloorOffsetInput", &sector->floorTex.offset.x, "%.2f");
			ImGui::PopItemWidth();

			ImGui::NewLine();

			ImGui::Text("Ceil Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##CeilOffsetInput", &sector->ceilTex.offset.x, "%.2f");
			ImGui::PopItemWidth();
		}
		ImGui::EndChild();
	}
		
	void drawInfoPanel()
	{
		// Draw the info bars.
		s_infoHeight = 486 + 44;

		infoToolBegin(s_infoHeight);
		{
			if (s_editMode == LEDIT_VERTEX && (s_hoveredVtxId >= 0 || s_selectedVtxId >= 0))
			{
				infoPanelVertex();
			}
			else if (s_editMode == LEDIT_SECTOR && (s_hoveredSector || s_selectedSector))
			{
				infoPanelSector();
			}
			else if (s_editMode == LEDIT_WALL && (s_hoveredWallId >= 0 || s_selectedWallId >= 0))
			{
				infoPanelWall();
			}
			// TODO
			//else if (s_hoveredEntity >= 0 || s_selectedEntity >= 0)
			//{
			//	infoPanelEntity();
			//}
			else
			{
				infoPanelMap();
			}
		}
		infoToolEnd();
	}
		
	void infoToolEnd()
	{
		ImGui::End();
	}
}
