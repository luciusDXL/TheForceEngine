#include "editSurface.h"
#include "editTransforms.h"
#include "editVertex.h"
#include "editCommon.h"
#include "browser.h"
#include "levelEditor.h"
#include "hotkeys.h"
#include "error.h"
#include "infoPanel.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "levelEditorHistory.h"
#include "sharedState.h"
#include "selection.h"
#include "guidelines.h"
#include <TFE_System/math.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/LevelEditor/Rendering/grid.h>
#include <TFE_Input/input.h>
#include <TFE_Jedi/Level/rwall.h>

#include <climits>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;
using namespace TFE_Jedi;

namespace LevelEditor
{
	static Vec2f s_copiedTextureOffset = { 0 };

	void selectSimilarFlats(EditorSector* rootSector, HitPart part);
	void selectSimilarWalls(EditorSector* rootSector, s32 wallIndex, HitPart part, bool autoAlign=false);
	void snapSignToCursor(EditorSector* sector, EditorWall* wall, s32 signTexIndex, Vec2f* signOffset);

	////////////////////////////////////////
	// API
	////////////////////////////////////////
	void handleMouseControlSurface(Vec2f worldPos)
	{
		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);

		// Short names to make the logic easier to follow.
		const bool selAdd = TFE_Input::keyModDown(KEYMOD_SHIFT);
		const bool selRem = TFE_Input::keyModDown(KEYMOD_ALT);
		const bool selToggle = TFE_Input::keyModDown(KEYMOD_CTRL);
		const bool selToggleDrag = selAdd && selToggle;

		const bool mousePressed = s_singleClick;
		const bool mouseDown = TFE_Input::mouseDown(MouseButton::MBUTTON_LEFT);

		const bool del = TFE_Input::keyPressed(KEY_DELETE);
		const bool hasHovered = selection_hasHovered();
		const bool hasFeature = selection_getCount() > 0;

		s32 sectorId = -1, wallIndex;
		HitPart part;
		EditorSector* featureSector = nullptr;
		if (del && (hasHovered || hasFeature))
		{
			if (selection_getSurface(hasFeature ? 0 : SEL_INDEX_HOVERED, featureSector, wallIndex, &part))
			{
				sectorId = featureSector->id;
			}
			if (sectorId < 0) { return; }

			if (part == HP_FLOOR || part == HP_CEIL)
			{
				edit_deleteSector(sectorId);
			}
			else if (part == HP_SIGN)
			{
				if (featureSector && wallIndex >= 0)
				{
					// Clear the selections when deleting a sign -
					// otherwise the source wall will still be selected.
					edit_clearSelections();

					FeatureId id = createFeatureId(featureSector, wallIndex, HP_SIGN);
					edit_clearTexture(1, &id);
				}
			}
			else
			{
				// Deleting a wall is the same as deleting vertex 0.
				// So re-use the same command, but with the delete wall name.
				const s32 vertexIndex = s_level.sectors[sectorId].walls[wallIndex].idx[0];
				edit_deleteVertex(sectorId, vertexIndex, LName_DeleteWall);
			}
			return;
		}

