#include "levelEditor.h"
#include "levelEditorData.h"
#include "levelEditorHistory.h"
#include "guidelines.h"
#include "tabControl.h"
#include "camera.h"
#include "infoPanel.h"
#include "groups.h"
#include "sharedState.h"
#include "selection.h"
#include <TFE_Input/input.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
#include <TFE_Editor/LevelEditor/Rendering/grid.h>
#include <TFE_Editor/LevelEditor/Scripting/levelEditorScripts.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_System/math.h>
#include <TFE_Ui/ui.h>

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
		TAB_COUNT
	};
	const char* c_infoTabs[TAB_COUNT] = { "Info", "Item" };
	const char* c_infoToolTips[TAB_COUNT] =
	{
		"Level Info.\nManual grid height setting.\nMessage, warning, and error output.",
		"Hovered or selected item property editor,\nblank when no item (sector, wall, object) is selected.",
	};
	const ImVec4 tabSelColor = { 0.25f, 0.75f, 1.0f, 1.0f };
	const ImVec4 tabStdColor = { 1.0f, 1.0f, 1.0f, 0.5f };

	enum TermainlFilter
	{
		LFILTER_SCRIPT = (1 << LFILTER_ERROR),
	};
	struct LeMessage
	{
		LeMsgType type;
		std::string msg;
	};
	static std::vector<LeMessage> s_outputMsg;
	static u32 s_outputFilter = LFILTER_DEFAULT;
	static s32 s_outputTabSel = 0;
	static f32 s_infoWith;
	static s32 s_infoHeight;
	static Vec2f s_infoPos;
	static InfoTab s_infoTab = TAB_INFO;

	static s32 s_outputHeight = 26 * 5;
	static s32 s_outputPrevHeight = 26 * 5;
	static bool s_collapsed = false;

	static Feature s_prevVertexFeature = {};
	static Feature s_prevWallFeature = {};
	static Feature s_prevSectorFeature = {};
	static Feature s_prevObjectFeature = {};
	static bool s_wallShownLast = false;
	static s32 s_prevCategoryFlags = 0;

	static s32 s_groupOpen = -1;

	static bool s_scrollToBottom = false;
		
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
		s_scrollToBottom = true;
	}

	void infoPanelSetMsgFilter(u32 filter/*LFILTER_DEFAULT*/)
	{
		s_outputFilter = filter;
		s_scrollToBottom = true;
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
		s_infoTab = (InfoTab)handleTabs(s_infoTab, -12, -4, TAB_COUNT, c_infoTabs, c_infoToolTips, commitCurEntityChanges);

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4);
		ImGui::Separator();
	}

	static char s_scriptLine[4096] = { 0 };
		
	s32 infoPanelOutput(s32 width)
	{
		s32 height = s_outputHeight;

		DisplayInfo info;
		TFE_RenderBackend::getDisplayInfo(&info);

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing;
		ImGui::SetNextWindowPos({ 0.0f, f32(info.height - height + 1) });
		ImGui::SetNextWindowSize({ f32(width), f32(height) });

		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
		if (ImGui::Begin("Output##MapInfo", nullptr, window_flags))
		{
			bool restoreHeight = s_collapsed;
			s_collapsed = false;

			const size_t count = s_outputMsg.size();
			const LeMessage* msg = s_outputMsg.data();
			const ImVec4 c_typeColor[] = { {1.0f, 1.0f, 1.0f, 0.7f}, {1.0f, 1.0f, 0.25f, 1.0f}, {1.0f, 0.25f, 0.25f, 1.0f} };
						
			for (size_t i = 0; i < count; i++, msg++)
			{
				u32 typeFlag = 1 << msg->type;
				if (!(typeFlag & s_outputFilter)) { continue; }

				ImGui::TextColored(c_typeColor[msg->type], "%s", msg->msg.c_str());
			}

			if (s_outputFilter & LFILTER_SCRIPT)
			{
				if (ImGui::InputText("##ScriptInput", s_scriptLine, 4096, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					executeLine(s_scriptLine);
					memset(s_scriptLine, 0, 4096);
				}
			}

			if (restoreHeight)
			{
				s_outputHeight = s_outputPrevHeight;
			}
			else
			{
				s_outputPrevHeight = s_outputHeight;
				s_outputHeight = max(26 * 3, (s32)ImGui::GetWindowSize().y);
			}

			if (s_scrollToBottom)
			{
				ImGui::SetScrollHereY(0.999f);
				s_scrollToBottom = false;
			}
		}
		else
		{
			s_outputHeight = 26;
			s_collapsed = true;
		}
		ImGui::End();

		if (!s_collapsed)
		{
			const bool mousePressed = TFE_Input::mousePressed(MBUTTON_LEFT);
			
			ImGuiWindowFlags window_flags_tab = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_Tooltip;
			ImGui::SetNextWindowPos({ 128.0f, f32(info.height - height + 1) });
			if (ImGui::Begin("##MapInfoTab", nullptr, window_flags_tab))
			{
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4);
				ImGui::PushStyleColor(ImGuiCol_Text, s_outputTabSel == 0 ? tabSelColor : tabStdColor);
				ImGui::Text("All");
				ImGui::PopStyleColor();
				if (mouseInsideItem() && mousePressed)
				{
					s_outputTabSel = 0;
					s_outputFilter = LFILTER_DEFAULT;
				}
				
				ImGui::SameLine(0.0f, 16.0f);
				ImGui::PushStyleColor(ImGuiCol_Text, s_outputTabSel == 1 ? tabSelColor : tabStdColor);
				ImGui::Text("Warnings");
				ImGui::PopStyleColor();
				if (mouseInsideItem() && mousePressed)
				{
					s_outputTabSel = 1;
					s_outputFilter = LFILTER_WARNING;
				}

				ImGui::SameLine(0.0f, 16.0f);
				ImGui::PushStyleColor(ImGuiCol_Text, s_outputTabSel == 2 ? tabSelColor : tabStdColor);
				ImGui::Text("Errors");
				ImGui::PopStyleColor();
				if (mouseInsideItem() && mousePressed)
				{
					s_outputTabSel = 2;
					s_outputFilter = LFILTER_ERROR;
				}

				ImGui::SameLine(0.0f, 16.0f);
				ImGui::PushStyleColor(ImGuiCol_Text, (s_outputFilter & LFILTER_SCRIPT) ? tabSelColor : tabStdColor);
				ImGui::Text("Script");
				ImGui::PopStyleColor();
				if (mouseInsideItem() && mousePressed)
				{
					if (s_outputFilter & LFILTER_SCRIPT) { s_outputFilter &= ~LFILTER_SCRIPT; }
					else { s_outputFilter |= LFILTER_SCRIPT; }
					memset(s_scriptLine, 0, 4096);
					s_scrollToBottom = true;
				}
			}
			ImGui::End();
		}

		ImGui::PopStyleColor();

		return s_outputHeight;
	}

	void clearGroupSelection(Group* group)
	{
		// Clear the selection since some of it may be made non-interactable.
		selection_clear();
		if (s_featureCur.sector && s_featureCur.sector->groupId == group->id)
		{
			s_featureCur = {};
		}
		if (s_featureHovered.sector && s_featureHovered.sector->groupId == group->id)
		{
			s_featureHovered = {};
		}
	}

	void selectAndScrollToSector(EditorSector* sector)
	{
		// Clear the selection, select the sector.
		selection_clear();
		s_featureCur = {};
		s_featureHovered = {};
		s_featureCur.sector = sector;

		// Set the edit mode to "Sector"
		s_editMode = LEDIT_SECTOR;

		// Set the correct layer.
		if (!(s_editFlags & LEF_SHOW_ALL_LAYERS))
		{
			s_curLayer = sector->layer;
		}
		// Center the view on the sector.
		if (s_view == EDIT_VIEW_2D)
		{
			const Vec2f mapPos =
			{
				 (sector->bounds[0].x + sector->bounds[1].x) * 0.5f,
				-(sector->bounds[0].z + sector->bounds[1].z) * 0.5f
			};
			const Vec2f target =
			{
				mapPos.x - (s_viewportSize.x / 2) * s_zoom2d,
				mapPos.z - (s_viewportSize.z / 2) * s_zoom2d
			};
			setViewportScrollTarget2d(target);
		}
		else if (s_view == EDIT_VIEW_3D)
		{
			// Compute the look at angles.
			const Vec3f mapPos =
			{
				(sector->bounds[0].x + sector->bounds[1].x) * 0.5f,
				(sector->bounds[0].y + sector->bounds[1].y) * 0.5f,
				(sector->bounds[0].z + sector->bounds[1].z) * 0.5f
			};
			f32 targetYaw, targetPitch;
			computeLookAt(mapPos, targetYaw, targetPitch);

			// Then move closer if needed.
			Vec3f cornerDelta = { sector->bounds[1].x - mapPos.x, sector->bounds[1].y - mapPos.y, sector->bounds[1].z - mapPos.z };
			f32 radius = sqrtf(cornerDelta.x*cornerDelta.x + cornerDelta.y*cornerDelta.y + cornerDelta.z*cornerDelta.z);
			f32 d = max(20.0f, radius * 2.0f);

			Vec3f posDelta = { mapPos.x - s_camera.pos.x, mapPos.y - s_camera.pos.y, mapPos.z - s_camera.pos.z };
			f32 dist = sqrtf(posDelta.x*posDelta.x + posDelta.y*posDelta.y + posDelta.z*posDelta.z);

			if (dist > d)
			{
				const f32 scale = (dist - d) / dist;
				const Vec3f targetPos =
				{
					s_camera.pos.x + posDelta.x * scale,
					s_camera.pos.y + posDelta.y * scale,
					s_camera.pos.z + posDelta.z * scale
				};
				setViewportScrollTarget3d(targetPos, targetYaw, targetPitch);
			}
			else
			{
				setViewportScrollTarget3d(s_camera.pos, targetYaw, targetPitch);
			}
		}
	}
		
	void listNamedSectorsInGroup(Group* group)
	{
		const s32 count = (s32)s_level.sectors.size();
		const s32 groupId = group->id;
		EditorSector* sector = s_level.sectors.data();

		Sublist sublist;
		sublist_begin(sublist, 32.0f/*tabSize*/, 256.0f/*itemWidth*/);
		for (s32 i = 0; i < count; i++, sector++)
		{
			if (sector->groupId != groupId || sector->name.empty())
			{
				continue;
			}
			if (sublist_item(sublist, sector->name.c_str(), s_featureCur.sector == sector))
			{
				
			}
		}
		sublist_end(sublist);
	}

	void infoPanelMap()
	{
		const f32 iconBtnTint[] = { 103.0f / 255.0f, 122.0f / 255.0f, 139.0f / 255.0f, 1.0f };
		char name[64];
		strcpy(name, s_level.name.c_str());

		ImGui::Text("Level Name:"); ImGui::SameLine();
		if (ImGui::InputText("##InputLevelName", name, 64))
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
		ImGui::InputFloat("##GridHeight", &s_grid.height, 0.0f, 0.0f, "%0.2f", ImGuiInputTextFlags_CharsDecimal);
			
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing;

		ImVec2 pos = ImGui::GetCursorPos();
		ImGui::SetNextWindowSize({ s_infoWith, s_infoHeight - pos.y - 43 });
		ImGui::SetNextWindowPos({ s_infoPos.x, pos.y + s_infoPos.z });
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
		ImGui::Begin("Groups##MapInfo", nullptr, window_flags);
		{
			s32 groupCount = (s32)s_groups.size();
			Group* group = s_groups.data();
			for (s32 i = 0; i < groupCount; i++, group++)
			{
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4);
				if (iconButtonInline(s_groupOpen == i ? ICON_MINUS_SMALL : ICON_PLUS_SMALL, "Show named sectors in group.", iconBtnTint, true))
				{
					s_groupOpen = (s_groupOpen == i) ? -1 : i;
				}
				ImGui::SameLine(0.0f, 8.0f);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
				if (ImGui::Selectable(editor_getUniqueLabel(group->name.c_str()), s_groupCurrent == i, 0, ImVec2(264, 0)))
				{
					s_groupCurrent = i;
				}
				
				ImGui::SameLine(0.0f, 36.0f);
				if (iconButtonInline((group->flags & GRP_EXCLUDE) ? ICON_CIRCLE_BAN : ICON_CIRCLE, "Exclude group from export.", iconBtnTint, true))
				{
					if (group->flags & GRP_EXCLUDE) { group->flags &= ~GRP_EXCLUDE; }
					else { group->flags |= GRP_EXCLUDE; }
				}

				ImGui::SameLine(0.0f, 0.0f);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
				if (iconButtonInline((group->flags & GRP_HIDDEN) ? ICON_EYE_CLOSED : ICON_EYE, "Hide or show the group.", iconBtnTint, true))
				{
					if (group->flags & GRP_HIDDEN) { group->flags &= ~GRP_HIDDEN; }
					else { group->flags |= GRP_HIDDEN; }
					clearGroupSelection(group);
				}

				ImGui::SameLine(0.0f, 0.0f);
				if (iconButtonInline((group->flags & GRP_LOCKED) ? ICON_LOCKED : ICON_UNLOCKED, "Lock or unlock the group for editing.", iconBtnTint, true))
				{
					if (group->flags & GRP_LOCKED) { group->flags &= ~GRP_LOCKED; }
					else { group->flags |= GRP_LOCKED; }
					clearGroupSelection(group);
				}

				ImGui::SameLine(0.0f, 0.0f);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
				ImGui::ColorEdit3(editor_getUniqueLabel(""), group->color.m, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoInputs);

				if (s_groupOpen == i)
				{
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
					listNamedSectorsInGroup(group);
				}
				else
				{
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 8);
				}

				ImGui::Separator();
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
			}
		}
		ImGui::End();
		ImGui::PopStyleColor();

		bool isDisabled = false;
		ImGui::SetCursorPosY(pos.y + s_infoPos.z + s_infoHeight - pos.y - 52);
		if (ImGui::Button("Add"))
		{
			groups_clearName();
			openEditorPopup(TFE_Editor::POPUP_GROUP_NAME);
		}
		ImGui::SameLine(0.0f, 8.0f);

		if (s_groupCurrent == 0)
		{
			disableNextItem();
			isDisabled = true;
		}
		if (ImGui::Button("Remove"))
		{
			groups_remove(s_groupCurrent);
		}

		ImGui::SameLine(0.0f, 8.0f);

		if (s_groupCurrent == 1 && !isDisabled)
		{
			disableNextItem();
			isDisabled = true;
		}
		if (ImGui::ArrowButton("##MoveUp", ImGuiDir_Up))
		{
			groups_moveUp(s_groupCurrent);
		}

		ImGui::SameLine(0.0f, 8.0f);

		if (s_groupCurrent == 0 || s_groupCurrent == (s32)s_groups.size() - 1)
		{
			if (!isDisabled)
			{
				disableNextItem();
			}
			isDisabled = true;
		}
		else if (isDisabled)
		{
			enableNextItem();
			isDisabled = false;
		}
		if (ImGui::ArrowButton("##MoveDown", ImGuiDir_Down))
		{
			groups_moveDown(s_groupCurrent);
		}
		if (isDisabled)
		{
			enableNextItem();
		}
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
		if (ImGui::InputFloat(id0, &value->x, 0.0, 0.0, "%0.6f", ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_NoUndoRedo))
		{
			if (value->x != prevPos.x)
			{
				hadFocusX = true;
			}
		}
		bool lostFocusX = !ImGui::IsItemFocused() && !ImGui::IsItemHovered() && !ImGui::IsItemActive();

		ImGui::SameLine();
		ImGui::PushItemWidth(98.0f);
		if (ImGui::InputFloat(id1, &value->z, 0.0, 0.0, "%0.6f", ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_NoUndoRedo))
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
		ImGui::LabelText(labelId, "%s", labelText);
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

		EditorSector* prevSector;
		s32 prevWallId;
		getPrevWallFeature(prevSector, prevWallId);
		if (prevSector != sector || prevWallId != wallId)
		{
			ImGui::SetWindowFocus(NULL);
		}

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

		ImGui::SameLine(0.0f, 20.0f);
		if (ImGui::Button("Edit INF"))
		{
			TFE_Editor::openEditorPopup(POPUP_EDIT_INF, wallId, sector->name.empty() ? nullptr : (void*)sector->name.c_str());
		}

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

		// Keep text input from one sector from bleeding into the next.
		EditorSector* prev;
		getPrevSectorFeature(prev);
		if (prev != sector)
		{
			ImGui::SetWindowFocus(NULL);
		}

		setPrevSectorFeature(sector);
		s_wallShownLast = false;

		ImGui::Text("Sector ID: %d      Wall Count: %u", sector->id, (u32)sector->walls.size());
		ImGui::Separator();

		// Sector Name (optional, used by INF)
		char sectorName[64];
		char inputName[256];
		strcpy(sectorName, sector->name.c_str());
		sprintf(inputName, "##NameSector%d", sector->id);

		infoLabel("##NameLabel", "Name", 32);
		ImGui::PushItemWidth(240.0f);

		// Turn the name red if it matches another sector.
		s32 otherNameId = findSectorByName(sectorName, sector->id);
		if (otherNameId >= 0) { ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 0.2f, 0.2f, 1.0f}); }
		if (ImGui::InputText(inputName, sectorName, getSectorNameLimit()))
		{
			sector->name = sectorName;
		}
		ImGui::PopItemWidth();
		if (otherNameId >= 0) { ImGui::PopStyleColor(); }

		ImGui::SameLine(0.0f, 20.0f);
		if (ImGui::Button("Edit INF"))
		{
			TFE_Editor::openEditorPopup(POPUP_EDIT_INF, 0xffffffff, sector->name.empty() ? nullptr : (void*)sector->name.c_str());
		}

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
			s_prevObjectFeature.featureIndex = !s_prevObjectFeature.sector || s_prevObjectFeature.sector->obj.empty() ? -1 : 0;
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

	bool doesEntityExistInProject(const Entity* newEntityDef)
	{
		const s32 count = (s32)s_projEntityDefList.size();
		const Entity* entity = s_projEntityDefList.data();
		for (s32 e = 0; e < count; e++, entity++)
		{
			if (entityDefsEqual(newEntityDef, entity))
			{
				return true;
			}
		}
		return false;
	}
	
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

	void commitCurEntityChanges(void)
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

	static bool s_browsing = false;

	const Entity* getFirstEntityOfType(EntityType type)
	{
		const s32 count = (s32)s_entityDefList.size();
		const Entity* entity = s_entityDefList.data();
		for (s32 i = 0; i < count; i++, entity++)
		{
			if (entity->type == type)
			{
				return entity;
			}
		}
		return nullptr;
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
			ImGui::SetWindowFocus(NULL);
			commitCurEntityChanges();
		}
		setPrevObjectFeature(sector, objIndex);

		if (s_objEntity.id < 0)
		{
			s_objEntity = s_level.entities[obj->entityId];
		}

		if (s_browsing && !isPopupOpen())
		{
			s_browsing = false;
			const char* newAssetName = AssetBrowser::getSelectedAssetName();
			if (newAssetName)
			{
				s_objEntity.assetName = newAssetName;
			}
		}

		ImGui::Text("Sector ID: %d, Object Index: %d", sector->id, objIndex);
		ImGui::Separator();
		// Entity Name
		ImGui::Text("%s", "Name"); ImGui::SameLine(0.0f, 17.0f);
		char name[256];
		strcpy(name, s_objEntity.name.c_str());
		ImGui::SetNextItemWidth(196.0f);

		char inputName[256];
		sprintf(inputName, "##NameInput_%d_%d", sector->id, objIndex);
		if (ImGui::InputText(inputName, name, 256))
		{
			s_objEntity.name = name;
		}
		ImGui::SameLine(0.0f, 16.0f);
		// Entity Class/Type
		s32 classId = s_objEntity.type;
		ImGui::Text("%s", "Class"); ImGui::SameLine(0.0f, 8.0f);
		ImGui::SetNextItemWidth(96.0f);

		EntityType prev = s_objEntity.type;
		if (ImGui::Combo("##Class", (s32*)&classId, c_entityClassName, ETYPE_COUNT))
		{
			s_objEntity.type = EntityType(classId);
			// We need a default asset if the class type has changed, since the current asset will not work.
			if (prev != s_objEntity.type)
			{
				// So just search for the first entity in the template list of that type and use that asset.
				const Entity* defEntity = getFirstEntityOfType(s_objEntity.type);
				assert(defEntity);
				if (defEntity)
				{
					s_objEntity.assetName = defEntity->assetName;
				}
			}
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
			AssetType assetType = TYPE_NOT_SET;
			if (s_objEntity.type == ETYPE_SPRITE)
			{
				assetType = TYPE_SPRITE;
			}
			else if (s_objEntity.type == ETYPE_FRAME)
			{
				assetType = TYPE_FRAME;
			}
			else if (s_objEntity.type == ETYPE_3D)
			{
				assetType = TYPE_3DOBJ;
			}
			if (assetType != TYPE_NOT_SET)
			{
				openEditorPopup(TFE_Editor::POPUP_BROWSE, u32(assetType), (void*)s_objEntity.assetName.c_str());
				s_browsing = true;
			}
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
		ImGui::BeginChild("##LogicList", { (f32)min(listWidth, 400), 64 }, ImGuiChildFlags_Border);
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
		ImGui::BeginChild("##VariableList", { (f32)min(listWidth, 400), varListHeight }, ImGuiChildFlags_Border);
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

				if (def->type != EVARTYPE_FLAGS) { ImGui::SameLine(0.0f, 8.0f); }
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
						const s32 flagCount = (s32)def->flags.size();
						const EntityVarFlag* flag = def->flags.data();
						for (s32 f = 0; f < flagCount; f++, flag++)
						{
							ImGui::CheckboxFlags(flag->name.c_str(), (u32*)&list[i].value.iValue, (u32)flag->value);
							if (f < flagCount - 1) { ImGui::SameLine(0.0f, 12.0f); }
						}
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
						char pair1[256], pair2[256];
						strcpy(pair1, list[i].value.sValue.c_str());
						strcpy(pair2, list[i].value.sValue1.c_str());
						ImGui::SetNextItemWidth(128.0f);
						if (ImGui::InputText("###Pair1", pair1, 256))
						{
							list[i].value.sValue = pair1;
						}
						ImGui::SameLine(0.0f, 8.0f);
						ImGui::SetNextItemWidth(128.0f);
						if (ImGui::InputText("###Pair2", pair2, 256))
						{
							list[i].value.sValue1 = pair2;
						}
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
		if (ImGui::Button("Category"))
		{
			openEditorPopup(TFE_Editor::POPUP_CATEGORY);
			s_prevCategoryFlags = s_objEntity.categories;
		}
		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button("Add to Entity Def"))
		{
			s_objEntity.categories |= 1;	// custom.
			if (!doesEntityExistInProject(&s_objEntity))
			{
				s_projEntityDefList.push_back(s_objEntity);
				s_entityDefList.push_back(s_objEntity);
				saveProjectEntityList();
			}
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

	Vec4f packedColorToVec4(u32 color)
	{
		const f32 scale = 1.0f / 255.0f;
		const f32 a = f32((color >> 24u) & 0xffu) * scale;
		const f32 b = f32((color >> 16u) & 0xffu) * scale;
		const f32 g = f32((color >>  8u) & 0xffu) * scale;
		const f32 r = f32((color       ) & 0xffu) * scale;

		return { r, g, b, a };
	}

	u32 colorVec4ToPacked(Vec4f color)
	{
		u32 a = u32(color.w * 255.0f);
		u32 r = u32(color.x * 255.0f);
		u32 g = u32(color.y * 255.0f);
		u32 b = u32(color.z * 255.0f);

		return r | (g << 8u) | (b << 16u) | (a << 24u);
	}

	bool optionFloatInput(const char* label, f32* value, f32 step, s32 colWidth = 0, s32 width = 0, const char* prec = "%.3f");

	bool optionFloatInput(const char* label, f32* value, f32 step, s32 colWidth, s32 width, const char* prec)
	{
		const f32 textWidth = (colWidth == 0) ? ImGui::CalcTextSize(label).x + 8.0f : f32(colWidth);

		ImGui::SetNextItemWidth(textWidth);
		ImGui::LabelText("##Label", "%s", label); ImGui::SameLine();
		if (width) { ImGui::SetNextItemWidth((f32)width); }
		return ImGui::InputFloat(editor_getUniqueLabel(""), value, step, 10.0f * step, prec);
	}

	static s32 s_selectedOffset = -1;
	
	void infoPanelGuideline()
	{
		s32 id = s_curGuideline >= 0 ? s_curGuideline : s_hoveredGuideline;
		if (id < 0) { return; }

		Guideline* guideline = &s_level.guidelines[id];

		sectionHeader("Settings");
		f32 checkWidth = ImGui::CalcTextSize("Limit Height Shown").x + 8.0f;
		optionCheckbox("Limit Height Shown", &guideline->flags, GLFLAG_LIMIT_HEIGHT, checkWidth);
		optionCheckbox("Disable Snapping", &guideline->flags, GLFLAG_DISABLE_SNAPPING, checkWidth);
		ImGui::Separator();

		if ((guideline->flags & GLFLAG_LIMIT_HEIGHT) || !(guideline->flags & GLFLAG_DISABLE_SNAPPING))
		{
			sectionHeader("Values");
			s32 colWidth = s32(ImGui::CalcTextSize("Min Height Shown").x + 8.0f);
			if (guideline->flags & GLFLAG_LIMIT_HEIGHT)
			{
				optionFloatInput("Min Height Shown", &guideline->minHeight, 0.1f, colWidth, 128, "%.2f");
				optionFloatInput("Max Height Shown", &guideline->maxHeight, 0.1f, colWidth, 128, "%.2f");
			}
			if (!(guideline->flags & GLFLAG_DISABLE_SNAPPING))
			{
				optionFloatInput("Max Snap Range", &guideline->maxSnapRange, 0.1f, colWidth, 128, "%.2f");
			}
			ImGui::Separator();
		}

		sectionHeader("Offsets");
		s32 itemToRemove = -1;
		s32 count = (s32)guideline->offsets.size();
		f32* offsetValue = guideline->offsets.data();
		guideline->maxOffset = 0.0f;
		for (s32 i = 0; i < count; i++)
		{
			char label[256];
			sprintf(label, "Offset %d", i + 1);
			optionFloatInput(label, &offsetValue[i], 0.1f, 0, 128, "%.2f");
			guideline->maxOffset = std::max(guideline->maxOffset, fabsf(offsetValue[i]));

			ImGui::SameLine();
			if (ImGui::Button(editor_getUniqueLabel("Remove")))
			{
				itemToRemove = i;
			}
		}
		guideline_computeBounds(guideline);
		guideline_computeKnots(guideline);
		ImGui::Separator();
		if (count >= 4) { disableNextItem(); }
		if (ImGui::Button("Add"))
		{
			guideline->offsets.push_back({ 0.0f });
		}
		if (count >= 4) { enableNextItem(); }

		if (itemToRemove >= 0 && itemToRemove < count)
		{
			for (s32 i = itemToRemove; i < count - 1; i++)
			{
				guideline->offsets[i] = guideline->offsets[i + 1];
			}
			guideline->offsets.pop_back();
		}
	}

	void infoPanelNote()
	{
		s32 id = s_curLevelNote >= 0 ? s_curLevelNote : s_hoveredLevelNote;
		if (id < 0) { return; }
				
		LevelNote* note = &s_level.notes[id];
		
		ImGui::CheckboxFlags("2D Only", &note->flags, LNF_2D_ONLY); ImGui::SameLine();
		ImGui::CheckboxFlags("No Fade in 3D", &note->flags, LNF_3D_NO_FADE); ImGui::SameLine();
		ImGui::CheckboxFlags("Always Show Text", &note->flags, LNF_TEXT_ALWAYS_SHOW);
		ImGui::Spacing();
		ImGui::SetNextItemWidth(128.0f);
		ImGui::LabelText("##Label", "Start Fade (3D)"); ImGui::SameLine();
		ImGui::SetNextItemWidth(128.0f);
		ImGui::InputFloat("##StartFade", &note->fade.x, 1.0f, 10.0f, "%.1f");
		ImGui::SetNextItemWidth(128.0f);
		ImGui::LabelText("##Label", "End Fade (3D)"); ImGui::SameLine();
		ImGui::SetNextItemWidth(128.0f);
		ImGui::InputFloat("##EndFade", &note->fade.z, 1.0f, 10.0f, "%.1f");
		ImGui::Separator();
		ImGui::SetNextItemWidth(128.0f);
		ImGui::LabelText("##Label", "Icon Color"); ImGui::SameLine();
		Vec4f iconColor = packedColorToVec4(note->iconColor);
		if (ImGui::ColorEdit4(editor_getUniqueLabel(""), iconColor.m, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoInputs))
		{
			note->iconColor = colorVec4ToPacked(iconColor);
		}
		ImGui::SetNextItemWidth(128.0f);
		ImGui::LabelText("##Label", "Text Color"); ImGui::SameLine();
		Vec4f textColor = packedColorToVec4(note->textColor);
		if (ImGui::ColorEdit4(editor_getUniqueLabel(""), textColor.m, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoInputs))
		{
			note->textColor = colorVec4ToPacked(textColor);
		}
		ImGui::Separator();
		char textInputId[256];
		char tmpBuffer[4096];
		// Key the Input by ID since ImGUI keeps its own copy internally, which can cause issues when changing IDs.
		sprintf(textInputId, "##TextBox%d", id);
		strcpy(tmpBuffer, note->note.c_str());

		if (ImGui::InputTextMultiline(textInputId, tmpBuffer, 4096, { s_infoWith - 16.0f, 354.0f }))
		{
			note->note = tmpBuffer;
		}
	}
		
	void drawInfoPanel(EditorView view)
	{
		bool show = getSelectMode() == SELECTMODE_NONE;

		// Draw the info bars.
		s_infoHeight = 486 + 44;
		infoToolBegin(s_infoHeight);
		{
			if (s_infoTab == TAB_ITEM && show)
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
				else if (s_editMode == LEDIT_NOTES)
				{
					infoPanelNote();
				}
				else if (s_editMode == LEDIT_GUIDELINES)
				{
					infoPanelGuideline();
				}
			}
			else if (s_infoTab == TAB_INFO && show)
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
		
	bool categoryPopupUI()
	{
		f32 winWidth = 600.0f;
		f32 winHeight = 146.0f;
		bool btn = false;
		pushFont(TFE_Editor::FONT_SMALL);

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize;
		ImGui::SetNextWindowSize({ winWidth, winHeight });
		if (ImGui::BeginPopupModal("Category", nullptr, window_flags))
		{
			//s_objEntity;
			s32 count = (s32)s_categoryList.size();
			const Category* cat = s_categoryList.data() + 1;
			for (s32 i = 1; i < count; i++, cat++)
			{
				const s32 flag = 1 << i;
				ImGui::CheckboxFlags(cat->name.c_str(), (u32*)&s_objEntity.categories, (u32)flag);

				s32 rowIdx = (i & 3);
				if (rowIdx != 0 && i < count - 1)
				{
					ImGui::SameLine(150.0f * rowIdx, 0.0f);
				}
			}

			ImGui::Separator();

			if (ImGui::Button("Confirm"))
			{
				btn = true;
			}
			ImGui::SameLine(0.0f, 32.0f);
			if (ImGui::Button("Cancel"))
			{
				btn = true;
				s_objEntity.categories = s_prevCategoryFlags;
			}
			ImGui::EndPopup();
		}
		popFont();

		return btn;
	}
}
