#include "levelEditor.h"
#include "levelEditorData.h"
#include "selection.h"
#include "infoPanel.h"
#include "browser.h"
#include "camera.h"
#include "sharedState.h"
#include <TFE_FrontEndUI/frontEndUi.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/editorLevel.h>
#include <TFE_Editor/editorProject.h>
#include <TFE_Editor/editorResources.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/EditorAsset/editorAsset.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>
#include <TFE_Editor/LevelEditor/Rendering/grid2d.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderShared/lineDraw2d.h>
#include <TFE_RenderShared/lineDraw3d.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Polygon/polygon.h>
#include <TFE_System/parser.h>
#include <TFE_System/math.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;
#define SHOW_EDITOR_FPS 0

namespace LevelEditor
{
	const f32 c_defaultZoom = 0.25f;
	const f32 c_defaultYaw = PI;
	const f32 c_defaultCameraHeight = 6.0f;

	// The TFE Level Editor format is different than the base format and contains extra 
	// metadata, etc.
	// So existing levels need to be loaded into that format.
	// If the correct format already exists, though, then it is loaded directly.
	EditorLevel s_level = {};
	AssetList s_levelTextureList;
	LevelEditMode s_editMode = LEDIT_DRAW;
	
	u32 s_editFlags = LEF_DEFAULT;
	u32 s_lwinOpen = LWIN_NONE;
	s32 s_curLayer = 0;
			
	// Sector
	EditorSector* s_hoveredSector = nullptr;
	EditorSector* s_selectedSector = nullptr;

	// Vertex
	EditorSector* s_hoveredVtxSector = nullptr;
	EditorSector* s_lastHoveredVtxSector = nullptr;
	EditorSector* s_selectedVtxSector = nullptr;
	s32 s_hoveredVtxId = -1;
	s32 s_selectedVtxId = -1;
	Vec3f s_hoveredVtxPos;
	Vec3f s_selectedVtxPos;

	// Wall
	EditorSector* s_hoveredWallSector = nullptr;
	EditorSector* s_lastHoveredWallSector = nullptr;
	EditorSector* s_selectedWallSector = nullptr;
	s32 s_hoveredWallId = -1;
	s32 s_selectedWallId = -1;
	// 3D Selection
	HitPart s_hoveredWallPart = HP_COUNT;
	HitPart s_selectedWallPart = HP_COUNT;

	static bool s_editMove = false;
		
	static EditorView s_view = EDIT_VIEW_2D;
	static Vec2i s_editWinPos = { 0, 69 };
	static Vec2i s_editWinSize = { 0 };
	static Vec2f s_editWinMapCorner = { 0 };
	static Vec3f s_rayDir = { 0.0f, 0.0f, 1.0f };
	static f32 s_zoom = c_defaultZoom;
	static bool s_uiActive = false;
		
	static char s_layerMem[4 * 31];
	static char* s_layerStr[31];
		
	static s32 s_gridIndex = 4;
	static f32 c_gridSizeMap[] =
	{
		0.0f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f
	};
	static const char* c_gridSizes[] =
	{
		"Off",
		"1/64",
		"1/32",
		"1/16",
		"1/8",
		"1/4",
		"1/2",
		"1",
		"2",
		"4",
		"8",
		"16",
		"32",
		"64",
	};
	
	static TextureGpu* s_editCtrlToolbarData = nullptr;

	////////////////////////////////////////////////////////
	// Forward Declarations
	////////////////////////////////////////////////////////
	void toolbarBegin();
	void toolbarEnd();
	bool drawToolbarButton(void* ptr, u32 imageId, bool highlight);
	void levelEditWinBegin();
	void levelEditWinEnd();
	void messagePanel(ImVec2 pos);
	void cameraControl2d(s32 mx, s32 my);
	void cameraControl3d(s32 mx, s32 my);
	void resetZoom();
	Vec2f mouseCoordToWorldPos2d(s32 mx, s32 my);
	Vec3f mouseCoordToWorldDir3d(s32 mx, s32 my);
	Vec2i worldPos2dToMap(const Vec2f& worldPos);
	bool isUiActive();
	bool isViewportElementHovered();
	TextureGpu* loadGpuImage(const char* path);

	void selectFromSingleVertex(EditorSector* root, s32 featureId, bool clearList);
	void toggleVertexGroupInclusion(EditorSector* sector, s32 featureId);
		
	////////////////////////////////////////////////////////
	// Public API
	////////////////////////////////////////////////////////
	bool init(Asset* asset)
	{
		// Reset output messages.
		infoPanelClearMessages();
		infoPanelSetMsgFilter();

		// Cleanup any existing level data.
		destroy();
		// Load the new level.
		if (!loadLevelFromAsset(asset, &s_level))
		{
			return false;
		}
		infoPanelAddMsg(LE_MSG_INFO, "Loaded level '%s'", s_level.name.c_str());

		viewport_init();
		viewport_update((s32)UI_SCALE(480) + 16, (s32)UI_SCALE(68) + 18);
		s_gridIndex = 4;
		s_gridSize = c_gridSizeMap[s_gridIndex];

		s_editCtrlToolbarData = loadGpuImage("UI_Images/EditCtrl_32x6.png");
		if (s_editCtrlToolbarData)
		{
			infoPanelAddMsg(LE_MSG_INFO, "Loaded toolbar images 'UI_Images/EditCtrl_32x6.png'");
		}
		else
		{
			infoPanelAddMsg(LE_MSG_ERROR, "Failed to load toolbar images 'UI_Images/EditCtrl_32x6.png'");
		}

		u32 idx = 0;
		for (s32 i = -15; i < 16; i++, idx += 4)
		{
			s_layerStr[i + 15] = &s_layerMem[idx];
			sprintf(s_layerStr[i + 15], "%d", i);
		}

		s_viewportPos = { -24.0f, 0.0f, -200.0f };
		s_curLayer = std::min(1, s_level.layerRange[1]);

		s_sectorDrawMode = SDM_WIREFRAME;
		s_hoveredSector         = nullptr;
		s_selectedSector        = nullptr;
		s_hoveredVtxSector      = nullptr;
		s_selectedVtxSector     = nullptr;
		s_lastHoveredVtxSector  = nullptr;
		s_hoveredWallSector     = nullptr;
		s_lastHoveredWallSector = nullptr;
		s_selectedWallSector    = nullptr;
		s_hoveredVtxId          = -1;
		s_hoveredWallId         = -1;
		s_editMove = false;

		AssetBrowser::getLevelTextures(s_levelTextureList, asset->name.c_str());
		s_camera = { 0 };
		s_camera.pos.y = c_defaultCameraHeight;
		s_camera.yaw = c_defaultYaw;
		computeCameraTransform();
		s_cursor3d = { 0 };

		TFE_RenderShared::init(false);
		return true;
	}

	void destroy()
	{
		s_level.sectors.clear();
		viewport_destroy();
		TFE_RenderShared::destroy();

		TFE_RenderBackend::freeTexture(s_editCtrlToolbarData);
		s_editCtrlToolbarData = nullptr;
	}

	bool isPointInsideSector2d(EditorSector* sector, Vec2f pos, s32 layer)
	{
		// The layers need to match.
		if (sector->layer != layer) { return false; }
		// The point has to be within the bounding box.
		if (pos.x < sector->bounds[0].x || pos.x > sector->bounds[1].x ||
			pos.z < sector->bounds[0].z || pos.z > sector->bounds[1].z)
		{
			return false;
		}
		return TFE_Polygon::pointInsidePolygon(&sector->poly, pos);
	}