		if (mousePressed)
		{
			assert(!s_dragSelect.active);
			if (!selToggle && (selAdd || selRem))
			{
				startDragSelect(mx, my, selAdd ? DSEL_ADD : DSEL_REM);
			}
			else if (selToggle)
			{
				if (hasHovered && selection_getSurface(SEL_INDEX_HOVERED, featureSector, wallIndex, &part))
				{
					s_editMove = true;
					adjustGridHeight(featureSector);
					selection_surface(SA_TOGGLE, featureSector, wallIndex, part);
				}
			}
			else
			{
				if (hasHovered && selection_getSurface(SEL_INDEX_HOVERED, featureSector, wallIndex, &part))
				{
					handleSelectMode(featureSector, (part == HP_FLOOR || part == HP_CEIL) ? -1 : wallIndex);
					if (!selection_surface(SA_CHECK_INCLUSION, featureSector, wallIndex, part))
					{
						selection_surface(SA_SET, featureSector, wallIndex, part);
						edit_applyTransformChange();
					}

					// Set this to the 3D cursor position, so the wall doesn't "pop" when grabbed.
					s_curVtxPos = s_cursor3d;
					adjustGridHeight(featureSector);
					s_editMove = true;

					// TODO: Hotkeys...
					edit_setWallMoveMode(TFE_Input::keyDown(KEY_N) ? WMM_NORMAL : WMM_FREE);
				}
				else if (!s_editMove)
				{
					startDragSelect(mx, my, DSEL_SET);
				}
			}
		}
		else if (s_doubleClick)
		{
			if (hasHovered && selection_getSurface(SEL_INDEX_HOVERED, featureSector, wallIndex, &part))
			{
				if (!TFE_Input::keyModDown(KEYMOD_SHIFT))
				{
					selection_clear(SEL_GEO);
				}
				if (part == HP_FLOOR || part == HP_CEIL)
				{
					selectSimilarFlats(featureSector, part);
				}
				else
				{
					selectSimilarWalls(featureSector, wallIndex, part);
				}
			}
		}
		else if (mouseDown)
		{
			if (!s_dragSelect.active)
			{
				if (selToggleDrag && hasHovered && selection_getSurface(SEL_INDEX_HOVERED, featureSector, wallIndex, &part))
				{
					adjustGridHeight(featureSector);
					selection_surface(SA_ADD, featureSector, wallIndex, part);
				}
			}
			// Draw select continue.
			else if (!selToggle && !s_editMove)
			{
				updateDragSelect(mx, my);
			}
			else
			{
				s_dragSelect.active = false;
			}
		}
		else if (s_dragSelect.active)
		{
			s_dragSelect.active = false;
		}

