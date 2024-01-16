#include "levelEditor.h"
#include "levelEditorData.h"
#include "levelEditorHistory.h"
#include "infoPanel.h"
#include "sharedState.h"
#include "selection.h"
#include <TFE_Input/input.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_System/math.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>

using namespace TFE_Editor;

namespace LevelEditor
{
	enum InfoTab
	{
		TAB_INFO = 0,
		TAB_ITEM,
		TAB_GROUPS,
		TAB_COUNT
	};
	const char* c_infoTabs[TAB_COUNT] = { "Info", "Item", "Groups" };
	const char* c_infoToolTips[TAB_COUNT] =
	{
		"Level Info.\nManual grid height setting.\nMessage, warning, and error output.",
		"Hovered or selected item property editor,\nblank when no item (sector, wall, object) is selected.",
		"Editor groups, used to group sectors - \nwhich can be used to hide or lock them, \ncolor code them or set whether they are exported.",
	};

	struct LeMessage
	{
		LeMsgType type;
		std::string msg;
	};
	static std::vector<LeMessage> s_outputMsg;
	static u32 s_outputFilter = LFILTER_DEFAULT;
	static f32 s_infoWith;
	static s32 s_infoHeight;
	static Vec2f s_infoPos;
	static InfoTab s_infoTab = TAB_INFO;

	static Feature s_prevVertexFeature = {};
	static Feature s_prevWallFeature = {};
	static Feature s_prevSectorFeature = {};
	static Feature s_prevObjectFeature = {};
	static bool s_wallShownLast = false;

	void infoPanelClearMessages()
	{
		s_outputMsg.clear();
	}

	void infoPanelAddMsg(LeMsgType type, const char* msg, ...)
	{
		char fullStr[TFE_MAX_PATH * 2];
		va_list arg;
		va_start(arg, msg);
		vsprintf(fullStr, msg, arg);
		va_end(arg);

		s_outputMsg.push_back({ type, fullStr });
	}

	void infoPanelSetMsgFilter(u32 filter/*LFILTER_DEFAULT*/)
	{
		s_outputFilter = filter;
	}

	void infoPanelClearFeatures()
	{
		s_prevVertexFeature = {};
		s_prevWallFeature = {};
		s_prevSectorFeature = {};
		s_prevObjectFeature = {};
		s_wallShownLast = false;
	}

	s32 infoPanelGetHeight()
	{
		return s_infoHeight;
	}

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