	EditorSector* findSector2d(Vec2f pos, s32 layer)
	{
		const size_t sectorCount = s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		for (size_t s = 0; s < sectorCount; s++, sector++)
		{
			if (isPointInsideSector2d(sector, pos, layer))
			{
				return sector;
			}
		}
		return nullptr;
	}

	// Find the closest point to p2 on line segment p0 -> p1 as a parametric value on the segment.
	// Fills in point with the point itself.
	f32 closestPointOnLineSegment(Vec2f p0, Vec2f p1, Vec2f p2, Vec2f* point)
	{
		const Vec2f r = { p2.x - p0.x, p2.z - p0.z };
		const Vec2f d = { p1.x - p0.x, p1.z - p0.z };
		const f32 denom = d.x * d.x + d.z * d.z;
		if (fabsf(denom) < FLT_EPSILON) { return 0.0f; }

		const f32 s = std::max(0.0f, std::min(1.0f, (r.x * d.x + r.z * d.z) / denom));
		point->x = p0.x + s * d.x;
		point->z = p0.z + s * d.z;
		return s;
	}

	s32 findClosestWallInSector(const EditorSector* sector, const Vec2f* pos, f32 maxDistSq, f32* minDistToWallSq)
	{
		const u32 count = (u32)sector->walls.size();
		f32 minDistSq = FLT_MAX;
		s32 closestId = -1;
		const EditorWall* walls = sector->walls.data();
		const Vec2f* vertices = sector->vtx.data();
		for (u32 w = 0; w < count; w++)
		{
			const Vec2f* v0 = &vertices[walls[w].idx[0]];
			const Vec2f* v1 = &vertices[walls[w].idx[1]];

			Vec2f pointOnSeg;
			closestPointOnLineSegment(*v0, *v1, *pos, &pointOnSeg);
			const Vec2f diff = { pointOnSeg.x - pos->x, pointOnSeg.z - pos->z };
			const f32 distSq = diff.x*diff.x + diff.z*diff.z;

			if (distSq < maxDistSq && distSq < minDistSq && (!minDistToWallSq || distSq < *minDistToWallSq))
			{
				minDistSq = distSq;
				closestId = s32(w);
			}
		}
		if (minDistToWallSq)
		{
			*minDistToWallSq = std::min(*minDistToWallSq, minDistSq);
		}
		return closestId;
	}

	bool isUiActive()
	{
		return getMenuActive() || s_uiActive;
	}

	bool isViewportElementHovered()
	{
		return (s_hoveredVtxId >= 0 && s_editMode == LEDIT_VERTEX) || (s_hoveredWallId >= 0 && s_editMode == LEDIT_WALL) || 
			   (s_hoveredSector && s_editMode == LEDIT_SECTOR) || (s_selectedVtxId >= 0 && s_editMode == LEDIT_VERTEX) || 
			   (s_selectedWallId >= 0 && s_editMode == LEDIT_WALL) || (s_selectedSector && s_editMode == LEDIT_SECTOR);
	}

	Vec3f rayGridPlaneHit(const Vec3f& origin, const Vec3f& rayDir)
	{
		Vec3f hit = { 0 };
		if (fabsf(rayDir.y) < FLT_EPSILON) { return hit; }

		f32 s = (s_gridHeight - origin.y) / rayDir.y;
		if (s <= 0) { return hit; }

		hit.x = origin.x + s * rayDir.x;
		hit.y = origin.y + s * rayDir.y;
		hit.z = origin.z + s * rayDir.z;
		return hit;
	}

	void adjustGridHeight(EditorSector* sector)
	{
		if (!sector) { return; }
		s_gridHeight = sector->floorHeight;
	}