		// Handle copy and paste.
		if (s_view == EDIT_VIEW_3D && hasHovered)
		{
			edit_applySurfaceTextures();
		}
		handleVertexInsert(worldPos);
	}

	void edit_clearCopiedTextureOffset()
	{
		s_copiedTextureOffset = { 0 };
	}

	// Hacky version for now, clean up once it works... (separate selection lists).
	void edit_autoAlign(s32 sectorId, s32 wallIndex, HitPart part)
	{
		// Save the selection and clear.
		selection_saveGeoSelections();
		selection_clear(SEL_GEO);

		// Find similar walls.
		EditorSector* rootSector = &s_level.sectors[sectorId];
		selectSimilarWalls(rootSector, wallIndex, part, true);

		// Restore the selection.
		selection_restoreGeoSelections();
	}

	void edit_applySurfaceTextures()
	{
		if (!selection_hasHovered()) { return; }
		EditorSector* sector = nullptr;
		HitPart part = HP_MID;
		s32 index = -1;
		if (s_editMode == LEDIT_WALL)
		{
			selection_getSurface(SEL_INDEX_HOVERED, sector, index, &part);
		}
		else if (s_editMode == LEDIT_SECTOR)
		{
			selection_getSector(SEL_INDEX_HOVERED, sector);
			part = (s_sectorDrawMode == SDM_TEXTURED_FLOOR) ? HP_FLOOR : HP_CEIL;
		}
		if (!sector) { return; }

		if (TFE_Input::keyPressed(KEY_T) && TFE_Input::keyModDown(KEYMOD_CTRL))
		{
			const LevelTextureAsset* textureAsset = nullptr;
			if (s_view == EDIT_VIEW_2D)
			{
				if (s_sectorDrawMode == SDM_TEXTURED_FLOOR)
				{
					textureAsset = sector->floorTex.texIndex < 0 ? nullptr : &s_level.textures[sector->floorTex.texIndex];
					s_copiedTextureOffset = sector->floorTex.offset;
				}
				else if (s_sectorDrawMode == SDM_TEXTURED_CEIL)
				{
					textureAsset = sector->ceilTex.texIndex < 0 ? nullptr : &s_level.textures[sector->ceilTex.texIndex];
					s_copiedTextureOffset = sector->ceilTex.offset;
				}
			}
			else
			{
				if (part == HP_FLOOR)
				{
					textureAsset = sector->floorTex.texIndex < 0 ? nullptr : &s_level.textures[sector->floorTex.texIndex];
					s_copiedTextureOffset = sector->floorTex.offset;
				}
				else if (part == HP_CEIL)
				{
					textureAsset = sector->ceilTex.texIndex < 0 ? nullptr : &s_level.textures[sector->ceilTex.texIndex];
					s_copiedTextureOffset = sector->ceilTex.offset;
				}
				else
				{
					const EditorWall* wall = &sector->walls[index];
					if (part == HP_MID)
					{
						textureAsset = wall->tex[WP_MID].texIndex < 0 ? nullptr : &s_level.textures[wall->tex[WP_MID].texIndex];
						s_copiedTextureOffset = wall->tex[WP_MID].offset;
					}
					else if (part == HP_TOP)
					{
						textureAsset = wall->tex[HP_TOP].texIndex < 0 ? nullptr : &s_level.textures[wall->tex[HP_TOP].texIndex];
						s_copiedTextureOffset = wall->tex[HP_TOP].offset;
					}
					else if (part == HP_BOT)
					{
						textureAsset = wall->tex[HP_BOT].texIndex < 0 ? nullptr : &s_level.textures[wall->tex[HP_BOT].texIndex];
						s_copiedTextureOffset = wall->tex[HP_BOT].offset;
					}
					else if (part == HP_SIGN)
					{
						textureAsset = wall->tex[HP_SIGN].texIndex < 0 ? nullptr : &s_level.textures[wall->tex[HP_SIGN].texIndex];
						s_copiedTextureOffset = wall->tex[HP_SIGN].offset;
					}
				}
			}

			if (textureAsset)
			{
				// Find the texture in the list.
				const s32 count = (s32)s_levelTextureList.size();
				const Asset* levelTexAsset = s_levelTextureList.data();
				for (s32 i = 0; i < count; i++, levelTexAsset++)
				{
					if (strcasecmp(levelTexAsset->name.c_str(), textureAsset->name.c_str()) == 0)
					{
						s_selectedTexture = i;
						browserScrollToSelection();
						break;
					}
				}
			}
		}
		else if (TFE_Input::keyPressed(KEY_T) && TFE_Input::keyModDown(KEYMOD_SHIFT) && s_selectedTexture >= 0)
		{
			edit_applySignToSelection(s_selectedTexture);
		}
		else if (TFE_Input::keyPressed(KEY_T) && s_selectedTexture >= 0)
		{
			edit_applyTextureToSelection(s_selectedTexture, nullptr);
		}
		else if (TFE_Input::keyPressed(KEY_V) && TFE_Input::keyModDown(KEYMOD_CTRL) && s_selectedTexture >= 0)
		{
			edit_applyTextureToSelection(s_selectedTexture, &s_copiedTextureOffset);
		}
		else if (TFE_Input::keyPressed(KEY_A) && TFE_Input::keyModDown(KEYMOD_CTRL))
		{
			edit_autoAlign(sector->id, index, part);
		}
	}

	void edit_applyTextureToSelection(s32 texIndex, Vec2f* offset)
	{
		if (!selection_hasHovered()) { return; }

		bool doesItemExist = false;
		EditorSector* sector = nullptr;
		s32 featureIndex = -1;
		HitPart part = HP_NONE;
		if (s_editMode == LEDIT_WALL)
		{
			selection_getSurface(SEL_INDEX_HOVERED, sector, featureIndex, &part);
			doesItemExist = selection_surface(SA_CHECK_INCLUSION, sector, featureIndex, part);
		}
		else if (s_editMode == LEDIT_SECTOR)
		{
			selection_getSector(SEL_INDEX_HOVERED, sector);
			doesItemExist = selection_sector(SA_CHECK_INCLUSION, sector);
		}

		if (selection_getCount() < 1 || doesItemExist)
		{
			if (doesItemExist)
			{
				FeatureId* list = nullptr;
				u32 count = selection_getList(list);
				if (count && list)
				{
					edit_setTexture(count, list, texIndex, offset);
				}
				// cmd_addSetTexture(count, s_selectionList.data(), texIndex, offset);
			}
			else
			{
				FeatureId id = createFeatureId(sector, featureIndex, part);
				edit_setTexture(1, &id, texIndex, offset);
				// cmd_addSetTexture(1, &id, texIndex, offset);
			}
		}
	}

	void edit_applySignToSelection(s32 signIndex)
	{
		s32 texIndex = getTextureIndex(s_levelTextureList[signIndex].name.c_str());
		if (texIndex < 0) { return; }
		EditorTexture* signTex = (EditorTexture*)getAssetData(s_level.textures[texIndex].handle);
		if (!signTex) { return; }

		// For now, this only works for hovered features.
		//FeatureId id = createFeatureId(s_featureHovered.sector, s_featureHovered.featureIndex, WP_SIGN);
		if (!selection_hasHovered() || s_editMode != LEDIT_WALL)
		{
			return;
		}
		EditorSector* sector;
		s32 featureIndex;
		if (!selection_getSurface(SEL_INDEX_HOVERED, sector, featureIndex))
		{
			return;
		}

		// Center the sign on the mouse cursor.
		EditorWall* wall = &sector->walls[featureIndex];

		Vec2f offset = { 0 };
		snapSignToCursor(sector, wall, texIndex, &offset);

		FeatureId id = createFeatureId(sector, featureIndex, WP_SIGN);
		edit_setTexture(1, &id, signIndex, &offset);
		// cmd_addSetTexture(1, &id, signIndex, &offset);
	}

	////////////////////////////////////////
	// Internal
	////////////////////////////////////////
	struct Adjoin
	{
		s32 adjoinId;
		s32 mirrorId;
		s32 side;
		Vec2f offset[2];
	};
	std::vector<Adjoin> s_adjoinList;

	void addToAdjoinList(Adjoin adjoin)
	{
		s32 count = (s32)s_adjoinList.size();
		Adjoin* adj = s_adjoinList.data();
		for (s32 i = 0; i < count; i++, adj++)
		{
			if (adjoin.adjoinId == adj->adjoinId && adjoin.mirrorId == adj->mirrorId && adjoin.side == adj->side)
			{
				return;
			}
		}
		s_adjoinList.push_back(adjoin);
	}

	s32 getLeftWall(EditorSector* sector, s32 wallIndex)
	{
		EditorWall* baseWall = &sector->walls[wallIndex];
		s32 leftIndex = baseWall->idx[0];

		const s32 wallCount = (s32)sector->walls.size();
		const EditorWall* wall = sector->walls.data();
		for (s32 w = 0; w < wallCount; w++, wall++)
		{
			if (wall->idx[1] == leftIndex)
			{
				return w;
			}
		}
		return -1;
	}

	s32 getRightWall(EditorSector* sector, s32 wallIndex)
	{
		EditorWall* baseWall = &sector->walls[wallIndex];
		s32 rightIndex = baseWall->idx[1];

		const s32 wallCount = (s32)sector->walls.size();
		const EditorWall* wall = sector->walls.data();
		for (s32 w = 0; w < wallCount; w++, wall++)
		{
			if (wall->idx[0] == rightIndex)
			{
				return w;
			}
		}
		return -1;
	}

	s32 getPartTextureIndex(EditorWall* wall, HitPart part)
	{
		s32 texId = -1;
		switch (part)
		{
			case HP_MID:
			{
				texId = wall->tex[WP_MID].texIndex;
			} break;
			case HP_TOP:
			{
				texId = wall->tex[WP_TOP].texIndex;
			} break;
			case HP_BOT:
			{
				texId = wall->tex[WP_BOT].texIndex;
			} break;
			case HP_SIGN:
			{
				texId = wall->tex[WP_SIGN].texIndex;
			} break;
		}
		return texId;
	}

	Vec2f getPartTextureOffset(EditorWall* wall, HitPart part)
	{
		Vec2f offset = { 0 };
		switch (part)
		{
			case HP_MID:
			{
				offset = wall->tex[WP_MID].offset;
			} break;
			case HP_TOP:
			{
				offset = wall->tex[WP_TOP].offset;
			} break;
			case HP_BOT:
			{
				offset = wall->tex[WP_BOT].offset;
			} break;
			case HP_SIGN:
			{
				offset = wall->tex[WP_SIGN].offset;
			} break;
		}
		return offset;
	}

	Vec2f getTexOffsetMod(s32 texIndex, Vec2f offset)
	{
		EditorTexture* tex = getTexture(texIndex);
		f32 xmod = tex->width  / 8.0f;
		return { fmodf(offset.x, xmod), offset.z };
	}

	void setPartTextureOffset(EditorWall* wall, HitPart part, Vec2f offset)
	{
		switch (part)
		{
			case HP_MID:
			{
				wall->tex[WP_MID].offset = getTexOffsetMod(wall->tex[WP_MID].texIndex, offset);
			} break;
			case HP_TOP:
			{
				wall->tex[WP_TOP].offset = getTexOffsetMod(wall->tex[WP_TOP].texIndex, offset);
			} break;
			case HP_BOT:
			{
				wall->tex[WP_BOT].offset = getTexOffsetMod(wall->tex[WP_BOT].texIndex, offset);
			} break;
			case HP_SIGN:
			{
				wall->tex[WP_SIGN].offset = offset;
			} break;
		}
	}

	void selectSimilarTraverse(EditorSector* sector, s32 wallIndex, s32 texId, s32 fixedDir, Vec2f* texOffset, f32 baseHeight)
	{
		const s32 layer = (s_editFlags & LEF_SHOW_ALL_LAYERS) ? LAYER_ANY : s_curLayer;

		s32 start = 0;
		s32 end = 2;
		if (fixedDir == 0) { end = 1; }
		else if (fixedDir == 1) { start = 1; }

		for (s32 dir = start; dir < end; dir++)
		{
			// Walk right until we either loop around again or stop.
			// If we stop, we need to loop left.
			s32 nextIndex = dir == 0 ? getRightWall(sector, wallIndex) : getLeftWall(sector, wallIndex);
			while (nextIndex >= 0)
			{
				EditorWall* wall = &sector->walls[nextIndex];
				if (wall->adjoinId < 0)
				{
					s32 curTexId = getPartTextureIndex(wall, HP_MID);
					if (texId < 0 || curTexId == texId)
					{
						if (!selection_surface(SA_ADD, sector, nextIndex))
						{
							break;
						}
						if (texOffset)
						{
							if (dir == 1) { texOffset[dir].x -= getWallLength(sector, wall); }
							setPartTextureOffset(wall, HP_MID, { texOffset[dir].x, texOffset[dir].z + sector->floorHeight - baseHeight });
							if (dir == 0) { texOffset[dir].x += getWallLength(sector, wall); }
						}
					}
					else
					{
						break;
					}
				}
				else
				{
					bool hasMatch = false;
					EditorSector* next = &s_level.sectors[wall->adjoinId];

					f32 offsetX = 0.0f, offsetZ = 0.0f;
					if (texOffset)
					{
						offsetX = dir == 1 ? texOffset[dir].x - getWallLength(sector, wall) : texOffset[dir].x;
						offsetZ = texOffset[dir].z;
					}

					// Add adjoin to the list.
					// Then go left and right from the adjoin.
					if (layer == LAYER_ANY || next->layer == layer)
					{
						if (texOffset)
						{
							addToAdjoinList({ wall->adjoinId, wall->mirrorId, dir, {texOffset[0], texOffset[1]} });
						}
						else
						{
							addToAdjoinList({ wall->adjoinId, wall->mirrorId, dir, {0} });
						}
					}

					if (next->ceilHeight < sector->ceilHeight)
					{
						// top
						s32 curTexId = getPartTextureIndex(wall, HP_TOP);
						if (texId < 0 || curTexId == texId)
						{
							if (selection_surface(SA_ADD, sector, nextIndex, HP_TOP))
							{
								hasMatch = true;
								if (texOffset)
								{
									setPartTextureOffset(wall, HP_TOP, { offsetX, offsetZ + next->ceilHeight - baseHeight });
								}
							}
						}
					}
					if (next->floorHeight > sector->floorHeight)
					{
						// bottom
						s32 curTexId = getPartTextureIndex(wall, HP_BOT);
						if (texId < 0 || curTexId == texId)
						{
							if (selection_surface(SA_ADD, sector, nextIndex, HP_BOT))
							{
								hasMatch = true;
								if (texOffset)
								{
									setPartTextureOffset(wall, HP_BOT, { offsetX, offsetZ + sector->floorHeight - baseHeight });
								}
							}
						}
					}
					if (wall->flags[0] & WF1_ADJ_MID_TEX)
					{
						// mid
						s32 curTexId = getPartTextureIndex(wall, HP_MID);
						if (texId < 0 || curTexId == texId)
						{
							if (selection_surface(SA_ADD, sector, nextIndex, HP_MID))
							{
								hasMatch = true;
								if (texOffset)
								{
									setPartTextureOffset(wall, HP_MID, { offsetX, offsetZ + sector->floorHeight - baseHeight });
								}
							}
						}
					}

					if (!hasMatch)
					{
						break;
					}
					else
					{
						// Add adjoin to the list.
						// Then go left and right from the adjoin.
						if (layer == LAYER_ANY || next->layer == layer)
						{
							if (texOffset)
							{
								addToAdjoinList({ wall->adjoinId, wall->mirrorId, !dir, { texOffset[0], texOffset[1] } });
							}
							else
							{
								addToAdjoinList({ wall->adjoinId, wall->mirrorId, !dir, {0} });
							}
						}
						if (texOffset)
						{
							if (dir == 1) { texOffset[dir].x -= getWallLength(sector, wall); }
							if (dir == 0) { texOffset[dir].x += getWallLength(sector, wall); }
						}
					}
				}

				nextIndex = dir == 0 ? getRightWall(sector, nextIndex) : getLeftWall(sector, nextIndex);
			}
		}
	}
		
	void selectSimilarWalls(EditorSector* rootSector, s32 wallIndex, HitPart part, bool autoAlign/*=false*/)
	{
		s32 rootWallIndex = wallIndex;
		EditorWall* rootWall = &rootSector->walls[wallIndex];
		s32 texId = -1;
		if (s_sectorDrawMode == SDM_TEXTURED_CEIL || s_sectorDrawMode == SDM_TEXTURED_FLOOR)
		{
			texId = getPartTextureIndex(rootWall, part);
		}

		f32 leftAlign = 0.0f;
		f32 rightAlign = autoAlign ? getWallLength(rootSector, rootWall) : 0.0f;
		Vec2f texOffsetLeft = getPartTextureOffset(rootWall, part);
		Vec2f texOffset[2] = { { texOffsetLeft.x + rightAlign, texOffsetLeft.z }, texOffsetLeft };
		f32 baseHeight = rootSector->floorHeight;
		if (part == HP_TOP)
		{
			EditorSector* next = &s_level.sectors[rootWall->adjoinId];
			baseHeight = next->ceilHeight;
		}

		// Add the matching parts from the start wall.
		if (rootWall->adjoinId < 0)
		{
			s32 curTexId = getPartTextureIndex(rootWall, HP_MID);
			if (texId < 0 || curTexId == texId)
			{
				selection_surface(SA_ADD, rootSector, wallIndex, HP_MID);
			}
		}
		else
		{
			EditorSector* next = &s_level.sectors[rootWall->adjoinId];
			if (next->ceilHeight < rootSector->ceilHeight)
			{
				s32 curTexId = getPartTextureIndex(rootWall, HP_TOP);
				if (texId < 0 || curTexId == texId)
				{
					selection_surface(SA_ADD, rootSector, wallIndex, HP_TOP);
					if (autoAlign) { setPartTextureOffset(rootWall, HP_TOP, { texOffset[1].x, texOffset[1].z + next->ceilHeight - baseHeight }); }
				}
			}
			if (next->floorHeight > rootSector->floorHeight)
			{
				s32 curTexId = getPartTextureIndex(rootWall, HP_BOT);
				if (texId < 0 || curTexId == texId)
				{
					selection_surface(SA_ADD, rootSector, wallIndex, HP_BOT);
					if (autoAlign) { setPartTextureOffset(rootWall, HP_BOT, { texOffset[1].x, texOffset[1].z + rootSector->floorHeight - baseHeight }); }
				}
			}
			if (rootWall->flags[0] & WF1_ADJ_MID_TEX)
			{
				s32 curTexId = getPartTextureIndex(rootWall, HP_MID);
				if (texId < 0 || curTexId == texId)
				{
					selection_surface(SA_ADD, rootSector, wallIndex, HP_MID);
					if (autoAlign) { setPartTextureOffset(rootWall, HP_MID, { texOffset[1].x, texOffset[1].z + rootSector->floorHeight - baseHeight }); }
				}
			}
		}

		s_adjoinList.clear();
		selectSimilarTraverse(rootSector, wallIndex, texId, -1, autoAlign ? texOffset : nullptr, baseHeight);

		// Only traverse adjoins if using textures as a separator.
		// Otherwise we often select too many walls (most or all of the level).
		if (texId >= 0)
		{
			size_t adjoinIndex = 0;
			while (adjoinIndex < s_adjoinList.size())
			{
				EditorSector* sector = &s_level.sectors[s_adjoinList[adjoinIndex].adjoinId];
				Vec2f adjOffset[] = { s_adjoinList[adjoinIndex].offset[0], s_adjoinList[adjoinIndex].offset[1] };
				selectSimilarTraverse(sector, s_adjoinList[adjoinIndex].mirrorId, texId, s_adjoinList[adjoinIndex].side, autoAlign ? adjOffset : nullptr, baseHeight);
				adjoinIndex++;
			}
		}
	}

	// We are looking for other flats of the same height, texture, and part.
	void selectSimilarFlats(EditorSector* rootSector, HitPart part)
	{
		s_searchKey++;
		s_sectorChangeList.clear();
		s_sectorChangeList.push_back(rootSector);

		s32 texId = -1;
		if (s_sectorDrawMode == SDM_TEXTURED_CEIL || s_sectorDrawMode == SDM_TEXTURED_FLOOR)
		{
			texId = part == HP_FLOOR ? rootSector->floorTex.texIndex : rootSector->ceilTex.texIndex;
		}

		while (!s_sectorChangeList.empty())
		{
			EditorSector* sector = s_sectorChangeList.back();
			s_sectorChangeList.pop_back();

			selection_surface(SA_ADD, sector, 0, part);

			const s32 wallCount = (s32)sector->walls.size();
			const EditorWall* wall = sector->walls.data();
			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				if (wall->adjoinId < 0) { continue; }
				EditorSector* next = &s_level.sectors[wall->adjoinId];
				if (next->searchKey != s_searchKey)
				{
					next->searchKey = s_searchKey;
					if ((part == HP_FLOOR && next->floorHeight == rootSector->floorHeight) || (part == HP_CEIL && next->ceilHeight == rootSector->ceilHeight))
					{
						bool texMatches = true;
						if (texId >= 0)
						{
							s32 curTexId = part == HP_FLOOR ? next->floorTex.texIndex : next->ceilTex.texIndex;
							texMatches = curTexId == texId;
						}
						if (texMatches)
						{
							s_sectorChangeList.push_back(next);
						}
					}
				}
			}
		}
	}

	void snapSignToCursor(EditorSector* sector, EditorWall* wall, s32 signTexIndex, Vec2f* signOffset)
	{
		EditorTexture* signTex = (EditorTexture*)getAssetData(s_level.textures[signTexIndex].handle);
		if (!signTex) { return; }

		// Center the sign on the mouse cursor.
		const Vec2f* v0 = &sector->vtx[wall->idx[0]];
		const Vec2f* v1 = &sector->vtx[wall->idx[1]];
		Vec2f wallDir = { v1->x - v0->x, v1->z - v0->z };
		wallDir = TFE_Math::normalize(&wallDir);

		f32 uOffset = wall->tex[WP_MID].offset.x;
		f32 vOffset = sector->floorHeight;
		if (wall->adjoinId >= 0)
		{
			const EditorSector* next = &s_level.sectors[wall->adjoinId];
			if (next->floorHeight > sector->floorHeight)
			{
				uOffset = wall->tex[WP_BOT].offset.x;
			}
			else if (next->ceilHeight < sector->ceilHeight)
			{
				uOffset = wall->tex[WP_TOP].offset.x;
				vOffset = next->ceilHeight;
			}
		}

		Vec3f mousePos = s_cursor3d;
		snapToSurfaceGrid(sector, wall, mousePos);

		f32 wallIntersect;
		if (fabsf(wallDir.x) >= fabsf(wallDir.z)) { wallIntersect = (mousePos.x - v0->x) / wallDir.x; }
		else { wallIntersect = (mousePos.z - v0->z) / wallDir.z; }

		signOffset->x = uOffset + wallIntersect - 0.5f*signTex->width / 8.0f;
		signOffset->z = vOffset - mousePos.y + 0.5f*signTex->height / 8.0f;
	}
}