	s32 handleTabs(s32 curTab, s32 offsetX, s32 offsetY, s32 count, const char** names, const char** tooltips)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 4.0f, 1.0f });
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (f32)offsetX);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (f32)offsetY);

		for (s32 i = 0; i < count; i++)
		{
			setTabColor(curTab == i);
			if (ImGui::Button(names[i], { 100.0f, 18.0f }))
			{
				curTab = i;
				commitCurEntityChanges();
			}
			setTooltip("%s", tooltips[i]);
			restoreTabColor();
			if (i < count - 1)
			{
				ImGui::SameLine(0.0f, 2.0f);
			}
		}
		ImGui::PopStyleVar(1);

		return curTab;
	}

	void infoToolBegin(s32 height)
	{
		bool infoTool = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		s_infoWith = 480.0f;
		s_infoPos = { (f32)displayInfo.width - 480.0f, 22.0f };

		ImGui::SetWindowPos("Info Panel", { s_infoPos.x, s_infoPos.z });
		ImGui::SetWindowSize("Info Panel", { s_infoWith, f32(height) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar;

		ImGui::Begin("Info Panel", &infoTool, window_flags); ImGui::SameLine();
		s_infoTab = (InfoTab)handleTabs(s_infoTab, -12, -4, TAB_COUNT, c_infoTabs, c_infoToolTips);

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4);
		ImGui::Separator();
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
		ImGui::LabelText("##GridLabel", "Grid Height");
		ImGui::SameLine(128.0f);
		ImGui::SetNextItemWidth(196.0f);
		ImGui::InputFloat("##GridHeight", &s_gridHeight, 0.0f, 0.0f, "%0.2f", ImGuiInputTextFlags_CharsDecimal);
		ImGui::Separator();

		// Display messages here?
		ImGui::CheckboxFlags("Info", &s_outputFilter, LFILTER_INFO); ImGui::SameLine(0.0f, 32.0f);
		ImGui::CheckboxFlags("Warnings", &s_outputFilter, LFILTER_WARNING); ImGui::SameLine(0.0f, 32.0f);
		ImGui::CheckboxFlags("Errors", &s_outputFilter, LFILTER_ERROR);

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing;

		ImVec2 pos = ImGui::GetCursorPos();
		ImGui::SetNextWindowSize({ s_infoWith, s_infoHeight - pos.y });
		ImGui::SetNextWindowPos({ s_infoPos.x, pos.y + s_infoPos.z });
		ImGui::Begin("Output##MapInfo", nullptr, window_flags);
		const size_t count = s_outputMsg.size();
		const LeMessage* msg = s_outputMsg.data();
		const ImVec4 c_typeColor[] = { {1.0f, 1.0f, 1.0f, 0.7f}, {1.0f, 1.0f, 0.25f, 1.0f}, {1.0f, 0.25f, 0.25f, 1.0f} };

		for (size_t i = 0; i < count; i++, msg++)
		{
			u32 typeFlag = 1 << msg->type;
			if (!(typeFlag & s_outputFilter)) { continue; }

			ImGui::TextColored(c_typeColor[msg->type], "%s", msg->msg.c_str());
		}
		ImGui::End();
	}

	// Manually created Vec2 control, so that the value is only updated when an input loses focus
	// or Return is pressed. This avoids vertices or other elements jittering as values are added, and
	// avoids many useless undo commands being created.
	// Return true if the value should be updated.
	bool inputVec2f(const char* id, Vec2f* value)
	{
		char id0[256], id1[256];
		sprintf(id0, "%sX", id);
		sprintf(id1, "%sZ", id);

		bool hadFocusX = false, hadFocusZ = false;
		Vec2f prevPos = *value;
		ImGui::PushItemWidth(98.0f);
		if (ImGui::InputFloat(id0, &value->x, NULL, NULL, "%0.6f", ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_NoUndoRedo))
		{
			if (value->x != prevPos.x)
			{
				hadFocusX = true;
			}
		}
		bool lostFocusX = !ImGui::IsItemFocused() && !ImGui::IsItemHovered() && !ImGui::IsItemActive();

		ImGui::SameLine();
		ImGui::PushItemWidth(98.0f);
		if (ImGui::InputFloat(id1, &value->z, NULL, NULL, "%0.6f", ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_NoUndoRedo))
		{
			if (value->z != prevPos.z)
			{
				hadFocusZ = true;
			}
		}
		bool lostFocusZ = !ImGui::IsItemFocused() && !ImGui::IsItemHovered() && !ImGui::IsItemActive();

		// Only update the value and add the edit when the input loses focus (clicking away or on another control) or
		// Enter/Return is pressed.
		bool retOrTabPressed = TFE_Input::keyPressed(KEY_RETURN) || TFE_Input::keyPressed(KEY_TAB);
		return ((lostFocusX || retOrTabPressed) && hadFocusX) ||
			((lostFocusZ || retOrTabPressed) && hadFocusZ);
	}

	void getPrevVertexFeature(EditorSector*& sector, s32& index)
	{
		// Make sure the previous selection is still valid.
		bool selectionInvalid = !s_prevVertexFeature.sector || s_prevVertexFeature.featureIndex < 0;
		if (s_prevVertexFeature.sector && s_prevVertexFeature.featureIndex >= (s32)s_prevVertexFeature.sector->vtx.size())
		{
			selectionInvalid = true;
		}

		if (selectionInvalid)
		{
			s_prevVertexFeature.sector = s_level.sectors.empty() ? nullptr : &s_level.sectors[0];
			if (s_prevVertexFeature.sector)
			{
				s_prevVertexFeature.featureIndex = 0;
			}
		}
		sector = s_prevVertexFeature.sector;
		index = s_prevVertexFeature.featureIndex;
	}

	void setPrevVertexFeature(EditorSector* sector, s32 index)
	{
		s_prevVertexFeature.sector = sector;
		s_prevVertexFeature.featureIndex = index;
	}

	void infoPanelVertex()
	{
		s32 index = -1;
		EditorSector* sector = nullptr;
		if (s_featureCur.featureIndex < 0 && s_featureHovered.featureIndex < 0 && !s_selectionList.empty())
		{
			sector = unpackFeatureId(s_selectionList[0], &index);
		}
		else
		{
			sector = (s_featureCur.featureIndex >= 0) ? s_featureCur.sector : s_featureHovered.sector;
			index = s_featureCur.featureIndex >= 0 ? s_featureCur.featureIndex : s_featureHovered.featureIndex;
		}

		// If there is no selected vertex, just show vertex 0...
		if (index < 0 || !sector)
		{
			getPrevVertexFeature(sector, index);
		}
		if (index < 0 || !sector) { return; }
		setPrevVertexFeature(sector, index);

		Vec2f* vtx = sector->vtx.data() + index;
		if (s_selectionList.size() > 1)
		{
			ImGui::Text("Multiple vertices selected, enter a movement delta.");
		}
		else
		{
			ImGui::Text("Vertex %d of Sector %d", index, sector->id);
		}

		ImGui::NewLine();
		ImGui::PushItemWidth(UI_SCALE(80));
		ImGui::LabelText("##PositionLabel", "Position");
		ImGui::PopItemWidth();

		ImGui::SameLine();
		Vec2f curPos = { 0 };
		if (s_selectionList.size() <= 1)
		{
			curPos = *vtx;
		}

		Vec2f prevPos = curPos;
		if (inputVec2f("##VertexPosition", &curPos))
		{
			if (s_selectionList.size() <= 1) // Move the single vertex.
			{
				const FeatureId id = createFeatureId(sector, index, 0, false);
				cmd_addSetVertex(id, curPos);
				edit_setVertexPos(id, curPos);
			}
			else  // Move the whole group.
			{
				const Vec2f delta = { curPos.x - prevPos.x, curPos.z - prevPos.z };
				cmd_addMoveVertices((s32)s_selectionList.size(), s_selectionList.data(), delta);
				edit_moveVertices((s32)s_selectionList.size(), s_selectionList.data(), delta);
			}
		}
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

	void getPrevWallFeature(EditorSector*& sector, s32& index)
	{
		// Make sure the previous selection is still valid.
		bool selectionInvalid = !s_prevWallFeature.sector || s_prevWallFeature.featureIndex < 0;
		if (s_prevWallFeature.sector && s_prevWallFeature.featureIndex >= (s32)s_prevWallFeature.sector->walls.size())
		{
			selectionInvalid = true;
		}

		if (selectionInvalid)
		{
			s_prevWallFeature.sector = s_level.sectors.empty() ? nullptr : &s_level.sectors[0];
			if (s_prevWallFeature.sector)
			{
				s_prevWallFeature.featureIndex = 0;
			}
		}
		sector = s_prevWallFeature.sector;
		index = s_prevWallFeature.featureIndex;
	}

	void setPrevWallFeature(EditorSector* sector, s32 index)
	{
		s_prevWallFeature.sector = sector;
		s_prevWallFeature.featureIndex = index;
	}

	void infoPanelWall()
	{
		s32 wallId;
		EditorSector* sector;
		if (s_featureCur.featureIndex >= 0 && s_featureCur.sector) { sector = s_featureCur.sector; wallId = s_featureCur.featureIndex; }
		else if (s_featureHovered.featureIndex >= 0 && s_featureHovered.sector) { sector = s_featureHovered.sector; wallId = s_featureHovered.featureIndex; }
		else { getPrevWallFeature(sector, wallId); }
		if (!sector || wallId < 0) { return; }
		setPrevWallFeature(sector, wallId);
		s_wallShownLast = true;

		bool insertTexture = s_selectedTexture >= 0 && TFE_Input::keyPressed(KEY_T);
		bool removeTexture = TFE_Input::mousePressed(MBUTTON_RIGHT);
		s32 texIndex = -1;
		if (insertTexture)
		{
			texIndex = s_selectedTexture;
		}

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
		wall->wallLight = light;

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

		EditorTexture* midTex = getTexture(wall->tex[WP_MID].texIndex);
		EditorTexture* topTex = getTexture(wall->tex[WP_TOP].texIndex);
		EditorTexture* botTex = getTexture(wall->tex[WP_BOT].texIndex);
		EditorTexture* sgnTex = getTexture(wall->tex[WP_SIGN].texIndex);

		const f32 texCol = 150.0f;
		// Labels
		ImGui::Text("Mid Texture"); ImGui::SameLine(texCol);
		ImGui::Text("Sign Texture");

		// Textures - Mid / Sign
		const f32 midScale = midTex ? 1.0f / (f32)std::max(midTex->width, midTex->height) : 0.0f;
		const f32 sgnScale = sgnTex ? 1.0f / (f32)std::max(sgnTex->width, sgnTex->height) : 0.0f;
		const f32 aspectMid[] = { midTex ? f32(midTex->width) * midScale : 1.0f, midTex ? f32(midTex->height) * midScale : 1.0f };
		const f32 aspectSgn[] = { sgnTex ? f32(sgnTex->width) * sgnScale : 1.0f, sgnTex ? f32(sgnTex->height) * sgnScale : 1.0f };

		ImGui::ImageButton(midTex ? TFE_RenderBackend::getGpuPtr(midTex->frames[0]) : nullptr, { 128.0f * aspectMid[0], 128.0f * aspectMid[1] }, { 0.0f, 1.0f }, { 1.0f, 0.0f });
		if (texIndex >= 0 && ImGui::IsItemHovered())
		{
			FeatureId id = createFeatureId(sector, wallId, HP_MID);
			edit_setTexture(1, &id, texIndex);
			cmd_addSetTexture(1, &id, texIndex, nullptr);
		}

		ImGui::SameLine(texCol);
		ImGui::ImageButton(sgnTex ? TFE_RenderBackend::getGpuPtr(sgnTex->frames[0]) : nullptr, { 128.0f * aspectSgn[0], 128.0f * aspectSgn[1] }, { 0.0f, 1.0f }, { 1.0f, 0.0f });
		if (texIndex >= 0 && ImGui::IsItemHovered())
		{
			FeatureId id = createFeatureId(sector, wallId, HP_SIGN);
			edit_setTexture(1, &id, texIndex);
			cmd_addSetTexture(1, &id, texIndex, nullptr);
		}
		else if (removeTexture && ImGui::IsItemHovered())
		{
			FeatureId id = createFeatureId(sector, wallId, HP_SIGN);
			edit_clearTexture(1, &id);
			cmd_addClearTexture(1, &id);
		}

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

			ImGui::ImageButton(topTex ? TFE_RenderBackend::getGpuPtr(topTex->frames[0]) : nullptr, { 128.0f * aspectTop[0], 128.0f * aspectTop[1] }, { 0.0f, 1.0f }, { 1.0f, 0.0f });
			if (texIndex >= 0 && ImGui::IsItemHovered())
			{
				FeatureId id = createFeatureId(sector, wallId, HP_TOP);
				edit_setTexture(1, &id, texIndex);
				cmd_addSetTexture(1, &id, texIndex, nullptr);
			}

			ImGui::SameLine(texCol);
			ImGui::ImageButton(botTex ? TFE_RenderBackend::getGpuPtr(botTex->frames[0]) : nullptr, { 128.0f * aspectBot[0], 128.0f * aspectBot[1] }, { 0.0f, 1.0f }, { 1.0f, 0.0f });
			if (texIndex >= 0 && ImGui::IsItemHovered())
			{
				FeatureId id = createFeatureId(sector, wallId, HP_BOT);
				edit_setTexture(1, &id, texIndex);
				cmd_addSetTexture(1, &id, texIndex, nullptr);
			}

			imageLeft1 = ImGui::GetItemRectMin();
			imageRight1 = ImGui::GetItemRectMax();

			// Names
			ImGui::Text("%s", topTex ? topTex->name : "NONE"); ImGui::SameLine(texCol);
			ImGui::Text("%s", botTex ? botTex->name : "NONE");
		}

		// Sign buttons.
		ImVec2 signLeft = { imageLeft0.x + texCol - 8.0f, imageLeft0.y + 2.0f };
		ImGui::SetNextWindowPos(signLeft);
		ImGui::BeginChild("##SignOptions", { 32, 48 });
		{
			if (ImGui::Button("X") && wall->tex[HP_SIGN].texIndex >= 0)
			{
				FeatureId id = createFeatureId(sector, wallId, HP_SIGN);
				edit_clearTexture(1, &id);
				cmd_addClearTexture(1, &id);
			}
			if (ImGui::Button("C") && wall->tex[HP_SIGN].texIndex >= 0)
			{
				Vec2f prevOffset = wall->tex[HP_SIGN].offset;
				centerSignOnSurface(sector, wall);
				Vec2f delta = { wall->tex[HP_SIGN].offset.x - prevOffset.x, wall->tex[HP_SIGN].offset.z - prevOffset.z };

				if (delta.x || delta.z)
				{
					FeatureId id = createFeatureId(sector, wallId, HP_SIGN);
					cmd_addMoveTexture(1, &id, delta);
				}
			}
		}
		ImGui::EndChild();

		// Texture Offsets
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		// Set 0
		ImVec2 offsetLeft = { imageLeft0.x + texCol + 8.0f + 16.0f, imageLeft0.y + 8.0f };
		ImVec2 offsetSize = { displayInfo.width - (imageLeft0.x + texCol + 16.0f - 32.0f), 128.0f };
		ImGui::SetNextWindowPos(offsetLeft);
		// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
		ImGui::BeginChild("##TextureOffsets0Wall", offsetSize);
		{
			ImGui::Text("Mid Offset");
			ImGui::PushItemWidth(136.0f);
			ImGui::InputFloat2("##MidOffsetInput", &wall->tex[WP_MID].offset.x, "%.3f");
			ImGui::PopItemWidth();

			ImGui::NewLine();

			ImGui::Text("Sign Offset");
			ImGui::PushItemWidth(136.0f);
			ImGui::InputFloat2("##SignOffsetInput", &wall->tex[WP_SIGN].offset.x, "%.3f");
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
				ImGui::PushItemWidth(136.0f);
				ImGui::InputFloat2("##TopOffsetInput", &wall->tex[WP_TOP].offset.x, "%.3f");
				ImGui::PopItemWidth();

				ImGui::NewLine();

				ImGui::Text("Bottom Offset");
				ImGui::PushItemWidth(136.0f);
				ImGui::InputFloat2("##BotOffsetInput", &wall->tex[WP_BOT].offset.x, "%.3f");
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();
		}
	}

	void getPrevSectorFeature(EditorSector*& sector)
	{
		// Make sure the previous selection is still valid.
		bool selectionInvalid = !s_prevSectorFeature.sector;
		if (selectionInvalid)
		{
			s_prevSectorFeature.sector = s_level.sectors.empty() ? nullptr : &s_level.sectors[0];
		}
		sector = s_prevSectorFeature.sector;
	}

	void setPrevSectorFeature(EditorSector* sector)
	{
		s_prevSectorFeature.sector = sector;
	}

	void infoPanelSector()
	{
		bool insertTexture = s_selectedTexture >= 0 && TFE_Input::keyPressed(KEY_T);
		s32 texIndex = -1;
		if (insertTexture)
		{
			texIndex = s_selectedTexture;
		}

		EditorSector* sector = s_featureCur.sector ? s_featureCur.sector : s_featureHovered.sector;
		if (!sector) { getPrevSectorFeature(sector); }
		if (!sector) { return; }
		setPrevSectorFeature(sector);
		s_wallShownLast = false;

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
		EditorTexture* floorTex = getTexture(sector->floorTex.texIndex);
		EditorTexture* ceilTex = getTexture(sector->ceilTex.texIndex);

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
		if (texIndex >= 0 && ImGui::IsItemHovered())
		{
			FeatureId id = createFeatureId(sector, 0, HP_FLOOR);
			edit_setTexture(1, &id, texIndex);
			cmd_addSetTexture(1, &id, texIndex, nullptr);
		}

		ImGui::SameLine(texCol);
		ImGui::ImageButton(ceilPtr, { 128.0f * aspectCeil[0], 128.0f * aspectCeil[1] }, { 0.0f, 1.0f }, { 1.0f, 0.0f });
		if (texIndex >= 0 && ImGui::IsItemHovered())
		{
			FeatureId id = createFeatureId(sector, 0, HP_CEIL);
			edit_setTexture(1, &id, texIndex);
			cmd_addSetTexture(1, &id, texIndex, nullptr);
		}

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
			ImGui::PushItemWidth(136.0f);
			ImGui::InputFloat2("##FloorOffsetInput", &sector->floorTex.offset.x, "%.3f");
			ImGui::PopItemWidth();

			ImGui::NewLine();

			ImGui::Text("Ceil Offset");
			ImGui::PushItemWidth(136.0f);
			ImGui::InputFloat2("##CeilOffsetInput", &sector->ceilTex.offset.x, "%.3f");
			ImGui::PopItemWidth();
		}
		ImGui::EndChild();
	}

	Entity s_objEntity = {};
	EditorObject* s_prevObj = nullptr;

	bool hasObjectSelectionChanged(EditorSector* sector, s32 index)
	{
		return s_prevObjectFeature.sector != sector || s_prevObjectFeature.featureIndex != index;
	}

	void getPrevObjectFeature(EditorSector*& sector, s32& index)
	{
		// Make sure the previous selection is still valid.
		bool selectionInvalid = !s_prevObjectFeature.sector || s_prevObjectFeature.featureIndex < 0;
		if (s_prevObjectFeature.sector && s_prevObjectFeature.featureIndex >= (s32)s_prevObjectFeature.sector->obj.size())
		{
			selectionInvalid = true;
			s_objEntity = {};
		}

		if (selectionInvalid)
		{
			s_prevObjectFeature.sector = s_level.sectors.empty() ? nullptr : &s_level.sectors[0];
			s_prevObjectFeature.featureIndex = s_prevObjectFeature.sector->obj.empty() ? -1 : 0;
		}
		sector = s_prevObjectFeature.sector;
		index = s_prevObjectFeature.featureIndex;
	}

	void setPrevObjectFeature(EditorSector* sector, s32 index)
	{
		s_prevObjectFeature.sector = sector;
		s_prevObjectFeature.featureIndex = index;
	}

	static const char* c_entityClassName[] =
	{
		"Spirit", // ETYPE_SPIRIT = 0,
		"Safe",   // ETYPE_SAFE,
		"Sprite", // ETYPE_SPRITE,
		"Frame",  // ETYPE_FRAME,
		"3D",     // ETYPE_3D,
	};

	static const char* c_objDifficulty[] =
	{
		"Easy, Medium, Hard",	//  0
		"Easy, Medium",         // -2
		"Easy",                 // -1
		"Medium, Hard",         // 2
		"Hard"                  // 3
	};
	static const s32 c_objDiffValue[] =
	{
		1, -2, -1, 2, 3
	};
	static const s32 c_objDiffStart = -3;
	static const s32 c_objDiffToIndex[] =
	{
		0, 1, 2, 0, 0, 3, 4
	};
	static s32 s_logicIndex = 0;
	static s32 s_varIndex = 0;
	static s32 s_logicSelect = -1;
	
	void commitEntityChanges(EditorSector* sector, s32 objIndex, const Entity* newEntityDef)
	{
		// 1. Has the entity data actually changed? If not, then just leave it alone.
		EditorObject* obj = &sector->obj[objIndex];
		const Entity* curEntity = &s_level.entities[obj->entityId];
		if (entityDefsEqual(newEntityDef, curEntity))
		{
			return;
		}

		// 2. Search through existing entities and see if this already exists.
		const s32 count = (s32)s_level.entities.size();
		const Entity* entity = s_level.entities.data();
		s32 entityIndex = -1;
		for (s32 e = 0; e < count; e++, entity++)
		{
			if (entityDefsEqual(newEntityDef, entity))
			{
				entityIndex = e;
				break;
			}
		}
		if (entityIndex >= 0)
		{
			obj->entityId = entityIndex;
			return;
		}

		// 3. Create a new entity.
		obj->entityId = (s32)s_level.entities.size();
		s_level.entities.push_back(*newEntityDef);
		loadSingleEntityData(&s_level.entities.back());
	}

	void clearEntityChanges()
	{
		s_prevObjectFeature.sector = nullptr;
		s_prevObjectFeature.featureIndex = -1;
		s_objEntity = {};
	}

	void commitCurEntityChanges()
	{
		if (!s_prevObjectFeature.sector || s_prevObjectFeature.featureIndex < 0 ||
			s_prevObjectFeature.featureIndex >= (s32)s_prevObjectFeature.sector->obj.size() || s_objEntity.id < 0)
		{
			s_objEntity = {};
			return;
		}

		commitEntityChanges(s_prevObjectFeature.sector, s_prevObjectFeature.featureIndex, &s_objEntity);
		s_objEntity = {};
	}

	void addVariableToList(s32 varId, s32* varList, s32& varCount)
	{
		for (s32 v = 0; v < varCount; v++)
		{
			if (varList[v] == varId) { return; }
		}
		varList[varCount++] = varId;
	}

	void addLogicVariables(const std::string& logicName, s32* varList, s32& varCount)
	{
		if (logicName.empty())
		{
			// Add common variables.
			const LogicDef* def = &s_logicDefList.back();
			const s32 commonCount = (s32)def->var.size();
			const LogicVar* var = def->var.data();
			for (s32 v = 0; v < commonCount; v++, var++)
			{
				addVariableToList(var->varId, varList, varCount);
			}
			return;
		}

		const s32 id = getLogicId(logicName.c_str());
		if (id < 0) { return; }
		const LogicDef* def = &s_logicDefList[id];
		const s32 count = (s32)def->var.size();

		// If there are no variables, use the defaults.
		if (count <= 0)
		{
			addLogicVariables("", varList, varCount);
			return;
		}

		const LogicVar* var = def->var.data();
		for (s32 v = 0; v < count; v++, var++)
		{
			addVariableToList(var->varId, varList, varCount);
		}
	}

	void addAllLogicVariables(s32* varList, s32& varCount)
	{
		// Add variables for each logic.
		s32 count = (s32)s_objEntity.logic.size();
		EntityLogic* list = s_objEntity.logic.data();
		for (s32 i = 0; i < count; i++)
		{
			addLogicVariables(list[i].name, varList, varCount);
		}
		// Finally add common variables.
		const LogicDef* def = &s_logicDefList.back();
		const s32 commonCount = (s32)def->var.size();
		const LogicVar* var = def->var.data();
		for (s32 v = 0; v < commonCount; v++, var++)
		{
			addVariableToList(var->varId, varList, varCount);
		}
	}

	s32 getStringListIndex(const std::string& sValue, const std::vector<std::string>& strList)
	{
		const char* src = sValue.c_str();
		const s32 count = (s32)strList.size();
		const std::string* list = strList.data();
		for (s32 i = 0; i < count; i++, list++)
		{
			if (strcasecmp(src, list->c_str()) == 0)
			{
				return i;
			}
		}
		return -1;
	}

	void infoPanelObject()
	{
		EditorSector* sector = s_featureCur.sector ? s_featureCur.sector : s_featureHovered.sector;
		s32 objIndex = s_featureCur.sector ? s_featureCur.featureIndex : s_featureHovered.featureIndex;
		EditorObject* obj = sector && objIndex >= 0 ? &sector->obj[objIndex] : nullptr;

		if (!sector || objIndex < 0)
		{
			getPrevObjectFeature(sector, objIndex);
			obj = sector && objIndex >= 0 ? &sector->obj[objIndex] : nullptr;
		}
		if (!sector || objIndex < 0 || !obj) { return; }

		if (hasObjectSelectionChanged(sector, objIndex))
		{
			commitCurEntityChanges();
		}
		setPrevObjectFeature(sector, objIndex);

		if (s_objEntity.id < 0)
		{
			s_objEntity = s_level.entities[obj->entityId];
		}

		ImGui::Text("Sector ID: %d, Object Index: %d", sector->id, objIndex);
		ImGui::Separator();
		// Entity Name
		ImGui::Text("%s", "Name"); ImGui::SameLine(0.0f, 17.0f);
		char name[256];
		strcpy(name, s_objEntity.name.c_str());
		ImGui::SetNextItemWidth(196.0f);
		if (ImGui::InputText("##NameInput", name, 256))
		{
			s_objEntity.name = name;
		}
		ImGui::SameLine(0.0f, 16.0f);
		// Entity Class/Type
		s32 classId = s_objEntity.type;
		ImGui::Text("%s", "Class"); ImGui::SameLine(0.0f, 8.0f);
		ImGui::SetNextItemWidth(96.0f);
		if (ImGui::Combo("##Class", (s32*)&classId, c_entityClassName, ETYPE_COUNT))
		{
			s_objEntity.type = EntityType(classId);
		}
		// Entity Asset
		ImGui::Text("%s", "Asset"); ImGui::SameLine(0.0f, 8.0f);
		char assetName[256];
		strcpy(assetName, s_objEntity.assetName.c_str());
		if (ImGui::InputText("##AssetName", assetName, 256))
		{
			s_objEntity.assetName = assetName;
		}
		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button("Browse"))
		{
			// TODO
		}
		ImGui::Separator();

		ImGui::Text("%s", "Position"); ImGui::SameLine(0.0f, 8.0f);
		ImGui::InputFloat3("##Position", &obj->pos.x);

		bool orientAdjusted = false;
		ImGui::Text("%s", "Angle"); ImGui::SameLine(0.0f, 32.0f);
		ImGui::SetNextItemWidth(206.0f);
		if (ImGui::SliderAngle("##Angle", &obj->angle, 0.0f))
		{
			orientAdjusted = true;
		}
		ImGui::SameLine(0.0f, 4.0f);
		ImGui::SetNextItemWidth(96.0f);

		f32 angle = obj->angle * 180.0f / PI;
		if (ImGui::InputFloat("##AngleEdit", &angle))
		{
			obj->angle = angle * PI / 180.0f;
			orientAdjusted = true;
		}
		
		// Show pitch and roll.
		if (s_objEntity.type == ETYPE_3D)
		{
			f32 pitch = obj->pitch * 180.0f / PI;
			f32 roll  = obj->roll  * 180.0f / PI;

			ImGui::Text("%s", "Pitch"); ImGui::SameLine(0.0f, 32.0f);
			ImGui::SetNextItemWidth(128.0f);
			if (ImGui::InputFloat("##PitchEdit", &pitch))
			{
				obj->pitch = pitch * PI / 180.0f;
				orientAdjusted = true;
			}
			ImGui::SameLine(0.0f, 32.0f);
			ImGui::Text("%s", "Roll"); ImGui::SameLine(0.0f, 8.0f);
			ImGui::SetNextItemWidth(128.0f);
			if (ImGui::InputFloat("##RollEdit", &roll))
			{
				obj->roll = roll * PI / 180.0f;
				orientAdjusted = true;
			}

			if (orientAdjusted)
			{
				compute3x3Rotation(&obj->transform, obj->angle, obj->pitch, obj->roll);
			}
		}

		// Difficulty.
		ImGui::Text("%s", "Difficulty"); ImGui::SameLine(0.0f, 8.0f);
		s32 index = c_objDiffToIndex[obj->diff - c_objDiffStart];
		ImGui::SetNextItemWidth(256.0f);
		if (ImGui::Combo("##DiffCombo", (s32*)&index, c_objDifficulty, TFE_ARRAYSIZE(c_objDifficulty)))
		{
			obj->diff = c_objDiffValue[index];
		}
		ImGui::Separator();

		const s32 listWidth = (s32)s_infoWith - 32;

		// Logics
		if (s_logicSelect >= (s32)s_objEntity.logic.size())
		{
			s_logicSelect = -1;
		}
		// Always select one of the them, if there are any to select.
		if (s_logicSelect < 0 && !s_objEntity.logic.empty())
		{
			s_logicSelect = 0;
		}

		ImGui::Text("%s", "Logics");
		ImGui::BeginChild("##LogicList", { (f32)min(listWidth, 400), 64 }, true);
		{
			s32 count = (s32)s_objEntity.logic.size();
			EntityLogic* list = s_objEntity.logic.data();
			for (s32 i = 0; i < count; i++)
			{
				char name[256];
				sprintf(name, "%s##%d", list[i].name.c_str(), i);
				bool sel = s_logicSelect == i;
				ImGui::Selectable(name, &sel);
				if (sel) { s_logicSelect = i; }
				else if (s_logicSelect == i) { s_logicSelect = -1; }
			}
		}
		ImGui::EndChild();
		if (ImGui::Button("Add"))
		{
			LogicDef* def = &s_logicDefList[s_logicIndex];
			EntityLogic newLogic = {};
			newLogic.name = def->name;
			s_objEntity.logic.push_back(newLogic);

			EntityLogic* logic = &s_objEntity.logic.back();

			// Add Variables automatically.
			const s32 varCount = (s32)def->var.size();
			const LogicVar* var = def->var.data();
			for (s32 v = 0; v < varCount; v++, var++)
			{
				const EntityVarDef* varDef = &s_varDefList[var->varId];
				if (var->type == VAR_TYPE_DEFAULT || var->type == VAR_TYPE_REQUIRED)
				{
					EntityVar newVar;
					newVar.defId = var->varId;
					newVar.value = varDef->defValue;
					logic->var.push_back(newVar);
				}
			}
		}

		ImGui::SameLine(0.0f, 8.0f);
		ImGui::SetNextItemWidth(128.0f);
		if (ImGui::BeginCombo("##LogicCombo", s_logicDefList[s_logicIndex].name.c_str()))
		{
			s32 count = (s32)s_logicDefList.size() - 1;
			for (s32 i = 0; i < count; i++)
			{
				if (ImGui::Selectable(s_logicDefList[i].name.c_str(), i == s_logicIndex))
				{
					s_logicIndex = i;
				}
				setTooltip(s_logicDefList[i].tooltip.c_str());
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine(0.0f, 16.0f);
		if (ImGui::Button("Remove") && s_logicSelect >= 0)
		{
			s32 count = (s32)s_objEntity.logic.size();
			for (s32 i = s_logicSelect; i < count - 1; i++)
			{
				s_objEntity.logic[i] = s_objEntity.logic[i + 1];
			}
			s_objEntity.logic.pop_back();
			s_logicSelect = -1;
		}

		ImGui::Separator();
		
		// Variables
		static s32 varSel = -1;

		// Select the variable list.
		std::vector<EntityVar>* varList = &s_objEntity.var;
		if (s_logicSelect >= 0 && s_logicSelect < (s32)s_objEntity.logic.size())
		{
			varList = &s_objEntity.logic[s_logicSelect].var;
		}

		const f32 varListHeight = s_objEntity.type == ETYPE_3D ? 114.0f : 140.0f;
		ImGui::Text("%s", "Variables");
		ImGui::BeginChild("##VariableList", { (f32)min(listWidth, 400), varListHeight }, true);
		{
			s32 count = (s32)varList->size();
			EntityVar* list = varList->data();
			for (s32 i = 0; i < count; i++)
			{
				EntityVarDef* def = getEntityVar(list[i].defId);

				char name[256];
				sprintf(name, "%s##%d", def->name.c_str(), i);
				bool sel = varSel == i;
				ImGui::Selectable(name, &sel, 0, { 100.0f,0.0f });
				if (sel) { varSel = i; }
				else if (varSel == i) { varSel = -1; }

				ImGui::SameLine(0.0f, 8.0f);
				switch (def->type)
				{
					case EVARTYPE_BOOL:
					{
						sprintf(name, "##VarBool%d", i);
						ImGui::Checkbox(name, &list[i].value.bValue);
					} break;
					case EVARTYPE_FLOAT:
					{
						sprintf(name, "##VarFloat%d", i);
						ImGui::InputFloat(name, &list[i].value.fValue);
					} break;
					case EVARTYPE_INT:
					{
						sprintf(name, "##VarInt%d", i);
						ImGui::InputInt(name, &list[i].value.iValue);
					} break;
					case EVARTYPE_FLAGS:
					{
					} break;
					case EVARTYPE_STRING_LIST:
					{
						const s32 index = getStringListIndex(list[i].value.sValue, def->strList);
						sprintf(name, "##VarCombo%d", i);
						if (ImGui::BeginCombo(name, list[i].value.sValue.c_str()))
						{
							const s32 listCount = (s32)def->strList.size();
							for (s32 s = 0; s < listCount; s++)
							{
								if (ImGui::Selectable(def->strList[s].c_str(), s == index))
								{
									list[i].value.sValue = def->strList[s];
								}
							}
							ImGui::EndCombo();
						}
					} break;
					case EVARTYPE_INPUT_STRING_PAIR:
					{
					} break;
				}
			}
		}
		ImGui::EndChild();

		s32 varComboList[256];
		s32 varComboCount = 0;
		if (s_logicSelect >= 0 && s_logicSelect < (s32)s_objEntity.logic.size())
		{
			addLogicVariables(s_objEntity.logic[s_logicSelect].name, varComboList, varComboCount);
		}
		else
		{
			addLogicVariables("", varComboList, varComboCount);
		}

		if (ImGui::Button("Add##Variable") && s_varIndex >= 0 && s_varIndex < varComboCount)
		{
			EntityVar var;
			var.defId = varComboList[s_varIndex];
			var.value = s_varDefList[var.defId].defValue;
			varList->push_back(var);
		}
		ImGui::SameLine(0.0f, 8.0f);
		ImGui::SetNextItemWidth(128.0f);

		if (s_varIndex < 0 || s_varIndex >= varComboCount) { s_varIndex = 0; }

		if (ImGui::BeginCombo("##VarCombo", varComboCount ? s_varDefList[varComboList[s_varIndex]].name.c_str() : ""))
		{
			for (s32 i = 0; i < varComboCount; i++)
			{
				if (ImGui::Selectable(s_varDefList[varComboList[i]].name.c_str(), i == s_varIndex))
				{
					s_varIndex = i;
				}
				// setTooltip(s_varDefList[varList[s_varIndex]].tooltip.c_str());
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine(0.0f, 16.0f);
		if (ImGui::Button("Remove##Variable") && varSel >= 0 && varSel < (s32)varList->size())
		{
			s32 count = (s32)varList->size();
			for (s32 i = varSel; i < count - 1; i++)
			{
				(*varList)[i] = (*varList)[i + 1];
			}
			varList->pop_back();
			varSel = -1;
		}
		ImGui::Separator();

		// Buttons.
		if (ImGui::Button("Add to Entity Def"))
		{
			// TODO
		}
		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button("Commit"))
		{
			commitCurEntityChanges();
		}
		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button("Reset"))
		{
			s_objEntity = {};
		}
	}
		
	void drawInfoPanel(EditorView view)
	{
		// Draw the info bars.
		s_infoHeight = 486 + 44;
		infoToolBegin(s_infoHeight);
		{
			if (s_infoTab == TAB_ITEM)
			{
				if (s_editMode == LEDIT_VERTEX)
				{
					infoPanelVertex();
				}
				else if (s_editMode == LEDIT_WALL || s_editMode == LEDIT_SECTOR)
				{
					const bool is2d = view == EDIT_VIEW_2D;
					const bool showWall = s_editMode == LEDIT_WALL && (is2d || s_wallShownLast);

					// Prioritize selected wall, selected sector, hovered wall, hovered sector.
					// Allow sector views in wall mode, but NOT wall views in sector mode.
					const bool curFeatureIsFlat = s_featureCur.part == HP_FLOOR || s_featureCur.part == HP_CEIL;
					const bool hoverFeatureIsFlat = s_featureHovered.part == HP_FLOOR || s_featureHovered.part == HP_CEIL;

					if (s_editMode == LEDIT_WALL && s_featureCur.featureIndex >= 0 && !curFeatureIsFlat) { infoPanelWall(); }
					else if (s_featureCur.sector) { infoPanelSector(); }
					else if (s_editMode == LEDIT_WALL && s_featureHovered.featureIndex >= 0 && !hoverFeatureIsFlat) { infoPanelWall(); }
					else if (s_featureHovered.sector) { infoPanelSector(); }
					else if (showWall) { infoPanelWall(); }
					else { infoPanelSector(); }
				}
				else if (s_editMode == LEDIT_ENTITY)
				{
					infoPanelObject();
				}
			}
			else if (s_infoTab == TAB_INFO)
			{
				infoPanelMap();
			}
			else if (s_infoTab == TAB_GROUPS)
			{
				// TODO
			}
		}
		infoToolEnd();
	}
		
	void infoToolEnd()
	{
		ImGui::End();
	}
}