	void handleHoverAndSelection(RayHitInfo* info)
	{
		if (info->hitSectorId < 0) { return; }
		s_hoveredSector = &s_level.sectors[info->hitSectorId];

		// TODO: Move to central hotkey list.
		if (s_hoveredSector && TFE_Input::keyModDown(KEYMOD_CTRL) && TFE_Input::keyPressed(KEY_G))
		{
			adjustGridHeight(s_hoveredSector);
		}

		//////////////////////////////////////
		// SECTOR
		//////////////////////////////////////
		if (s_editMode == LEDIT_SECTOR && TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
		{
			s_selectedSector = s_hoveredSector;
			adjustGridHeight(s_selectedSector);
		}

		//////////////////////////////////////
		// VERTEX
		//////////////////////////////////////
		if (s_editMode == LEDIT_VERTEX)
		{
			s32 prevId = s_hoveredVtxId;
			// See if we are close enough to "hover" a vertex
			s_hoveredVtxSector = nullptr;
			s_hoveredVtxId = -1;
			if (s_hoveredSector)
			{
				// Keep track of the last vertex hovered sector and use it if no hovered sector is active to
				// make selecting vertices less fiddly.
				EditorSector* hoveredSector = s_hoveredSector;

				const size_t vtxCount = hoveredSector->vtx.size();
				const Vec2f* vtx = hoveredSector->vtx.data();
				const Vec3f* refPos = &info->hitPos;

				const f32 distFromCam = TFE_Math::distance(&info->hitPos, &s_camera.pos);
				const f32 maxDist = distFromCam * 64.0f / f32(s_viewportSize.z);
				const f32 floor = hoveredSector->floorHeight;
				const f32 ceil  = hoveredSector->ceilHeight;

				f32 closestDistSq = maxDist * maxDist;
				s32 closestVtx = -1;
				HitPart closestPart = HP_FLOOR;
				Vec3f finalPos;
				for (size_t v = 0; v < vtxCount; v++)
				{
					// Check against the floor and ceiling vertex of each vertex.
					const Vec3f floorVtx = { vtx[v].x, floor, vtx[v].z };
					const Vec3f ceilVtx  = { vtx[v].x, ceil,  vtx[v].z };
					const f32 floorDistSq = TFE_Math::distanceSq(&floorVtx, refPos);
					const f32 ceilDistSq  = TFE_Math::distanceSq(&ceilVtx,  refPos);

					if (floorDistSq < closestDistSq || ceilDistSq < closestDistSq)
					{
						closestDistSq = min(floorDistSq, ceilDistSq);
						closestPart = floorDistSq <= ceilDistSq ? HP_FLOOR : HP_CEIL;
						closestVtx = v;
						finalPos = floorDistSq <= ceilDistSq ? floorVtx : ceilVtx;
					}
				}

				if (closestVtx >= 0)
				{
					s_hoveredVtxSector = hoveredSector;
					s_hoveredVtxId = closestVtx;
					s_hoveredVtxPos = finalPos;
				}
			}

			if (TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
			{
				if (TFE_Input::keyModDown(KEYMOD_CTRL))
				{
					if (s_hoveredVtxSector && s_hoveredVtxId >= 0)
					{
						s_editMove = true;
						adjustGridHeight(s_hoveredVtxSector);
						toggleVertexGroupInclusion(s_hoveredVtxSector, s_hoveredVtxId);
					}
				}
				else
				{
					s_selectedVtxSector = nullptr;
					s_selectedVtxId = -1;
					if (s_hoveredVtxSector && s_hoveredVtxId >= 0)
					{
						s_selectedVtxSector = s_hoveredVtxSector;
						s_selectedVtxId = s_hoveredVtxId;
						s_selectedVtxPos = s_hoveredVtxPos;
						adjustGridHeight(s_selectedVtxSector);
						s_editMove = true;

						// Clear the selection if this is a new vertex and Ctrl isn't held.
						bool clearList = !selection_doesFeatureExist(createFeatureId(s_selectedVtxSector, s_selectedVtxId));
						selectFromSingleVertex(s_selectedVtxSector, s_selectedVtxId, clearList);
					}
				}
			}
		}
		else
		{
			s_hoveredVtxSector = nullptr;
			s_selectedVtxSector = nullptr;
			s_hoveredVtxId = -1;
			s_selectedVtxId = -1;
			selection_clear();
		}

		//////////////////////////////////////
		// WALL
		//////////////////////////////////////
		if (s_editMode == LEDIT_WALL)
		{
			// See if we are close enough to "hover" a vertex
			s_hoveredWallSector = nullptr;
			s_hoveredWallId = -1;
			if (info->hitWallId >= 0)
			{
				s_hoveredWallSector = s_hoveredSector;
				s_hoveredWallId = info->hitWallId;
				s_hoveredWallPart = info->hitPart;
			}
			else
			{
				// Are we close enough to an edge?
				// Project the point onto the floor.
				const Vec2f pos2d = { info->hitPos.x, info->hitPos.z };
				const f32 distFromCam = TFE_Math::distance(&info->hitPos, &s_camera.pos);
				const f32 maxDist = distFromCam * 16.0f / f32(s_viewportSize.z);
				const f32 maxDistSq = maxDist * maxDist;
				s_hoveredWallId = findClosestWallInSector(s_hoveredSector, &pos2d, maxDist * maxDist, nullptr);
				if (s_hoveredWallId >= 0)
				{
					s_hoveredWallSector = s_hoveredSector;
					EditorWall* wall = &s_hoveredWallSector->walls[s_hoveredWallId];
					EditorSector* next = wall->adjoinId >= 0 ? &s_level.sectors[wall->adjoinId] : nullptr;
					if (!next)
					{
						s_hoveredWallPart = HP_MID;
					}
					else if (info->hitPart == HP_FLOOR)
					{
						s_hoveredWallPart = (next->floorHeight > s_hoveredWallSector->floorHeight) ? HP_BOT : HP_MID;
					}
					else if (info->hitPart == HP_CEIL)
					{
						s_hoveredWallPart = (next->ceilHeight < s_hoveredWallSector->ceilHeight) ? HP_TOP : HP_MID;
					}
				}
				else
				{
					// Otherwise, select the floor or ceiling...
					if (info->hitPart == HP_FLOOR)
					{
						s_hoveredWallPart = HP_FLOOR;
					}
					else if (info->hitPart == HP_CEIL)
					{
						s_hoveredWallPart = HP_CEIL;
					}
				}
			}

			if (TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
			{
				s_selectedWallSector = nullptr;
				s_selectedSector = nullptr;
				s_selectedWallId = -1;
				if (s_hoveredWallSector && s_hoveredWallId >= 0)
				{
					s_selectedWallSector = s_hoveredWallSector;
					s_selectedWallId = s_hoveredWallId;
					s_selectedWallPart = s_hoveredWallPart;
					adjustGridHeight(s_selectedWallSector);
				}
				else if (s_hoveredSector && (s_hoveredWallPart == HP_FLOOR || s_hoveredWallPart == HP_CEIL))
				{
					s_selectedSector = s_hoveredSector;
					s_selectedWallPart = s_hoveredWallPart;
					adjustGridHeight(s_selectedSector);
				}
			}
		}
		else
		{
			s_hoveredWallSector = nullptr;
			s_selectedWallSector = nullptr;
			s_hoveredWallId = -1;
			s_selectedWallId = -1;
			s_hoveredWallPart = HP_COUNT;
			s_selectedWallPart = HP_COUNT;
		}

		if (s_editMode != LEDIT_SECTOR && (s_editMode != LEDIT_WALL || (s_hoveredWallPart != HP_FLOOR && s_hoveredWallPart != HP_CEIL)))
		{
			s_hoveredSector = nullptr;
		}
	}
		
	void updateWindowControls()
	{
		if (isUiActive()) { return; }

		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		if (!TFE_Input::relativeModeEnabled() && (mx < s_editWinPos.x || mx >= s_editWinPos.x + s_editWinSize.x || my < s_editWinPos.z || my >= s_editWinPos.z + s_editWinSize.z))
		{
			// Nothing is "hovered" if the mouse is not in the window.
			s_hoveredSector = nullptr;
			s_hoveredVtxSector = nullptr;
			s_lastHoveredVtxSector = nullptr;
			s_hoveredWallSector = nullptr;
			s_lastHoveredWallSector = nullptr;
			s_hoveredVtxId = -1;
			s_hoveredWallId = -1;
			return;
		}

		if (s_view == EDIT_VIEW_2D)
		{
			cameraControl2d(mx, my);

			// Selection
			Vec2f worldPos = mouseCoordToWorldPos2d(mx, my);
			if (s_editMode != LEDIT_DRAW)
			{
				// First check to see if the current hovered sector is still valid.
				if (s_hoveredSector)
				{
					if (!isPointInsideSector2d(s_hoveredSector, worldPos, s_curLayer))
					{
						s_hoveredSector = nullptr;
					}
				}
				// If not, then try to find one.
				if (!s_hoveredSector)
				{
					s_hoveredSector = findSector2d(worldPos, s_curLayer);
				}
				// TODO: Move to central hotkey list.
				if (s_hoveredSector && TFE_Input::keyModDown(KEYMOD_CTRL) && TFE_Input::keyPressed(KEY_G))
				{
					adjustGridHeight(s_hoveredSector);
				}
				
				if (s_editMode == LEDIT_SECTOR && TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
				{
					s_selectedSector = s_hoveredSector;
					adjustGridHeight(s_selectedSector);
				}
				else if (s_editMode != LEDIT_SECTOR)
				{
					s_selectedSector = nullptr;
				}

				if (s_editMode == LEDIT_VERTEX)
				{
					// See if we are close enough to "hover" a vertex
					s_hoveredVtxSector = nullptr;
					s_hoveredVtxId = -1;
					if (s_hoveredSector || s_lastHoveredVtxSector)
					{
						// Keep track of the last vertex hovered sector and use it if no hovered sector is active to
						// make selecting vertices less fiddly.
						EditorSector* hoveredSector = s_hoveredSector ? s_hoveredSector : s_lastHoveredVtxSector;

						const size_t vtxCount = hoveredSector->vtx.size();
						const Vec2f* vtx = hoveredSector->vtx.data();

						f32 closestDistSq = FLT_MAX;
						s32 closestVtx = -1;
						for (size_t v = 0; v < vtxCount; v++, vtx++)
						{
							Vec2f offset = { worldPos.x - vtx->x, worldPos.z - vtx->z };
							f32 distSq = offset.x*offset.x + offset.z*offset.z;
							if (distSq < closestDistSq)
							{
								closestDistSq = distSq;
								closestVtx = (s32)v;
							}
						}

						const f32 maxDist = s_zoom2d * 16.0f;
						if (closestDistSq <= maxDist*maxDist)
						{
							s_hoveredVtxSector = hoveredSector;
							s_lastHoveredVtxSector = hoveredSector;
							s_hoveredVtxId = closestVtx;
						}
					}

					if (TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
					{
						if (TFE_Input::keyModDown(KEYMOD_CTRL))
						{
							if (s_hoveredVtxSector && s_hoveredVtxId >= 0)
							{
								s_editMove = true;
								adjustGridHeight(s_hoveredVtxSector);
								toggleVertexGroupInclusion(s_hoveredVtxSector, s_hoveredVtxId);
							}
						}
						else
						{
							s_selectedVtxSector = nullptr;
							s_selectedVtxId = -1;
							if (s_hoveredVtxSector && s_hoveredVtxId >= 0)
							{
								s_selectedVtxSector = s_hoveredVtxSector;
								s_selectedVtxId = s_hoveredVtxId;
								adjustGridHeight(s_selectedVtxSector);
								s_editMove = true;

								// Clear the selection if this is a new vertex and Ctrl isn't held.
								bool clearList = !selection_doesFeatureExist(createFeatureId(s_selectedVtxSector, s_selectedVtxId));
								selectFromSingleVertex(s_selectedVtxSector, s_selectedVtxId, clearList);
							}
						}
					}
				}
				else
				{
					s_hoveredVtxSector = nullptr;
					s_selectedVtxSector = nullptr;
					s_hoveredVtxId = -1;
					s_selectedVtxId = -1;
					selection_clear();
				}

				if (s_editMode == LEDIT_WALL)
				{
					// See if we are close enough to "hover" a wall
					s_hoveredWallSector = nullptr;
					s_hoveredWallId = -1;
					if (s_hoveredSector || s_lastHoveredWallSector)
					{
						// Keep track of the last vertex hovered sector and use it if no hovered sector is active to
						// make selecting vertices less fiddly.
						EditorSector* hoveredSector = s_hoveredSector ? s_hoveredSector : s_lastHoveredWallSector;

						const f32 maxDist = s_zoom2d * 16.0f;
						s_hoveredWallId = findClosestWallInSector(hoveredSector, &worldPos, maxDist * maxDist, nullptr);
						if (s_hoveredWallId >= 0)
						{
							s_hoveredWallSector = hoveredSector;
							s_lastHoveredWallSector = hoveredSector;
						}
						else
						{
							s_hoveredWallSector = nullptr;
						}
					}

					if (TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
					{
						s_selectedWallSector = nullptr;
						s_selectedWallId = -1;
						if (s_hoveredWallSector && s_hoveredWallId >= 0)
						{
							s_selectedWallSector = s_hoveredWallSector;
							s_selectedWallId = s_hoveredWallId;
							adjustGridHeight(s_selectedWallSector);
						}
					}
				}
				else
				{
					s_hoveredWallSector = nullptr;
					s_selectedWallSector = nullptr;
					s_hoveredWallId = -1;
					s_selectedWallId = -1;
				}

				if (s_editMode != LEDIT_SECTOR)
				{
					s_hoveredSector = nullptr;
				}

				// DEBUG
				if (s_selectedSector && TFE_Input::keyPressed(KEY_T))
				{
					TFE_Polygon::computeTriangulation(&s_selectedSector->poly);
				}
			}
		}
		else if (s_view == EDIT_VIEW_3D)
		{
			cameraControl3d(mx, my);

			s_hoveredSector = nullptr;
			s_hoveredWallId = -1;
			s_hoveredWallSector = nullptr;
			s_hoveredVtxId = -1;
			s_hoveredVtxSector = nullptr;

			// TODO: Move out to common place for hotkeys.
			bool hitBackfaces = TFE_Input::keyDown(KEY_B);

			// Trace a ray through the mouse cursor.
			s_rayDir = mouseCoordToWorldDir3d(mx, my);
			RayHitInfo hitInfo;
			Ray ray = { s_camera.pos, s_rayDir, 1000.0f, s_curLayer };
			if (traceRay(&ray, &s_level, &hitInfo, hitBackfaces))
			{
				s_cursor3d = hitInfo.hitPos;
				if (s_editMode != LEDIT_DRAW)
				{
					handleHoverAndSelection(&hitInfo);
				}
			}
			else
			{
				s_hoveredSector = nullptr;
				s_cursor3d = rayGridPlaneHit(s_camera.pos, s_rayDir);

				if (TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
				{
					s_selectedVtxSector = nullptr;
					s_selectedWallSector = nullptr;
					s_selectedSector = nullptr;
					s_selectedWallId = -1;
					s_selectedVtxId = -1;
					selection_clear();
				}
			}
		}
	}

	bool menu()
	{
		bool menuActive = false;
		if (ImGui::BeginMenu("Level"))
		{
			menuActive = true;
			// Disable Save/Backup when there is no project.
			bool projectActive = project_get()->active;
			if (!projectActive) { disableNextItem(); }
			if (ImGui::MenuItem("Save", "Ctrl+S", (bool*)NULL))
			{
				// Save the current state of the level to disk.
			}
			if (ImGui::MenuItem("Snapshot", "Ctrl+N", (bool*)NULL))
			{
				// Bring up a pop-up where the snapshot can be named.
			}
			if (!projectActive) { enableNextItem(); }

			if (ImGui::MenuItem("Close", NULL, (bool*)NULL))
			{
				// TODO: If the level has changed, pop up a warning and allow the level to be saved.
				disableAssetEditor();
			}
			ImGui::Separator();
			// TODO: Add GOTO option (to go to a sector or other object).
			if (ImGui::MenuItem("Undo", "Ctrl+Z", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("History View", NULL, (s_lwinOpen & LWIN_HISTORY) != 0))
			{
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "Ctrl+X", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Copy", "Ctrl+C", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Paste", "Ctrl+V", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Duplicate", "Ctrl+D", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Delete", "Del", (bool*)NULL))
			{
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			menuActive = true;
			if (ImGui::MenuItem("2D", "Ctrl+1", s_view == EDIT_VIEW_2D))
			{
				if (s_view == EDIT_VIEW_3D)
				{
					// Center the 2D map on the 3D camera position before changing.
					Vec2f mapPos = { s_camera.pos.x * c_defaultZoom / s_zoom2d, -s_camera.pos.z * c_defaultZoom / s_zoom2d };
					s_viewportPos.x = mapPos.x - (s_viewportSize.x / 2) * s_zoom2d;
					s_viewportPos.z = mapPos.z - (s_viewportSize.z / 2) * s_zoom2d;
				}
				s_view = EDIT_VIEW_2D;
			}
			if (ImGui::MenuItem("3D (Editor)", "Ctrl+2", s_view == EDIT_VIEW_3D))
			{
				// Put the 3D camera at the center of the 2D map.
				if (s_view == EDIT_VIEW_2D)
				{
					Vec2f worldPos = mouseCoordToWorldPos2d(s_viewportSize.x / 2 + s_editWinMapCorner.x, s_viewportSize.z / 2 + s_editWinMapCorner.z);
					s_camera.pos.x = worldPos.x;
					s_camera.pos.z = worldPos.z;
					computeCameraTransform();
				}
				s_view = EDIT_VIEW_3D;
			}
			if (ImGui::MenuItem("3D (Game)", "Ctrl+3", s_view == EDIT_VIEW_3D_GAME))
			{
				// TODO
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Reset Zoom", nullptr, (bool*)NULL))
			{
				resetZoom();
			}

			bool showLower = (s_editFlags & LEF_SHOW_LOWER_LAYERS) != 0;
			if (ImGui::MenuItem("Show Lower Layers", "Ctrl+L", showLower))
			{
				showLower = !showLower;
				if (showLower) { s_editFlags |= LEF_SHOW_LOWER_LAYERS;  }
				else { s_editFlags &= ~LEF_SHOW_LOWER_LAYERS; }
			}

			bool showAllEdges = (s_editFlags & LEF_SECTOR_EDGES) != 0;
			if (ImGui::MenuItem("Show All Sector Edges", "Ctrl+E", showAllEdges))
			{
				showAllEdges = !showAllEdges;
				if (showAllEdges) { s_editFlags |= LEF_SECTOR_EDGES; }
				else { s_editFlags &= ~LEF_SECTOR_EDGES; }
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Wireframe", "Ctrl+F1", s_sectorDrawMode == SDM_WIREFRAME))
			{
				s_sectorDrawMode = SDM_WIREFRAME;
			}
			if (ImGui::MenuItem("Lighting", "Ctrl+F2", s_sectorDrawMode == SDM_LIGHTING))
			{
				s_sectorDrawMode = SDM_LIGHTING;
			}
			if (ImGui::MenuItem("Textured (Floor)", "Ctrl+F3", s_sectorDrawMode == SDM_TEXTURED_FLOOR))
			{
				s_sectorDrawMode = SDM_TEXTURED_FLOOR;
			}
			if (ImGui::MenuItem("Textured (Ceiling)", "Ctrl+F4", s_sectorDrawMode == SDM_TEXTURED_CEIL))
			{
				s_sectorDrawMode = SDM_TEXTURED_CEIL;
			}
			ImGui::Separator();
			bool fullbright = (s_editFlags & LEF_FULLBRIGHT) != 0;
			if (ImGui::MenuItem("Fullbright", "Ctrl+F5", fullbright))
			{
				fullbright = !fullbright;
				if (fullbright) { s_editFlags |= LEF_FULLBRIGHT; }
				else { s_editFlags &= ~LEF_FULLBRIGHT; }
			}
			ImGui::Separator();
			if (ImGui::MenuItem("View Settings", NULL, (s_lwinOpen & LWIN_VIEW_SETTINGS) != 0))
			{
			}
			ImGui::EndMenu();
		}

		return menuActive;
	}

	void selectNone()
	{
		s_selectedVtxId = -1;
		s_selectedWallId = -1;
		s_selectedSector = nullptr;
		selection_clear();
	}
	
	void update()
	{
		pushFont(TFE_Editor::FONT_SMALL);
		updateWindowControls();

		viewport_update((s32)UI_SCALE(480) + 16, (s32)UI_SCALE(68) + 18);
		viewport_render(s_view);

		// Toolbar
		s_uiActive = false;
		toolbarBegin();
		{
			void* gpuPtr = TFE_RenderBackend::getGpuPtr(s_editCtrlToolbarData);
			if (drawToolbarButton(gpuPtr, 0, false))
			{
				// Play
			}
			ImGui::SameLine(0.0f, 32.0f);

			for (u32 i = 1; i < 6; i++)
			{
				if (drawToolbarButton(gpuPtr, i, i == s_editMode))
				{
					s_editMode = LevelEditMode(i);
					s_editMove = false;
				}
				ImGui::SameLine();
			}

			// Leave Boolean buttons off for now.

			ImGui::SameLine(0.0f, 32.0f);
			ImGui::PushItemWidth(64.0f);
			if (ImGui::BeginCombo("Grid Size", c_gridSizes[s_gridIndex]))
			{
				s_uiActive = true;
				for (int n = 0; n < TFE_ARRAYSIZE(c_gridSizes); n++)
				{
					if (ImGui::Selectable(c_gridSizes[n], n == s_gridIndex))
					{
						s_gridIndex = n;
						s_gridSize = c_gridSizeMap[s_gridIndex];
					}
				}
				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();
			// Get the "Grid Size" combo position to align the message panel later.
			const ImVec2 itemPos = ImGui::GetItemRectMin();

			ImGui::SameLine(0.0f, 32.0f);
			ImGui::PushItemWidth(64.0f);
			s32 layerIndex = s_curLayer - s_level.layerRange[0];
			s32 minLayerIndex = s_level.layerRange[0] + 15;
			s32 count = s_level.layerRange[1] - s_level.layerRange[0] + 1;
			bool showAllLayers = (s_editFlags & LEF_SHOW_ALL_LAYERS) != 0;
			if (ImGui::BeginCombo("Layer", showAllLayers ? "All" : s_layerStr[layerIndex + minLayerIndex]))
			{
				s_uiActive = true;
				for (int n = 0; n < count; n++)
				{
					if (ImGui::Selectable(s_layerStr[n + minLayerIndex], n == layerIndex))
					{
						s_curLayer = n + s_level.layerRange[0];
						showAllLayers = false;
					}
				}
				// Add the "all" option
				if (ImGui::Selectable("All", showAllLayers))
				{
					showAllLayers = true;
				}
				ImGui::EndCombo();
			}
			if (showAllLayers) { s_editFlags |= LEF_SHOW_ALL_LAYERS; }
			else { s_editFlags &= ~LEF_SHOW_ALL_LAYERS; }
			ImGui::PopItemWidth();

			// Message Panel
			messagePanel(itemPos);
		}
		toolbarEnd();

		// Info Panel
		drawInfoPanel();

		// Browser
		drawBrowser();

		// Main viewport view.
		levelEditWinBegin();
		{
			const TextureGpu* texture = viewport_getTexture();
			ImGui::ImageButton(TFE_RenderBackend::getGpuPtr(texture), { (f32)s_viewportSize.x, (f32)s_viewportSize.z },
				{ 0.0f, 0.0f }, { 1.0f, 1.0f }, 0, { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
			const ImVec2 itemPos = ImGui::GetItemRectMin();
			s_editWinMapCorner = { itemPos.x, itemPos.y };

			// Display items on top of the viewport.
			s32 mx, my;
			TFE_Input::getMousePos(&mx, &my);
			const bool editWinHovered = mx >= s_editWinPos.x && mx < s_editWinPos.x + s_editWinSize.x && my >= s_editWinPos.z && my < s_editWinPos.z + s_editWinSize.z;

			if (s_view == EDIT_VIEW_2D && !getMenuActive() && !isUiActive())
			{
				// Display vertex info.
				if (s_hoveredVtxSector && s_hoveredVtxId >= 0 && editWinHovered)
				{
					// Give the "world space" vertex position, get back to the pixel position for the UI.
					const Vec2f vtx = s_hoveredVtxSector->vtx[s_hoveredVtxId];
					const Vec2i mapPos = worldPos2dToMap(vtx);

					const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
						| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;

					ImGui::SetNextWindowPos({ (f32)mapPos.x - UI_SCALE(20), (f32)mapPos.z - UI_SCALE(20) - 16 });
					ImGui::Begin("##VtxInfo", nullptr, window_flags);
					ImGui::Text("%d: %0.3f, %0.3f", s_hoveredVtxId, vtx.x, vtx.z);
					ImGui::End();
				}

				// Display Grid Info
				if ((s_editFlags & LEF_SHOW_GRID) && !isUiActive() && !isViewportElementHovered())
				{
					Vec2f worldPos = mouseCoordToWorldPos2d(mx, my);
					Vec2f snappedPos;
					grid2d_snap(worldPos, 1, snappedPos);
					f32 grid = grid2d_getGrid(1);

					Vec2i mapPos0 = worldPos2dToMap(snappedPos);
					Vec2i mapPos1 = worldPos2dToMap({ snappedPos.x + grid, snappedPos.z + grid });

					if (mapPos0.x >= s_editWinMapCorner.x && mapPos1.x < s_editWinMapCorner.x + s_editWinSize.x &&
						mapPos1.z >= s_editWinMapCorner.z && mapPos1.z < s_editWinMapCorner.x + s_editWinSize.x)
					{
						char dispString[64];
						if (grid == 1.0f) { sprintf(dispString, "%d unit", s32(grid)); }
						else if (grid > 1.0f) { sprintf(dispString, "%d units", s32(grid)); }
						else { sprintf(dispString, "1/%d unit", s32(1.0f / grid)); }

						f32 len = ImGui::CalcTextSize(dispString).x;
						f32 offset = (mapPos1.x - mapPos0.x - len)*0.5f;
						f32 height = ImGui::GetFontSize();
						if (offset >= 0.0f)
						{
							const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
								| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground;

							ImGui::PushStyleColor(ImGuiCol_Text, { 0.8f, 0.9f, 1.0f, 0.25f });
							ImGui::SetNextWindowSize({ f32(mapPos1.x - mapPos0.x) + 4.0f, ImGui::GetFontSize() + 8.0f });
							ImGui::SetNextWindowPos({ (f32)mapPos0.x, (f32)mapPos1.z - UI_SCALE(20) - 4 });
							ImGui::Begin("##GridInfo", nullptr, window_flags);

							ImVec2 pos = ImGui::GetCursorPos();
							ImDrawList* drawList = ImGui::GetWindowDrawList();

							Vec2f lPos0 = { (f32)mapPos0.x, pos.y + height * 0.5f + (f32)mapPos1.z - UI_SCALE(20) - 4.0f };
							Vec2f lPos1 = { lPos0.x + offset - 2.0f, lPos0.z };
							Vec2f rPos0 = { lPos0.x + len + offset + 2.0f, lPos0.z };
							Vec2f rPos1 = { (f32)mapPos1.x - 4.0f, lPos0.z };
							if (rPos1.x > rPos0.x)
							{
								drawList->AddLine({ lPos0.x, lPos0.z }, { lPos1.x, lPos1.z }, 0x40e6ccff, 1.25f);
								drawList->AddLine({ rPos0.x, rPos0.z }, { rPos1.x, rPos1.z }, 0x40e6ccff, 1.25f);

								drawList->AddLine({ lPos0.x + 4.0f, lPos0.z - 6.0f }, { lPos0.x + 4.0f, lPos0.z + 6.0f }, 0x40e6ccff, 1.25f);
								drawList->AddLine({ rPos1.x, rPos1.z - 6.0f }, { rPos1.x, rPos1.z + 6.0f }, 0x40e6ccff, 1.25f);
							}

							ImGui::SetCursorPos({ offset, pos.y });
							ImGui::Text(dispString, s32(grid));
							ImGui::End();
							ImGui::PopStyleColor();
						}
					}
				}
			}
		}
		levelEditWinEnd();
		popFont();

	#if SHOW_EDITOR_FPS == 1
		TFE_FrontEndUI::drawFps(1800);
	#endif
	}

	////////////////////////////////////////////////////////
	// Internal
	////////////////////////////////////////////////////////
	void resetZoom()
	{
		// We want to zoom into the mouse position.
		s32 relX = s_editWinSize.x / 2;
		s32 relY = s_editWinSize.z / 2;
		// Old position in world units.
		Vec2f worldPos;
		worldPos.x = s_viewportPos.x + f32(relX) * s_zoom2d;
		worldPos.z = s_viewportPos.z + f32(relY) * s_zoom2d;

		s_zoom = c_defaultZoom;
		s_zoom2d = c_defaultZoom;

		// We want worldPos to stay put as we zoom
		Vec2f newWorldPos;
		newWorldPos.x = s_viewportPos.x + f32(relX) * s_zoom2d;
		newWorldPos.z = s_viewportPos.z + f32(relY) * s_zoom2d;
		s_viewportPos.x += (worldPos.x - newWorldPos.x);
		s_viewportPos.z += (worldPos.z - newWorldPos.z);
	}

	void snapToGrid(Vec2f* pos)
	{
		if (!TFE_Input::keyModDown(KEYMOD_ALT))
		{
			pos->x = floorf(pos->x / s_gridSize + 0.5f) * s_gridSize;
			pos->z = floorf(pos->z / s_gridSize + 0.5f) * s_gridSize;
		}
		else  // Snap to the finest grid.
		{
			pos->x = floorf(pos->x * 65536.0f + 0.5f) / 65536.0f;
			pos->z = floorf(pos->z * 65536.0f + 0.5f) / 65536.0f;
		}
	}

	void snapToGrid(Vec3f* pos)
	{
		if (!TFE_Input::keyModDown(KEYMOD_ALT))
		{
			pos->x = floorf(pos->x / s_gridSize + 0.5f) * s_gridSize;
			pos->z = floorf(pos->z / s_gridSize + 0.5f) * s_gridSize;
		}
		else  // Snap to the finest grid.
		{
			pos->x = floorf(pos->x * 65536.0f + 0.5f) / 65536.0f;
			pos->z = floorf(pos->z * 65536.0f + 0.5f) / 65536.0f;
		}
	}
	
	bool vtxEqual(const Vec2f* a, const Vec2f* b)
	{
		const f32 eps = 0.00001f;
		return fabsf(a->x - b->x) < eps && fabsf(a->z - b->z) < eps;
	}

	static u32 s_searchKey = 0;

	void selectFromSingleVertex(EditorSector* root, s32 featureId, bool clearList)
	{
		const Vec2f* rootVtx = &root->vtx[featureId];
		FeatureId rootId = createFeatureId(root, featureId, 0, false);
		if (clearList)
		{
			selection_clear();
			selection_add(rootId);
		}
		else
		{
			if (!selection_add(rootId))
			{
				return;
			}
		}
		s_searchKey++;

		// Which walls share the vertex?
		std::vector<EditorSector*> stack;

		root->searchKey = s_searchKey;
		{
			const size_t wallCount = root->walls.size();
			EditorWall* wall = root->walls.data();
			for (size_t w = 0; w < wallCount; w++, wall++)
			{
				if ((wall->idx[0] == featureId || wall->idx[1] == featureId) && wall->adjoinId >= 0)
				{
					EditorSector* next = &s_level.sectors[wall->adjoinId];
					if (next->searchKey != s_searchKey)
					{
						stack.push_back(next);
						next->searchKey = s_searchKey;
					}
				}
			}
		}

		while (!stack.empty())
		{
			EditorSector* sector = stack.back();
			stack.pop_back();

			const size_t wallCount = sector->walls.size();
			EditorWall* wall = sector->walls.data();
			for (size_t w = 0; w < wallCount; w++, wall++)
			{
				s32 idx = -1;
				if (vtxEqual(rootVtx, &sector->vtx[wall->idx[0]]))
				{
					idx = 0;
				}
				else if (vtxEqual(rootVtx, &sector->vtx[wall->idx[1]]))
				{
					idx = 1;
				}

				if (idx >= 0)
				{
					FeatureId id = createFeatureId(sector, wall->idx[idx], 0, true);
					selection_add(id);
					// Add the next sector to the stack, if it hasn't already been processed.
					EditorSector* next = wall->adjoinId < 0 ? nullptr : &s_level.sectors[wall->adjoinId];
					if (next && next->searchKey != s_searchKey)
					{
						stack.push_back(next);
						next->searchKey = s_searchKey;
					}
				}
			}
		}
	}

	void toggleVertexGroupInclusion(EditorSector* sector, s32 featureId)
	{
		FeatureId id = createFeatureId(sector, featureId);
		if (selection_doesFeatureExist(id))
		{
			// Remove, and all matches.
		}
		else
		{
			// Add
			selectFromSingleVertex(sector, featureId, false);
		}
	}

	void moveVertex(Vec2f worldPos2d)
	{
		snapToGrid(&worldPos2d);
		Vec2f delta = { worldPos2d.x - s_selectedVtxSector->vtx[s_selectedVtxId].x, worldPos2d.z - s_selectedVtxSector->vtx[s_selectedVtxId].z };

		const size_t count = s_selectionList.size();
		const FeatureId* id = s_selectionList.data();
		for (size_t i = 0; i < count; i++)
		{
			s32 featureIndex, featureData;
			bool overlapped;
			EditorSector* sector = unpackFeatureId(id[i], &featureIndex, &featureData, &overlapped);

			sector->vtx[featureIndex].x += delta.x;
			sector->vtx[featureIndex].z += delta.z;

			sectorToPolygon(sector);
			sector->bounds[0] = { sector->poly.bounds[0].x, 0.0f, sector->poly.bounds[0].z };
			sector->bounds[1] = { sector->poly.bounds[1].x, 0.0f, sector->poly.bounds[1].z };
			sector->bounds[0].y = min(sector->floorHeight, sector->ceilHeight);
			sector->bounds[1].y = max(sector->floorHeight, sector->ceilHeight);
		}

		s_selectedVtxPos.x = s_selectedVtxSector->vtx[s_selectedVtxId].x;
		s_selectedVtxPos.z = s_selectedVtxSector->vtx[s_selectedVtxId].z;
	}

	void cameraControl2d(s32 mx, s32 my)
	{
		// WASD controls.
		const f32 moveSpd = s_zoom2d * f32(960.0 * TFE_System::getDeltaTime());
		if (TFE_Input::keyDown(KEY_W))
		{
			s_viewportPos.z -= moveSpd;
		}
		else if (TFE_Input::keyDown(KEY_S))
		{
			s_viewportPos.z += moveSpd;
		}

		if (TFE_Input::keyDown(KEY_A))
		{
			s_viewportPos.x -= moveSpd;
		}
		else if (TFE_Input::keyDown(KEY_D))
		{
			s_viewportPos.x += moveSpd;
		}

		// Mouse scrolling.
		if (TFE_Input::mouseDown(MBUTTON_RIGHT))
		{
			s32 dx, dy;
			TFE_Input::getMouseMove(&dx, &dy);

			s_viewportPos.x -= f32(dx) * s_zoom2d;
			s_viewportPos.z -= f32(dy) * s_zoom2d;
		}

		s32 dx, dy;
		TFE_Input::getMouseWheel(&dx, &dy);
		if (dy != 0)
		{
			// We want to zoom into the mouse position.
			s32 relX = s32(mx - s_editWinMapCorner.x);
			s32 relY = s32(my - s_editWinMapCorner.z);
			// Old position in world units.
			Vec2f worldPos;
			worldPos.x = s_viewportPos.x + f32(relX) * s_zoom2d;
			worldPos.z = s_viewportPos.z + f32(relY) * s_zoom2d;

			s_zoom = std::max(s_zoom - f32(dy) * s_zoom * 0.1f, 1.0f/1024.0f);
			s_zoom = std::min(s_zoom, 4.0f);

			s_zoom2d = floorf(s_zoom * 1024.0f) / 1024.0f;

			// We want worldPos to stay put as we zoom
			Vec2f newWorldPos;
			newWorldPos.x = s_viewportPos.x + f32(relX) * s_zoom2d;
			newWorldPos.z = s_viewportPos.z + f32(relY) * s_zoom2d;
			s_viewportPos.x += (worldPos.x - newWorldPos.x);
			s_viewportPos.z += (worldPos.z - newWorldPos.z);
		}

		if (s_editMove)
		{
			Vec2f worldPos2d = mouseCoordToWorldPos2d(mx, my);

			if (s_editMode == LEDIT_VERTEX)
			{
				if (s_selectedVtxId >= 0 && s_selectedVtxSector && TFE_Input::mouseDown(MBUTTON_LEFT) && !TFE_Input::keyModDown(KEYMOD_CTRL))
				{
					moveVertex(worldPos2d);
				}
				else if (s_selectedVtxId >= 0 && s_selectedVtxSector && s_editMove)
				{
					// Finalize
					// TODO: Add to history.
					s_editMove = false;
				}
				else
				{
					s_editMove = false;
				}
			}
		}
	}

	void cameraControl3d(s32 mx, s32 my)
	{
		// WASD controls.
		f32 moveSpd = f32(16.0 * TFE_System::getDeltaTime());
		if (TFE_Input::keyDown(KEY_LSHIFT) || TFE_Input::keyDown(KEY_RSHIFT))
		{
			moveSpd *= 10.0f;
		}

		if (TFE_Input::keyDown(KEY_W))
		{
			s_camera.pos.x -= s_camera.viewMtx.m2.x * moveSpd;
			s_camera.pos.y -= s_camera.viewMtx.m2.y * moveSpd;
			s_camera.pos.z -= s_camera.viewMtx.m2.z * moveSpd;
		}
		else if (TFE_Input::keyDown(KEY_S))
		{
			s_camera.pos.x += s_camera.viewMtx.m2.x * moveSpd;
			s_camera.pos.y += s_camera.viewMtx.m2.y * moveSpd;
			s_camera.pos.z += s_camera.viewMtx.m2.z * moveSpd;
		}

		if (TFE_Input::keyDown(KEY_A))
		{
			s_camera.pos.x -= s_camera.viewMtx.m0.x * moveSpd;
			s_camera.pos.y -= s_camera.viewMtx.m0.y * moveSpd;
			s_camera.pos.z -= s_camera.viewMtx.m0.z * moveSpd;
		}
		else if (TFE_Input::keyDown(KEY_D))
		{
			s_camera.pos.x += s_camera.viewMtx.m0.x * moveSpd;
			s_camera.pos.y += s_camera.viewMtx.m0.y * moveSpd;
			s_camera.pos.z += s_camera.viewMtx.m0.z * moveSpd;
		}

		// Turning.
		if (TFE_Input::mouseDown(MBUTTON_RIGHT))
		{
			s32 dx, dy;
			TFE_Input::getAccumulatedMouseMove(&dx, &dy);

			const f32 turnSpeed = 0.01f;
			s_camera.yaw   += f32(dx) * turnSpeed;
			s_camera.pitch += f32(dy) * turnSpeed;

			if (s_camera.yaw < 0.0f) { s_camera.yaw += PI * 2.0f; }
			else { s_camera.yaw = fmodf(s_camera.yaw, PI * 2.0f); }

			if (s_camera.pitch < -1.55f) { s_camera.pitch = -1.55f; }
			else if (s_camera.pitch > 1.55f) { s_camera.pitch = 1.55f; }

			s_cursor3d = { 0 };
		}
		else
		{
			TFE_Input::clearAccumulatedMouseMove();
		}
		computeCameraTransform();

		if (s_editMove)
		{
			// TODO: Factor out 2D and 3D versions.
			// TODO: Auto-select overlapping vertices from connected sectors.
			f32 u = (s_selectedVtxPos.y - s_camera.pos.y) / (s_cursor3d.y - s_camera.pos.y);
			Vec2f worldPos2d;
			worldPos2d.x = s_camera.pos.x + u * (s_cursor3d.x - s_camera.pos.x);
			worldPos2d.z = s_camera.pos.z + u * (s_cursor3d.z - s_camera.pos.z);

			if (s_editMode == LEDIT_VERTEX)
			{
				if (s_selectedVtxId >= 0 && s_selectedVtxSector && TFE_Input::mouseDown(MBUTTON_LEFT) && !TFE_Input::keyModDown(KEYMOD_CTRL))
				{
					moveVertex(worldPos2d);
				}
				else if (s_selectedVtxId >= 0 && s_selectedVtxSector && s_editMove)
				{
					// Finalize
					// TODO: Add to history.
					s_editMove = false;
				}
				else
				{
					s_editMove = false;
				}
			}
		}
	}
		
	void toolbarBegin()
	{
		bool toolbarActive = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("MainToolbar", { 0.0f, 22.0f });
		ImGui::SetWindowSize("MainToolbar", { (f32)displayInfo.width - 480.0f, 48.0f });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("MainToolbar", &toolbarActive, window_flags);
	}

	void toolbarEnd()
	{
		ImGui::End();
	}

	bool drawToolbarButton(void* ptr, u32 imageId, bool highlight)
	{
		const f32 imageHeightScale = 1.0f / 192.0f;
		const f32 y0 = f32(imageId) * 32.0f;
		const f32 y1 = y0 + 32.0f;

		ImGui::PushID(imageId);
		bool res = ImGui::ImageButton(ptr, ImVec2(32, 32), ImVec2(0.0f, y0 * imageHeightScale), ImVec2(1.0f, y1 * imageHeightScale), 0, ImVec4(0, 0, 0, highlight ? 0.75f : 0.25f), ImVec4(1, 1, 1, 1));
		ImGui::PopID();

		return res;
	}

	void levelEditWinBegin()
	{
		bool gridActive = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);
		s_editWinSize = { (s32)displayInfo.width - (s32)UI_SCALE(480), (s32)displayInfo.height - (s32)UI_SCALE(68) };

		ImGui::SetWindowPos("LevelEditWin", { (f32)s_editWinPos.x, (f32)s_editWinPos.z });
		ImGui::SetWindowSize("LevelEditWin", { (f32)s_editWinSize.x, (f32)s_editWinSize.z });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("LevelEditWin", &gridActive, window_flags);
	}

	void levelEditWinEnd()
	{
		ImGui::End();
	}

	// Convert from viewport mouse coordinates to world space 2D position.
	Vec2f mouseCoordToWorldPos2d(s32 mx, s32 my)
	{
		// World position from viewport position, relative mouse position and zoom.
		Vec2f worldPos;
		worldPos.x =   s_viewportPos.x + f32(mx - s_editWinMapCorner.x) * s_zoom2d;
		worldPos.z = -(s_viewportPos.z + f32(my - s_editWinMapCorner.z) * s_zoom2d);
		return worldPos;
	}

	Vec2i worldPos2dToMap(const Vec2f& worldPos)
	{
		Vec2i mapPos;
		mapPos.x = s32(( worldPos.x - s_viewportPos.x) / s_zoom2d + s_editWinMapCorner.x);
		mapPos.z = s32((-worldPos.z - s_viewportPos.z) / s_zoom2d + s_editWinMapCorner.z);
		return mapPos;
	}

	Vec4f transform(const Mat4& mtx, const Vec4f& vec)
	{
		return { TFE_Math::dot(&mtx.m0, &vec), TFE_Math::dot(&mtx.m1, &vec), TFE_Math::dot(&mtx.m2, &vec), TFE_Math::dot(&mtx.m3, &vec) };
	}

	// Convert from viewport mouse coordinates to world space 3D direction.
	Vec3f mouseCoordToWorldDir3d(s32 mx, s32 my)
	{
		const Vec4f posScreen =
		{
			f32(mx - s_editWinMapCorner.x) / f32(s_viewportSize.x) * 2.0f - 1.0f,
			f32(my - s_editWinMapCorner.z) / f32(s_viewportSize.z) * 2.0f - 1.0f,
			-1.0f, 1.0f
		};
		const Vec4f posView = transform(s_camera.invProj, posScreen);
		const Mat3  viewT = TFE_Math::transpose(s_camera.viewMtx);

		const Vec3f posView3 = { posView.x, -posView.y, posView.z };
		const Vec3f posRelWorld =	// world relative to the camera position.
		{
			TFE_Math::dot(&posView3, &viewT.m0),
			TFE_Math::dot(&posView3, &viewT.m1),
			TFE_Math::dot(&posView3, &viewT.m2)
		};
		return TFE_Math::normalize(&posRelWorld);
	}

	void messagePanel(ImVec2 pos)
	{
		bool msgPanel = true;
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground;

		ImGui::SetNextWindowPos({ pos.x, pos.y + 24.0f });
		ImGui::BeginChild("MsgPanel", { 280.0f, 20.0f }, false, window_flags);

		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		if (mx >= s_editWinPos.x && mx < s_editWinPos.x + s_editWinSize.x && my >= s_editWinPos.z && my < s_editWinPos.z + s_editWinSize.z && !getMenuActive() && !isUiActive())
		{
			if (s_view == EDIT_VIEW_2D)
			{
				Vec2f worldPos = mouseCoordToWorldPos2d(mx, my);
				// Assume the default zoom = 0.25 = 100%
				ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 0.75f }, "Zoom %0.2f%% : Pos %0.2f, %0.2f", 0.25f * 100.0f / s_zoom2d, worldPos.x, worldPos.z);
			}
			else
			{
				ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 0.75f }, "Pos %0.3f, %0.3f, %0.3f", s_camera.pos.x, s_camera.pos.y, s_camera.pos.z);
			}
		}

		ImGui::EndChild();
	}

	TextureGpu* loadGpuImage(const char* path)
	{
		char imagePath[TFE_MAX_PATH];
		strcpy(imagePath, path);
		if (!TFE_Paths::mapSystemPath(imagePath))
		{
			memset(imagePath, 0, TFE_MAX_PATH);
			TFE_Paths::appendPath(TFE_PathType::PATH_PROGRAM, path, imagePath, TFE_MAX_PATH);
			FileUtil::fixupPath(imagePath);
		}

		TextureGpu* gpuImage = nullptr;
		SDL_Surface* image = TFE_Image::get(path);
		if (image)
		{
			gpuImage = TFE_RenderBackend::createTexture(image->w, image->h, (u32*)image->pixels, MAG_FILTER_LINEAR);
		}
		return gpuImage;
	}
}
