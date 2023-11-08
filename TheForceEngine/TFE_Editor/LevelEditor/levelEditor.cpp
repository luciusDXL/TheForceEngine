#include "levelEditor.h"
#include "levelEditorData.h"
#include "levelEditorHistory.h"
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

	struct VertexWallGroup
	{
		EditorSector* sector;
		s32 vertexIndex;
		s32 leftWall, rightWall;
	};
	
	enum WallMoveMode
	{
		WMM_NORMAL = 0,
		WMM_TANGENT,
		WMM_FREE,
		WMM_COUNT
	};

	enum EdgeId
	{
		EDGE0 = (0 << 8),
		EDGE1 = (1 << 8)
	};

	static WallMoveMode s_wallMoveMode = WMM_NORMAL;

	// The TFE Level Editor format is different than the base format and contains extra 
	// metadata, etc.
	// So existing levels need to be loaded into that format.
	// If the correct format already exists, though, then it is loaded directly.
	AssetList s_levelTextureList;
	LevelEditMode s_editMode = LEDIT_DRAW;

	u32 s_editFlags = LEF_DEFAULT;
	u32 s_lwinOpen = LWIN_NONE;
	s32 s_curLayer = 0;

	// Unified feature hover/cur
	Feature s_featureHovered = {};
	Feature s_featureCur = {};
	Feature s_featureCurWall = {};	// used to access wall data in other modes.

	// Vertex
	Vec3f s_hoveredVtxPos;
	Vec3f s_curVtxPos;
		
	// Search
	u32 s_searchKey = 0;
	static std::vector<VertexWallGroup> s_vertexWallGroups;

	// Sector Drawing
	const f32 c_defaultSectorHeight = 16.0f;

	bool s_drawStarted = false;
	DrawMode s_drawMode;
	f32 s_drawHeight[2];
	std::vector<Vec2f> s_shape;
	Vec2f s_drawCurPos;
	Polygon s_shapePolygon;

	static bool s_editMove = false;
	
	static EditorView s_view = EDIT_VIEW_2D;
	static Vec2i s_editWinPos = { 0, 69 };
	static Vec2i s_editWinSize = { 0 };
	static Vec2f s_editWinMapCorner = { 0 };
	static Vec3f s_rayDir = { 0.0f, 0.0f, 1.0f };
	static f32 s_zoom = c_defaultZoom;
	static bool s_gravity = false;
	static bool s_showAllLabels = false;
	static bool s_uiActive = false;

	static bool s_moveStarted = false;
	static Vec2f s_moveStartPos;
	static Vec2f s_moveStartPos1;
	static Vec2f s_moveTotalDelta;

	static char s_layerMem[4 * 31];
	static char* s_layerStr[31];

	// Wall editing
	static Vec2f s_wallNrm;
	static Vec2f s_wallTan;
	static Vec3f s_prevPos = { 0 };

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
	bool isUiActive();
	bool isViewportElementHovered();
	Vec3f moveAlongRail(Vec3f dir);
	Vec2f mouseCoordToWorldPos2d(s32 mx, s32 my);
	Vec3f mouseCoordToWorldDir3d(s32 mx, s32 my);
	Vec3f viewportCoordToWorldDir3d(Vec2i vCoord);
	Vec2i worldPos2dToMap(const Vec2f& worldPos);
	bool worldPosToViewportCoord(Vec3f worldPos, Vec2f* screenPos);
	TextureGpu* loadGpuImage(const char* path);

	void selectFromSingleVertex(EditorSector* root, s32 featureIndex, bool clearList);
	void selectOverlappingVertices(EditorSector* root, s32 featureIndex, const Vec2f* rootVtx, bool addToSelection, bool addToVtxList=false);
	void toggleVertexGroupInclusion(EditorSector* sector, s32 featureId);
	void toggleWallGroupInclusion(EditorSector* sector, s32 featureIndex, s32 part);
	void gatherVerticesFromWallSelection();
	bool vtxEqual(const Vec2f* a, const Vec2f* b);
	void moveFeatures(const Vec3f& newPos);
	void handleVertexInsert(Vec2f worldPos);

	void snapToGrid(f32* value);
	void snapToGrid(Vec2f* pos);
	void snapToGrid(Vec3f* pos);

	void drawViewportInfo(s32 index, Vec2i mapPos, const char* info, f32 xOffset, f32 yOffset, f32 alpha=1.0f);
	void getWallLengthText(const Vec2f* v0, const Vec2f* v1, char* text, Vec2i& mapPos, s32 index = -1, Vec2f* mapOffset = nullptr);
	
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
		if (!loadLevelFromAsset(asset))
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
		s_featureHovered = {};
		s_featureCur = {};
		s_editMove = false;
		s_gravity = false;
		s_showAllLabels = false;

		AssetBrowser::getLevelTextures(s_levelTextureList, asset->name.c_str());
		s_camera = { 0 };
		s_camera.pos.y = c_defaultCameraHeight;
		s_camera.yaw = c_defaultYaw;
		computeCameraTransform();
		s_cursor3d = { 0 };

		levHistory_init();
		levHistory_createSnapshot("Imported Level");

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

		levHistory_destroy();
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
		return s_featureHovered.featureIndex >= 0 || s_featureCur.featureIndex >= 0;
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

	bool aabbOverlap2d(const Vec3f* aabb0, const Vec3f* aabb1)
	{
		// Ignore the Y axis.
		// X
		if (aabb0[0].x > aabb1[1].x || aabb0[1].x < aabb1[0].x ||
			aabb1[0].x > aabb0[1].x || aabb1[1].x < aabb0[0].x)
		{
			return false;
		}

		// Z
		if (aabb0[0].z > aabb1[1].z || aabb0[1].z < aabb1[0].z ||
			aabb1[0].z > aabb0[1].z || aabb1[1].z < aabb0[0].z)
		{
			return false;
		}

		return true;
	}

	bool aabbOverlap3d(const Vec3f* aabb0, const Vec3f* aabb1)
	{
		// Ignore the Y axis.
		// X
		if (aabb0[0].x > aabb1[1].x || aabb0[1].x < aabb1[0].x ||
			aabb1[0].x > aabb0[1].x || aabb1[1].x < aabb0[0].x)
		{
			return false;
		}

		// Y
		if (aabb0[0].y > aabb1[1].y || aabb0[1].y < aabb1[0].y ||
			aabb1[0].y > aabb0[1].y || aabb1[1].y < aabb0[0].y)
		{
			return false;
		}

		// Z
		if (aabb0[0].z > aabb1[1].z || aabb0[1].z < aabb1[0].z ||
			aabb1[0].z > aabb0[1].z || aabb1[1].z < aabb0[0].z)
		{
			return false;
		}

		return true;
	}

	void wallComputeDragSelect()
	{
		if (s_dragSelect.curPos.x == s_dragSelect.startPos.x && s_dragSelect.curPos.z == s_dragSelect.startPos.z)
		{
			return;
		}
		if (s_dragSelect.mode == DSEL_SET)
		{
			selection_clear(false);
		}
		s_featureCur.featureIndex = -1;
		s_featureCur.sector = nullptr;

		if (s_view == EDIT_VIEW_2D)
		{
			// Convert drag select coordinates to world space.
			// World position from viewport position, relative mouse position and zoom.
			Vec2f worldPos[2];
			worldPos[0].x =   s_viewportPos.x + f32(s_dragSelect.curPos.x) * s_zoom2d;
			worldPos[0].z = -(s_viewportPos.z + f32(s_dragSelect.curPos.z) * s_zoom2d);
			worldPos[1].x =   s_viewportPos.x + f32(s_dragSelect.startPos.x) * s_zoom2d;
			worldPos[1].z = -(s_viewportPos.z + f32(s_dragSelect.startPos.z) * s_zoom2d);

			Vec3f aabb[] =
			{
				{ std::min(worldPos[0].x, worldPos[1].x), 0.0f, std::min(worldPos[0].z, worldPos[1].z) },
				{ std::max(worldPos[0].x, worldPos[1].x), 0.0f, std::max(worldPos[0].z, worldPos[1].z) }
			};

			const size_t sectorCount = s_level.sectors.size();
			EditorSector* sector = s_level.sectors.data();
			for (size_t s = 0; s < sectorCount; s++, sector++)
			{
				if (s_curLayer != sector->layer && s_curLayer != LAYER_ANY) { continue; }
				if (!aabbOverlap2d(sector->bounds, aabb)) { continue; }

				const size_t wallCount = sector->walls.size();
				const EditorWall* wall = sector->walls.data();
				const Vec2f* vtx = sector->vtx.data();
				for (size_t w = 0; w < wallCount; w++, wall++)
				{
					const Vec2f* v0 = &vtx[wall->idx[0]];
					const Vec2f* v1 = &vtx[wall->idx[1]];

					if (v0->x >= aabb[0].x && v0->x < aabb[1].x && v0->z >= aabb[0].z && v0->z < aabb[1].z &&
						v1->x >= aabb[0].x && v1->x < aabb[1].x && v1->z >= aabb[0].z && v1->z < aabb[1].z)
					{
						FeatureId id = createFeatureId(sector, (s32)w, HP_MID);
						if (s_dragSelect.mode == DSEL_REM)
						{
							selection_remove(id);
						}
						else
						{
							selection_add(id);
						}
					}
				}
			}
		}
		else if (s_view == EDIT_VIEW_3D)
		{
			// In 3D, build a frustum and select point inside the frustum.
			// The near plane corners of the frustum are derived from the screenspace rectangle.
			const s32 x0 = std::min(s_dragSelect.startPos.x, s_dragSelect.curPos.x);
			const s32 x1 = std::max(s_dragSelect.startPos.x, s_dragSelect.curPos.x);
			const s32 z0 = std::min(s_dragSelect.startPos.z, s_dragSelect.curPos.z);
			const s32 z1 = std::max(s_dragSelect.startPos.z, s_dragSelect.curPos.z);
			// Directions to the near-plane frustum corners.
			const Vec3f d0 = viewportCoordToWorldDir3d({ x0, z0 });
			const Vec3f d1 = viewportCoordToWorldDir3d({ x1, z0 });
			const Vec3f d2 = viewportCoordToWorldDir3d({ x1, z1 });
			const Vec3f d3 = viewportCoordToWorldDir3d({ x0, z1 });
			// Camera forward vector.
			const Vec3f fwd = { -s_camera.viewMtx.m2.x, -s_camera.viewMtx.m2.y, -s_camera.viewMtx.m2.z };
			// Trace forward at the screen center to get the likely focus sector.
			Vec3f centerViewDir = viewportCoordToWorldDir3d({ s_viewportSize.x / 2, s_viewportSize.z / 2 });
			RayHitInfo hitInfo;

			s32 layer = (s_editFlags & LEF_SHOW_ALL_LAYERS) ? LAYER_ANY : s_curLayer;
			Ray ray = { s_camera.pos, centerViewDir, 1000.0f, layer };

			f32 nearDist = 1.0f;
			f32 farDist = 100.0f;
			if (traceRay(&ray, &hitInfo, false) && hitInfo.hitSectorId >= 0)
			{
				EditorSector* hitSector = &s_level.sectors[hitInfo.hitSectorId];
				Vec3f bbox[] =
				{
					{hitSector->bounds[0].x, hitSector->floorHeight, hitSector->bounds[0].z},
					{hitSector->bounds[1].x, hitSector->floorHeight, hitSector->bounds[0].z},
					{hitSector->bounds[1].x, hitSector->floorHeight, hitSector->bounds[1].z},
					{hitSector->bounds[0].x, hitSector->floorHeight, hitSector->bounds[1].z},

					{hitSector->bounds[0].x, hitSector->ceilHeight, hitSector->bounds[0].z},
					{hitSector->bounds[1].x, hitSector->ceilHeight, hitSector->bounds[0].z},
					{hitSector->bounds[1].x, hitSector->ceilHeight, hitSector->bounds[1].z},
					{hitSector->bounds[0].x, hitSector->ceilHeight, hitSector->bounds[1].z},
				};
				f32 maxDist = 0.0f;
				for (s32 i = 0; i < 8; i++)
				{
					Vec3f offset = { bbox[i].x - s_camera.pos.x, bbox[i].y - s_camera.pos.y, bbox[i].z - s_camera.pos.z };
					f32 dist = TFE_Math::dot(&offset, &fwd);
					if (dist > maxDist)
					{
						maxDist = dist;
					}
				}
				if (maxDist > 0.0f) { farDist = maxDist; }
			}
			// Build Planes.
			Vec3f near = { s_camera.pos.x + nearDist*fwd.x, s_camera.pos.y + nearDist*fwd.y, s_camera.pos.z + nearDist*fwd.z };
			Vec3f far  = { s_camera.pos.x + farDist*fwd.x, s_camera.pos.y + farDist*fwd.y, s_camera.pos.z + farDist*fwd.z };
						
			Vec3f left  = TFE_Math::cross(&d3, &d0);
			Vec3f right = TFE_Math::cross(&d1, &d2);
			Vec3f top   = TFE_Math::cross(&d0, &d1);
			Vec3f bot   = TFE_Math::cross(&d2, &d3);

			left  = TFE_Math::normalize(&left);
			right = TFE_Math::normalize(&right);
			top   = TFE_Math::normalize(&top);
			bot   = TFE_Math::normalize(&bot);

			const Vec4f plane[] =
			{
				{ -fwd.x, -fwd.y, -fwd.z,  TFE_Math::dot(&fwd, &near) },
				{  fwd.x,  fwd.y,  fwd.z, -TFE_Math::dot(&fwd, &far) },
				{ left.x, left.y, left.z, -TFE_Math::dot(&left, &s_camera.pos) },
				{ right.x, right.y, right.z, -TFE_Math::dot(&right, &s_camera.pos) },
				{ top.x, top.y, top.z, -TFE_Math::dot(&top, &s_camera.pos) },
				{ bot.x, bot.y, bot.z, -TFE_Math::dot(&bot, &s_camera.pos) }
			};

			const size_t sectorCount = s_level.sectors.size();
			EditorSector* sector = s_level.sectors.data();
			for (size_t s = 0; s < sectorCount; s++, sector++)
			{
				if (layer != sector->layer && layer != LAYER_ANY) { continue; }

				// For now, do them all...
				const size_t wallCount = sector->walls.size();
				const EditorWall* wall = sector->walls.data();
				const Vec2f* vtx = sector->vtx.data();
				for (size_t w = 0; w < wallCount; w++, wall++)
				{
					const Vec2f* v0 = &vtx[wall->idx[0]];
					const Vec2f* v1 = &vtx[wall->idx[1]];

					struct Vec3f wallVtx[]=
					{
						{ v0->x, sector->floorHeight, v0->z },
						{ v1->x, sector->floorHeight, v1->z },
						{ v1->x, sector->ceilHeight, v1->z },
						{ v0->x, sector->ceilHeight, v0->z }
					};
					
					bool inside = true;
					for (s32 p = 0; p < 6; p++)
					{
						// Check the wall vertices.
						for (s32 v = 0; v < 4; v++)
						{
							if (plane[p].x*wallVtx[v].x + plane[p].y*wallVtx[v].y + plane[p].z*wallVtx[v].z + plane[p].w > 0.0f)
							{
								inside = false;
								break;
							}
						}
					}

					if (inside)
					{
						FeatureId id = createFeatureId(sector, (s32)w, HP_MID);
						if (s_dragSelect.mode == DSEL_REM) { selection_remove(id); }
						else { selection_add(id); }
					}
				}
				// TODO: Check the floor and ceiling?
			}
		}
	}

	void vertexComputeDragSelect()
	{
		if (s_dragSelect.curPos.x == s_dragSelect.startPos.x && s_dragSelect.curPos.z == s_dragSelect.startPos.z)
		{
			return;
		}
		if (s_dragSelect.mode == DSEL_SET)
		{
			selection_clear(false);
		}
		s_featureCur.featureIndex = -1;
		s_featureCur.sector = nullptr;

		if (s_view == EDIT_VIEW_2D)
		{
			// Convert drag select coordinates to world space.
			// World position from viewport position, relative mouse position and zoom.
			Vec2f worldPos[2];
			worldPos[0].x =   s_viewportPos.x + f32(s_dragSelect.curPos.x) * s_zoom2d;
			worldPos[0].z = -(s_viewportPos.z + f32(s_dragSelect.curPos.z) * s_zoom2d);
			worldPos[1].x =   s_viewportPos.x + f32(s_dragSelect.startPos.x) * s_zoom2d;
			worldPos[1].z = -(s_viewportPos.z + f32(s_dragSelect.startPos.z) * s_zoom2d);

			Vec3f aabb[] =
			{
				{ std::min(worldPos[0].x, worldPos[1].x), 0.0f, std::min(worldPos[0].z, worldPos[1].z) },
				{ std::max(worldPos[0].x, worldPos[1].x), 0.0f, std::max(worldPos[0].z, worldPos[1].z) }
			};

			const size_t sectorCount = s_level.sectors.size();
			EditorSector* sector = s_level.sectors.data();
			for (size_t s = 0; s < sectorCount; s++, sector++)
			{
				if (s_curLayer != sector->layer && s_curLayer != LAYER_ANY) { continue; }
				if (!aabbOverlap2d(sector->bounds, aabb)) { continue; }

				const size_t vtxCount = sector->vtx.size();
				const Vec2f* vtx = sector->vtx.data();
				for (size_t v = 0; v < vtxCount; v++, vtx++)
				{
					if (vtx->x >= aabb[0].x && vtx->x < aabb[1].x && vtx->z >= aabb[0].z && vtx->z < aabb[1].z)
					{
						FeatureId id = createFeatureId(sector, (s32)v, 0, false);
						if (s_dragSelect.mode == DSEL_REM)
						{
							selection_remove(id);
						}
						else
						{
							selection_add(id);
						}
					}
				}
			}
		}
		else if (s_view == EDIT_VIEW_3D)
		{
			// In 3D, build a frustum and select point inside the frustum.
			// The near plane corners of the frustum are derived from the screenspace rectangle.
			const s32 x0 = std::min(s_dragSelect.startPos.x, s_dragSelect.curPos.x);
			const s32 x1 = std::max(s_dragSelect.startPos.x, s_dragSelect.curPos.x);
			const s32 z0 = std::min(s_dragSelect.startPos.z, s_dragSelect.curPos.z);
			const s32 z1 = std::max(s_dragSelect.startPos.z, s_dragSelect.curPos.z);
			// Directions to the near-plane frustum corners.
			const Vec3f d0 = viewportCoordToWorldDir3d({ x0, z0 });
			const Vec3f d1 = viewportCoordToWorldDir3d({ x1, z0 });
			const Vec3f d2 = viewportCoordToWorldDir3d({ x1, z1 });
			const Vec3f d3 = viewportCoordToWorldDir3d({ x0, z1 });
			// Camera forward vector.
			const Vec3f fwd = { -s_camera.viewMtx.m2.x, -s_camera.viewMtx.m2.y, -s_camera.viewMtx.m2.z };
			// Trace forward at the screen center to get the likely focus sector.
			Vec3f centerViewDir = viewportCoordToWorldDir3d({ s_viewportSize.x / 2, s_viewportSize.z / 2 });
			RayHitInfo hitInfo;

			s32 layer = (s_editFlags & LEF_SHOW_ALL_LAYERS) ? LAYER_ANY : s_curLayer;
			Ray ray = { s_camera.pos, centerViewDir, 1000.0f, layer };

			f32 nearDist = 1.0f;
			f32 farDist = 100.0f;
			if (traceRay(&ray, &hitInfo, false) && hitInfo.hitSectorId >= 0)
			{
				EditorSector* hitSector = &s_level.sectors[hitInfo.hitSectorId];
				Vec3f bbox[] =
				{
					{hitSector->bounds[0].x, hitSector->floorHeight, hitSector->bounds[0].z},
					{hitSector->bounds[1].x, hitSector->floorHeight, hitSector->bounds[0].z},
					{hitSector->bounds[1].x, hitSector->floorHeight, hitSector->bounds[1].z},
					{hitSector->bounds[0].x, hitSector->floorHeight, hitSector->bounds[1].z},

					{hitSector->bounds[0].x, hitSector->ceilHeight, hitSector->bounds[0].z},
					{hitSector->bounds[1].x, hitSector->ceilHeight, hitSector->bounds[0].z},
					{hitSector->bounds[1].x, hitSector->ceilHeight, hitSector->bounds[1].z},
					{hitSector->bounds[0].x, hitSector->ceilHeight, hitSector->bounds[1].z},
				};
				f32 maxDist = 0.0f;
				for (s32 i = 0; i < 8; i++)
				{
					Vec3f offset = { bbox[i].x - s_camera.pos.x, bbox[i].y - s_camera.pos.y, bbox[i].z - s_camera.pos.z };
					f32 dist = TFE_Math::dot(&offset, &fwd);
					if (dist > maxDist)
					{
						maxDist = dist;
					}
				}
				if (maxDist > 0.0f) { farDist = maxDist; }
			}
			// Build Planes.
			Vec3f near = { s_camera.pos.x + nearDist*fwd.x, s_camera.pos.y + nearDist*fwd.y, s_camera.pos.z + nearDist*fwd.z };
			Vec3f far  = { s_camera.pos.x + farDist*fwd.x, s_camera.pos.y + farDist*fwd.y, s_camera.pos.z + farDist*fwd.z };
						
			Vec3f left  = TFE_Math::cross(&d3, &d0);
			Vec3f right = TFE_Math::cross(&d1, &d2);
			Vec3f top   = TFE_Math::cross(&d0, &d1);
			Vec3f bot   = TFE_Math::cross(&d2, &d3);

			left  = TFE_Math::normalize(&left);
			right = TFE_Math::normalize(&right);
			top   = TFE_Math::normalize(&top);
			bot   = TFE_Math::normalize(&bot);

			const Vec4f plane[] =
			{
				{ -fwd.x, -fwd.y, -fwd.z,  TFE_Math::dot(&fwd, &near) },
				{  fwd.x,  fwd.y,  fwd.z, -TFE_Math::dot(&fwd, &far) },
				{ left.x, left.y, left.z, -TFE_Math::dot(&left, &s_camera.pos) },
				{ right.x, right.y, right.z, -TFE_Math::dot(&right, &s_camera.pos) },
				{ top.x, top.y, top.z, -TFE_Math::dot(&top, &s_camera.pos) },
				{ bot.x, bot.y, bot.z, -TFE_Math::dot(&bot, &s_camera.pos) }
			};

			const size_t sectorCount = s_level.sectors.size();
			EditorSector* sector = s_level.sectors.data();
			for (size_t s = 0; s < sectorCount; s++, sector++)
			{
				if (layer != sector->layer && layer != LAYER_ANY) { continue; }

				// For now, do them all...
				const size_t vtxCount = sector->vtx.size();
				const Vec2f* vtx = sector->vtx.data();
				for (size_t v = 0; v < vtxCount; v++, vtx++)
				{
					bool inside = true;
					for (s32 p = 0; p < 6; p++)
					{
						if (plane[p].x*vtx->x + plane[p].y*sector->floorHeight + plane[p].z*vtx->z + plane[p].w > 0.0f)
						{
							inside = false;
							break;
						}
					}

					if (inside)
					{
						FeatureId id = createFeatureId(sector, (s32)v, 0, false);
						if (s_dragSelect.mode == DSEL_REM) { selection_remove(id); }
						else { selection_add(id); }
						selectOverlappingVertices(sector, (s32)v, vtx, s_dragSelect.mode != DSEL_REM);
					}
				}
			}
		}
	}

	void startDragSelect(s32 mx, s32 my, DragSelectMode mode)
	{
		// Drag select start.
		mx -= (s32)s_editWinMapCorner.x;
		my -= (s32)s_editWinMapCorner.z;
		if (mx >= 0 && my >= 0 && mx < s_viewportSize.x && my < s_viewportSize.z)
		{
			s_dragSelect.mode = mode;
			s_dragSelect.active = true;
			s_dragSelect.moved = false;
			s_dragSelect.startPos = { mx, my };
			s_dragSelect.curPos = s_dragSelect.startPos;
		}
	}

	void updateDragSelect(s32 mx, s32 my)
	{
		// Drag select start.
		mx -= (s32)s_editWinMapCorner.x;
		my -= (s32)s_editWinMapCorner.z;
		if (mx < 0 || my < 0 || mx >= s_viewportSize.x || my >= s_viewportSize.z)
		{
			s_dragSelect.active = false;
		}
		else
		{
			s_dragSelect.curPos = { mx, my };
			if (s_editMode == LEDIT_VERTEX)
			{
				vertexComputeDragSelect();
			}
			else if (s_editMode == LEDIT_WALL)
			{
				wallComputeDragSelect();
			}
		}
	}

	bool isSingleOverlappedVertex()
	{
		const size_t count = s_selectionList.size();
		if (count <= 1) { return true; }

		const FeatureId* list = s_selectionList.data();

		s32 featureIndex;
		EditorSector* featureSector = unpackFeatureId(list[0], &featureIndex);
		const Vec2f* vtx = &featureSector->vtx[featureIndex];

		for (size_t i = 1; i < count; i++)
		{
			EditorSector* featureSector = unpackFeatureId(list[i], &featureIndex);
			if (!vtxEqual(vtx, &featureSector->vtx[featureIndex]))
			{
				return false;
			}
		}
		return true;
	}
			
	void handleMouseControlVertex(Vec2f worldPos)
	{
		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);

		// Short names to make the logic easier to follow.
		const bool selAdd    = TFE_Input::keyModDown(KEYMOD_SHIFT);
		const bool selRem    = TFE_Input::keyModDown(KEYMOD_ALT);
		const bool selToggle = TFE_Input::keyModDown(KEYMOD_CTRL);
		const bool selToggleDrag = selAdd && selToggle;

		const bool mousePressed = TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT);
		const bool mouseDown = TFE_Input::mouseDown(MouseButton::MBUTTON_LEFT);

		const bool del = TFE_Input::keyPressed(KEY_DELETE);

		if (del && (!s_selectionList.empty() || s_featureCur.featureIndex >= 0 || s_featureHovered.featureIndex >= 0))
		{
			s32 sectorId, vertexIndex;
			if (!s_selectionList.empty())
			{
				// TODO: Currently, you can only delete one vertex at a time. It should be possible to delete multiple.
				EditorSector* featureSector = unpackFeatureId(s_selectionList[0], &vertexIndex);
				sectorId = featureSector->id;
			}
			else if (s_featureCur.featureIndex >= 0 && s_featureCur.sector)
			{
				sectorId = s_featureCur.sector->id;
				vertexIndex = s_featureCur.featureIndex;
			}
			else if (s_featureHovered.featureIndex >= 0 && s_featureHovered.sector)
			{
				sectorId = s_featureHovered.sector->id;
				vertexIndex = s_featureHovered.featureIndex;
			}

			edit_deleteVertex(sectorId, vertexIndex);
			cmd_addDeleteVertex(sectorId, vertexIndex);
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
				if (s_featureHovered.sector && s_featureHovered.featureIndex >= 0)
				{
					s_editMove = true;
					adjustGridHeight(s_featureHovered.sector);
					toggleVertexGroupInclusion(s_featureHovered.sector, s_featureHovered.featureIndex);
				}
			}
			else
			{
				s_featureCur.sector = nullptr;
				s_featureCur.featureIndex = -1;
				if (s_featureHovered.sector && s_featureHovered.featureIndex >= 0)
				{
					FeatureId id = createFeatureId(s_featureHovered.sector, s_featureHovered.featureIndex);
					bool doesItemExist = selection_doesFeatureExist(id);
					bool canChangeSelection = true;
					if (s_selectionList.size() > 1 && !doesItemExist)
					{
						canChangeSelection = isSingleOverlappedVertex();
					}

					if (canChangeSelection)
					{
						s_featureCur.sector = s_featureHovered.sector;
						s_featureCur.featureIndex = s_featureHovered.featureIndex;
						s_curVtxPos = s_hoveredVtxPos;
						adjustGridHeight(s_featureCur.sector);
						s_editMove = true;

						// Clear the selection if this is a new vertex and Ctrl isn't held.
						bool clearList = !doesItemExist;
						selectFromSingleVertex(s_featureCur.sector, s_featureCur.featureIndex, clearList);
					}
				}
				else if (!s_editMove)
				{
					startDragSelect(mx, my, DSEL_SET);
				}
			}
		}
		else if (mouseDown)
		{
			if (!s_dragSelect.active)
			{
				if (selToggleDrag && s_featureHovered.sector && s_featureHovered.featureIndex >= 0)
				{
					s_editMove = true;
					adjustGridHeight(s_featureHovered.sector);
					selectFromSingleVertex(s_featureHovered.sector, s_featureHovered.featureIndex, false);
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

		handleVertexInsert(worldPos);
	}

	void splitWall(EditorSector* sector, s32 wallIndex, Vec2f newPos, EditorWall* outWalls[])
	{
		// TODO: Is it worth it to insert the vertex right after i0 as well instead of at the end?
		const s32 newIdx = (s32)sector->vtx.size();
		sector->vtx.push_back(newPos);

		EditorWall* srcWall = &sector->walls[wallIndex];
		const s32 idx[3] = { srcWall->idx[0], newIdx, srcWall->idx[1] };

		// Copy wall attributes (textures, flags, etc.).
		EditorWall newWall = *srcWall;
		// Then setup indices.
		srcWall->idx[0] = idx[0];
		srcWall->idx[1] = idx[1];
		newWall.idx[0] = idx[1];
		newWall.idx[1] = idx[2];

		// Insert the new wall right after the wall being split.
		// This makes the process a bit more complicated, but keeps things clean.
		sector->walls.insert(sector->walls.begin() + wallIndex + 1, newWall);

		// Pointers to the new walls (note that since the wall array was resized, the source wall
		// pointer might have changed as well).
		outWalls[0] = &sector->walls[wallIndex];
		outWalls[1] = &sector->walls[wallIndex + 1];
	}

	void fixupWallMirrors(EditorSector* sector, s32 wallIndex)
	{
		bool hasWallsPastSplit = wallIndex + 1 < sector->walls.size();
		if (!hasWallsPastSplit) { return; }

		// Gather sectors that might need to be changed.
		s_searchKey++;
		s_sectorChangeList.clear();
		const size_t wallCount = sector->walls.size();
		EditorWall* wall = sector->walls.data();
		for (size_t w = 0; w < wallCount; w++, wall++)
		{
			if (wall->adjoinId < 0) { continue; }
			EditorSector* nextSector = &s_level.sectors[wall->adjoinId];
			if (nextSector->searchKey != s_searchKey)
			{
				nextSector->searchKey = s_searchKey;
				s_sectorChangeList.push_back(nextSector);
			}
		}

		// Loop through potentially effected sectors and adjust mirrors.
		const size_t sectorCount = s_sectorChangeList.size();
		EditorSector** sectorList = s_sectorChangeList.data();
		for (size_t s = 0; s < sectorCount; s++)
		{
			EditorSector* matchSector = sectorList[s];
			if (matchSector == sector) { continue; }

			const size_t wallCount = matchSector->walls.size();
			EditorWall* wall = matchSector->walls.data();
			for (size_t w = 0; w < wallCount; w++, wall++)
			{
				if (wall->adjoinId != sector->id) { continue; }
				if (wall->mirrorId > wallIndex)
				{
					wall->mirrorId++;
				}
			}
		}
	}

	// Algorithm:
	// * Insert new wall after current wall.
	// * Find any references to sector with mirror > currentWall and fix-up (+1).
	// * If current wall has an adjoin, split the mirror wall.
	// * Find any references to the adjoined sector with mirror > currentWall mirror.
	// Return true if a split was required.
	bool edit_splitWall(s32 sectorId, s32 wallIndex, Vec2f newPos)
	{
		EditorSector* sector = &s_level.sectors[sectorId];
		// If a vertex at the desired position already exists, do not split the wall (early-out).
		const size_t vtxCount = sector->vtx.size();
		const Vec2f* vtx = sector->vtx.data();
		for (size_t v = 0; v < vtxCount; v++) { if (vtxEqual(&newPos, &vtx[v])) {return false;} }

		// Split the wall, insert the new wall after the original.
		// Example, split B into {B, N} : {A, B, C, D} -> {A, B, N, C, D}.
		EditorWall* outWalls[2] = { nullptr };
		splitWall(sector, wallIndex, newPos, outWalls);

		// Find any references to > wallIndex and fix them up.
		fixupWallMirrors(sector, wallIndex);

		// If the current wall is an adjoin, split the matching adjoin.
		if (outWalls[0]->adjoinId >= 0 && outWalls[0]->mirrorId >= 0)
		{
			EditorSector* nextSector = &s_level.sectors[outWalls[0]->adjoinId];
			const s32 mirrorWallIndex = outWalls[0]->mirrorId;

			// Split the mirror wall.
			EditorWall* outWallsAdjoin[2] = { nullptr };
			splitWall(nextSector, mirrorWallIndex, newPos, outWallsAdjoin);
	
			// Fix-up the mirrors for the adjoined sector.
			fixupWallMirrors(nextSector, mirrorWallIndex);

			// Connect the split edges together.
			outWalls[0]->mirrorId = mirrorWallIndex + 1;
			outWalls[1]->mirrorId = mirrorWallIndex;
			outWallsAdjoin[0]->mirrorId = wallIndex + 1;
			outWallsAdjoin[1]->mirrorId = wallIndex;

			// Fix-up the adjoined sector polygon.
			sectorToPolygon(nextSector);
		}
		// Fix-up the sector polygon.
		sectorToPolygon(sector);
		return true;
	}
		
	void selectVerticesToDelete(EditorSector* root, s32 featureIndex, const Vec2f* rootVtx)
	{
		s_searchKey++;

		// Which walls share the vertex?
		std::vector<EditorSector*> stack;

		root->searchKey = s_searchKey;
		{
			const s32 wallCount = (s32)root->walls.size();
			EditorWall* wall = root->walls.data();
			s32 leftIdx = -1, rightIdx = -1;
			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				if (wall->idx[0] == featureIndex || wall->idx[1] == featureIndex)
				{
					if (wall->idx[0] == featureIndex) { rightIdx = w; }
					else { leftIdx = w; }

					if (wall->adjoinId >= 0)
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

			if (leftIdx >= 0 || rightIdx >= 0)
			{
				s_vertexWallGroups.push_back({ root, featureIndex, leftIdx, rightIdx });
			}
		}

		while (!stack.empty())
		{
			EditorSector* sector = stack.back();
			stack.pop_back();

			const s32 wallCount = (s32)sector->walls.size();
			EditorWall* wall = sector->walls.data();
			s32 leftIdx = -1, rightIdx = -1;
			s32 vtxId = -1;
			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				s32 i = -1;
				if (vtxEqual(rootVtx, &sector->vtx[wall->idx[0]]))
				{
					i = 0;
					rightIdx = w;
					vtxId = wall->idx[0];
				}
				else if (vtxEqual(rootVtx, &sector->vtx[wall->idx[1]]))
				{
					i = 1;
					leftIdx = w;
					vtxId = wall->idx[1];
				}

				if (i >= 0)
				{
					// Add the next sector to the stack, if it hasn't already been processed.
					EditorSector* next = wall->adjoinId < 0 ? nullptr : &s_level.sectors[wall->adjoinId];
					if (next && next->searchKey != s_searchKey)
					{
						stack.push_back(next);
						next->searchKey = s_searchKey;
					}
				}
			}

			if ((leftIdx >= 0 || rightIdx >= 0) && vtxId >= 0)
			{
				s_vertexWallGroups.push_back({ sector, vtxId, leftIdx, rightIdx });
			}
		}
	}

	void deleteVertex(EditorSector* sector, s32 vertexIndex)
	{
		// First delete the vertex.
		sector->vtx.erase(sector->vtx.begin() + vertexIndex);

		// Then fix-up the indices.
		const s32 wallCount = (s32)sector->walls.size();
		EditorWall* wall = sector->walls.data();
		for (s32 w = 0; w < wallCount; w++, wall++)
		{
			assert(wall->idx[0] != vertexIndex && wall->idx[1] != vertexIndex);
			if (wall->idx[0] > vertexIndex) { wall->idx[0]--; }
			if (wall->idx[1] > vertexIndex) { wall->idx[1]--; }
		}
	}

	bool sortBySectorId(EditorSector*& a, EditorSector*& b)
	{
		return a->id > b->id;
	}

	void deleteInvalidSectors(SectorList& list)
	{
		// Sort from highest id to lowest.
		std::sort(list.begin(), list.end(), sortBySectorId);

		// Then loop through the list and delete invalid sectors.
		//   * Adjoins referencing a deleted sector must be removed.
		//   * Adjoins referencing sectors by a higher ID must be modified.
		//   * Deleted sectors must be removed from the list.
		std::vector<s32> itemsToDelete;

		const s32 count = (s32)list.size();
		for (s32 i = 0; i < count; i++)
		{
			EditorSector* sector = list[i];
			// For now just look at vertex count, later remove degenerates?
			if (sector->vtx.size() >= 3) { continue; }

			// *TODO*
			// Is the sector degenerate (aka, has no area)?
			
			// *TODO*
			// Go through the walls, we need to delete any mirrors - but only if this is a sub-sector.

			// Get the ID and then erase it from the level.
			s32 delId = sector->id;
			s_level.sectors.erase(s_level.sectors.begin() + delId);

			// Update Sector IDs
			const s32 levSectorCount = (s32)s_level.sectors.size();
			sector = s_level.sectors.data() + delId;
			for (s32 s = delId; s < levSectorCount; s++, sector++)
			{
				sector->id--;
			}

			// Update Adjoins.
			sector = s_level.sectors.data();
			for (s32 s = 0; s < levSectorCount; s++, sector++)
			{
				const s32 wallCount = (s32)sector->walls.size();
				EditorWall* wall = sector->walls.data();
				for (s32 w = 0; w < wallCount; w++, wall++)
				{
					if (wall->adjoinId == delId)
					{
						// Remove adjoin since the sector no longer exists.
						wall->adjoinId = -1;
						wall->mirrorId = -1;
					}
					else if (wall->adjoinId > delId)
					{
						// All of the sector IDs after delID shifted down by 1.
						wall->adjoinId--;
					}
				}
			}

			// Add to the erase list.
			itemsToDelete.push_back(i);
		}

		// Erase sectors from the list since later fixup is not required.
		const s32 delCount = (s32)itemsToDelete.size();
		const s32* delList = itemsToDelete.data();
		for (s32 i = delCount - 1; i >= 0; i--)
		{
			list.erase(list.begin() + delList[i]);
		}
	}

	// This is an N^2 operation, the sector list should be as limited as possible.
	void findAdjoinsInList(SectorList& list)
	{
		const s32 sectorCount = (s32)list.size();
		EditorSector** sectorList = list.data();
		for (s32 s0 = 0; s0 < sectorCount; s0++)
		{
			EditorSector* sector0 = sectorList[s0];
			const s32 wallCount0 = (s32)sector0->walls.size();
			EditorWall* wall0 = sector0->walls.data();
			for (s32 w0 = 0; w0 < wallCount0; w0++, wall0++)
			{
				if (wall0->adjoinId >= 0 && wall0->mirrorId >= 0) { continue; }
				const Vec2f* v0 = &sector0->vtx[wall0->idx[0]];
				const Vec2f* v1 = &sector0->vtx[wall0->idx[1]];

				// Now check for overlaps in other sectors in the list.
				bool foundMirror = false;
				for (s32 s1 = 0; s1 < sectorCount && !foundMirror; s1++)
				{
					EditorSector* sector1 = sectorList[s1];
					if (sector1 == sector0) { continue; }

					const s32 wallCount1 = (s32)sector1->walls.size();
					EditorWall* wall1 = sector1->walls.data();
					for (s32 w1 = 0; w1 < wallCount1 && !foundMirror; w1++, wall1++)
					{
						if (wall1->adjoinId >= 0 && wall1->mirrorId >= 0) { continue; }
						const Vec2f* v2 = &sector1->vtx[wall1->idx[0]];
						const Vec2f* v3 = &sector1->vtx[wall1->idx[1]];

						if (vtxEqual(v0, v3) && vtxEqual(v1, v2))
						{
							// Found an adjoin!
							wall0->adjoinId = sector1->id;
							wall0->mirrorId = w1;
							wall1->adjoinId = sector0->id;
							wall1->mirrorId = w0;
							// For now, assume one adjoin per wall.
							foundMirror = true;
						}
						else if (vtxEqual(v0, v2) && vtxEqual(v1, v3))
						{
							assert(0);
						}
					}
				}
			}
		}
	}

	// Get the proper mirror ID, assuming the existing ID is no longer valid.
	s32 getMirrorId(EditorSector* sector, s32 wallId)
	{
		const EditorWall* curWall = &sector->walls[wallId];
		const EditorSector* next = curWall->adjoinId < 0 ? nullptr : &s_level.sectors[curWall->adjoinId];
		if (!next) { return -1; }

		const Vec2f* v0 = &sector->vtx[curWall->idx[0]];
		const Vec2f* v1 = &sector->vtx[curWall->idx[1]];
		
		const s32 wallCount = (s32)next->walls.size();
		const EditorWall* wall = next->walls.data();
		for (s32 w = 0; w < wallCount; w++, wall++)
		{
			const Vec2f* v2 = &next->vtx[wall->idx[0]];
			const Vec2f* v3 = &next->vtx[wall->idx[1]];

			if ((vtxEqual(v0, v3) && vtxEqual(v1, v2)) || (vtxEqual(v0, v2) && vtxEqual(v1, v3)))
			{
				return w;
			}
		}
		return -1;
	}

	// Delete a vertex.
	void edit_deleteVertex(s32 sectorId, s32 vertexIndex)
	{
		EditorSector* sector = &s_level.sectors[sectorId];
		const Vec2f* pos = &sector->vtx[vertexIndex];

		// Gather a list of walls and vertices.
		s_vertexWallGroups.clear();
		selectVerticesToDelete(sector, vertexIndex, pos);

		s_searchKey++;
		s_sectorChangeList.clear();
				
		const s32 wallGroupCount = (s32)s_vertexWallGroups.size();
		VertexWallGroup* group = s_vertexWallGroups.data();
		for (s32 i = 0; i < wallGroupCount; i++, group++)
		{
			EditorSector* sector = group->sector;
			// Gather sectors to later re-generate adjoins.
			if (sector->searchKey != s_searchKey)
			{
				sector->searchKey = s_searchKey;
				s_sectorChangeList.push_back(sector);

				// Add adjoined sectors as well.
				const s32 wallCount = (s32)sector->walls.size();
				EditorWall* wall = sector->walls.data();
				for (s32 w = 0; w < wallCount; w++, wall++)
				{
					EditorSector* next = wall->adjoinId < 0 ? nullptr : &s_level.sectors[wall->adjoinId];
					if (next && next->searchKey != s_searchKey)
					{
						next->searchKey = s_searchKey;
						s_sectorChangeList.push_back(next);
					}
				}
			}

			// Merge walls in group.
			if (group->leftWall >= 0 && group->rightWall >= 0)
			{
				EditorWall* left  = &sector->walls[group->leftWall];
				EditorWall* right = &sector->walls[group->rightWall];
				left->idx[1] = right->idx[1];
								
				// Erase adjoins, these will be fixed up later.
				if (left->adjoinId >= 0)
				{
					const s32 mirrorId = getMirrorId(sector, group->leftWall);
					EditorSector* next = &s_level.sectors[left->adjoinId];
					if (mirrorId >= 0 && mirrorId < (s32)next->walls.size())
					{
						next->walls[mirrorId].adjoinId = -1;
						next->walls[mirrorId].mirrorId = -1;
					}
				}
				if (right->adjoinId >= 0 && right->mirrorId >= 0)
				{
					const s32 mirrorId = getMirrorId(sector, group->rightWall);
					EditorSector* next = &s_level.sectors[right->adjoinId];
					if (mirrorId >= 0 && mirrorId < (s32)next->walls.size())
					{
						next->walls[mirrorId].adjoinId = -1;
						next->walls[mirrorId].mirrorId = -1;
					}
				}
				left->adjoinId = -1;
				left->mirrorId = -1;

				sector->walls.erase(sector->walls.begin() + group->rightWall);
			}
			else if (group->leftWall >= 0)
			{
				EditorWall* left = &sector->walls[group->leftWall];
				if (left->adjoinId >= 0)
				{
					const s32 mirrorId = getMirrorId(sector, group->leftWall);
					EditorSector* next = &s_level.sectors[left->adjoinId];
					if (mirrorId >= 0 && mirrorId < (s32)next->walls.size())
					{
						next->walls[mirrorId].adjoinId = -1;
						next->walls[mirrorId].mirrorId = -1;
					}
				}
				sector->walls.erase(sector->walls.begin() + group->leftWall);
			}
			else if (group->rightWall >= 0)
			{
				EditorWall* right = &sector->walls[group->rightWall];
				if (right->adjoinId >= 0)
				{
					const s32 mirrorId = getMirrorId(sector, group->rightWall);
					EditorSector* next = &s_level.sectors[right->adjoinId];
					if (mirrorId >= 0 && mirrorId < (s32)next->walls.size())
					{
						next->walls[mirrorId].adjoinId = -1;
						next->walls[mirrorId].mirrorId = -1;
					}
				}
				sector->walls.erase(sector->walls.begin() + group->rightWall);
			}

			deleteVertex(sector, group->vertexIndex);
			sectorToPolygon(sector);
		}

		// Remove any invalid sectors.
		deleteInvalidSectors(s_sectorChangeList);

		// Then find adjoins.
		findAdjoinsInList(s_sectorChangeList);

		// When deleting features, we need to clear out the selections.
		edit_clearSelections();
	}

	void edit_deleteSector(s32 sectorId)
	{
		// First, go through all sectors and remove references.
		s32 sectorCount = (s32)s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			if (sector->id == sectorId) { continue; }

			const s32 wallCount = (s32)sector->walls.size();
			EditorWall* wall = sector->walls.data();
			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				if (wall->adjoinId == sectorId)
				{
					wall->adjoinId = -1;
					wall->mirrorId = -1;
				}
			}
		}

		// Update the sector IDs.
		sector = s_level.sectors.data();
		for (s32 s = sectorId + 1; s < sectorCount; s++)
		{
			sector[s].id--;
		}

		// Then erase the sector.
		s_level.sectors.erase(s_level.sectors.begin() + sectorId);

		// Finally fix-up any references.
		sectorCount = (s32)s_level.sectors.size();
		sector = s_level.sectors.data();
		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			const s32 wallCount = (s32)sector->walls.size();
			EditorWall* wall = sector->walls.data();
			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				assert(wall->adjoinId != sectorId);
				if (wall->adjoinId > sectorId)
				{
					wall->adjoinId--;
				}
			}
		}

		// Selections are potentially invalid so clear.
		edit_clearSelections();
	}

	void handleVertexInsert(Vec2f worldPos)
	{
		const bool mousePressed = TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT);
		const bool mouseDown = TFE_Input::mouseDown(MouseButton::MBUTTON_LEFT);
		// TODO: Hotkeys.
		const bool insertVtx = TFE_Input::keyPressed(KEY_INSERT);
		const bool snapToGridEnabled = !TFE_Input::keyModDown(KEYMOD_ALT) && s_gridSize > 0.0f;

		// Early out.
		if (!insertVtx || s_editMove || mouseDown || mousePressed) { return; }

		EditorSector* sector = nullptr;
		s32 wallIndex = -1;
		if (s_editMode == LEDIT_WALL && s_featureHovered.part != HP_FLOOR && s_featureHovered.part != HP_CEIL)
		{
			sector = s_featureHovered.sector;
			wallIndex = s_featureHovered.featureIndex;
		}
		else if (s_editMode == LEDIT_VERTEX && s_featureCurWall.sector && s_featureCurWall.part != HP_FLOOR && s_featureCurWall.part != HP_CEIL)
		{
			// Determine the wall under the cursor.
			sector = s_featureCurWall.sector;
			wallIndex = s_featureCurWall.featureIndex;
		}

		// Return if no wall is found in range.
		if (!sector || wallIndex < 0) { return; }

		EditorWall* wall = &sector->walls[wallIndex];
		Vec2f v0 = sector->vtx[wall->idx[0]];
		Vec2f v1 = sector->vtx[wall->idx[1]];
		Vec2f newPos = v0;

		f32 s = closestPointOnLineSegment(v0, v1, worldPos, &newPos);
		if (snapToGridEnabled)
		{
			// Determine which projection we are using (XY or ZY).
			// This should match the way grids are rendered on surfaces.
			const f32 dx = fabsf(v1.x - v0.x);
			const f32 dz = fabsf(v1.z - v0.z);
			if (dx >= dz)  // X-intersect with line segment.
			{
				snapToGrid(&newPos.x);
				s = (newPos.x - v0.x) / (v1.x - v0.x);
			}
			else  // Z-intersect with line segment.
			{
				snapToGrid(&newPos.z);
				s = (newPos.z - v0.z) / (v1.z - v0.z);
			}
			newPos = { v0.x + s * (v1.x - v0.x), v0.z + s * (v1.z - v0.z) };
		}

		if (s > FLT_EPSILON && s < 1.0f - FLT_EPSILON)
		{
			// Split the wall.
			cmd_addInsertVertex(sector->id, wallIndex, newPos);
			edit_splitWall(sector->id, wallIndex, newPos);
		}
	}

	void handleMouseControlWall(Vec2f worldPos)
	{
		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);

		// Short names to make the logic easier to follow.
		const bool selAdd = TFE_Input::keyModDown(KEYMOD_SHIFT);
		const bool selRem = TFE_Input::keyModDown(KEYMOD_ALT);
		const bool selToggle = TFE_Input::keyModDown(KEYMOD_CTRL);
		const bool selToggleDrag = selAdd && selToggle;

		const bool mousePressed = TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT);
		const bool mouseDown = TFE_Input::mouseDown(MouseButton::MBUTTON_LEFT);

		const bool del = TFE_Input::keyPressed(KEY_DELETE);

		if (del && (!s_selectionList.empty() || s_featureCur.featureIndex >= 0 || s_featureHovered.featureIndex >= 0))
		{
			s32 sectorId, wallIndex;
			HitPart part;
			if (!s_selectionList.empty())
			{
				// TODO: Currently, you can only delete one vertex at a time. It should be possible to delete multiple.
				EditorSector* featureSector = unpackFeatureId(s_selectionList[0], &wallIndex, (s32*)&part);
				sectorId = featureSector->id;
			}
			else if (s_featureCur.featureIndex >= 0 && s_featureCur.sector)
			{
				sectorId = s_featureCur.sector->id;
				wallIndex = s_featureCur.featureIndex;
				part = s_featureCur.part;
			}
			else if (s_featureHovered.featureIndex >= 0 && s_featureHovered.sector)
			{
				sectorId = s_featureHovered.sector->id;
				wallIndex = s_featureHovered.featureIndex;
				part = s_featureHovered.part;
			}

			if (part == HP_FLOOR || part == HP_CEIL)
			{
				edit_deleteSector(sectorId);
				cmd_addDeleteSector(sectorId);
			}
			else
			{
				// Deleting a wall is the same as deleting vertex 0.
				// So re-use the same command, but with the delete wall name.
				const s32 vertexIndex = s_level.sectors[sectorId].walls[wallIndex].idx[0];
				edit_deleteVertex(sectorId, vertexIndex);
				cmd_addDeleteVertex(sectorId, vertexIndex, LName_DeleteWall);
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
				if (s_featureHovered.sector && s_featureHovered.featureIndex >= 0)
				{
					s_editMove = true;
					adjustGridHeight(s_featureHovered.sector);
					toggleWallGroupInclusion(s_featureHovered.sector, s_featureHovered.featureIndex, s_featureHovered.part);
				}
			}
			else
			{
				s_featureCur.sector = nullptr;
				s_featureCur.featureIndex = -1;
				if (s_featureHovered.sector && s_featureHovered.featureIndex >= 0)
				{
					FeatureId id = createFeatureId(s_featureHovered.sector, s_featureHovered.featureIndex, s_featureHovered.part);
					bool doesItemExist = selection_doesFeatureExist(id);
					if (s_selectionList.size() <= 1 || doesItemExist)
					{
						s_featureCur.sector = s_featureHovered.sector;
						s_featureCur.featureIndex = s_featureHovered.featureIndex;
						s_featureCur.part = s_featureHovered.part;
						// Set this to the 3D cursor position, so the wall doesn't "pop" when grabbed.
						s_curVtxPos = s_cursor3d;
						adjustGridHeight(s_featureCur.sector);
						s_editMove = true;

						// TODO: Hotkeys...
						s_wallMoveMode = WMM_FREE;
						if (TFE_Input::keyDown(KEY_T))
						{
							s_wallMoveMode = WMM_TANGENT;
						}
						else if (TFE_Input::keyDown(KEY_N))
						{
							s_wallMoveMode = WMM_NORMAL;
						}

						if (!doesItemExist) { selection_clear(); }
						selection_add(id);
						gatherVerticesFromWallSelection();
					}
				}
				else if (!s_editMove)
				{
					startDragSelect(mx, my, DSEL_SET);
				}
			}
		}
		else if (mouseDown)
		{
			if (!s_dragSelect.active)
			{
				if (selToggleDrag && s_featureHovered.sector && s_featureHovered.featureIndex >= 0)
				{
					s_editMove = true;
					adjustGridHeight(s_featureHovered.sector);

					FeatureId id = createFeatureId(s_featureHovered.sector, s_featureHovered.featureIndex, s_featureHovered.part);
					selection_add(id);
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

		handleVertexInsert(worldPos);
	}

	void handleMouseControlSector()
	{
		const bool del = TFE_Input::keyPressed(KEY_DELETE);

		if (del && (!s_selectionList.empty() || s_featureCur.sector || s_featureHovered.sector))
		{
			s32 sectorId = -1;
			if (!s_selectionList.empty())
			{
				// TODO: Currently, you can only delete one vertex at a time. It should be possible to delete multiple.
				s32 index;
				EditorSector* featureSector = unpackFeatureId(s_selectionList[0], &index);
				sectorId = featureSector->id;
			}
			else if (s_featureCur.sector)
			{
				sectorId = s_featureCur.sector->id;
			}
			else if (s_featureHovered.sector)
			{
				sectorId = s_featureHovered.sector->id;
			}

			if (sectorId >= 0)
			{
				edit_deleteSector(sectorId);
				cmd_addDeleteSector(sectorId);
			}
			return;
		}

		if (TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
		{
			s_featureCur.sector = s_featureHovered.sector;
			s_featureCur.featureIndex = 0;
			adjustGridHeight(s_featureCur.sector);
		}
		if (s_featureHovered.sector)
		{
			s_featureHovered.featureIndex = 0;
		}
	}

	void checkForWallHit3d(RayHitInfo* info, EditorSector*& wallSector, s32& wallIndex, HitPart& part, const EditorSector* hoverSector)
	{
		// See if we are close enough to "hover" a vertex
		wallIndex = -1;
		if (info->hitWallId >= 0 && info->hitSectorId >= 0)
		{
			wallSector = &s_level.sectors[info->hitSectorId];
			wallIndex = info->hitWallId;
			part = info->hitPart;
		}
		else
		{
			// Are we close enough to an edge?
			// Project the point onto the floor.
			const Vec2f pos2d = { info->hitPos.x, info->hitPos.z };
			const f32 distFromCam = TFE_Math::distance(&info->hitPos, &s_camera.pos);
			const f32 maxDist = distFromCam * 16.0f / f32(s_viewportSize.z);
			const f32 maxDistSq = maxDist * maxDist;
			wallIndex = findClosestWallInSector(hoverSector, &pos2d, maxDist * maxDist, nullptr);
			wallSector = (EditorSector*)hoverSector;
			if (wallIndex >= 0)
			{
				const EditorWall* wall = &hoverSector->walls[wallIndex];
				const EditorSector* next = wall->adjoinId >= 0 ? &s_level.sectors[wall->adjoinId] : nullptr;
				if (!next)
				{
					part = HP_MID;
				}
				else if (info->hitPart == HP_FLOOR)
				{
					part = (next->floorHeight > hoverSector->floorHeight) ? HP_BOT : HP_MID;
				}
				else if (info->hitPart == HP_CEIL)
				{
					part = (next->ceilHeight < hoverSector->ceilHeight) ? HP_TOP : HP_MID;
				}
			}
			else
			{
				// Otherwise, select the floor or ceiling...
				if (info->hitPart == HP_FLOOR)
				{
					part = HP_FLOOR;
					wallIndex = 0;
				}
				else if (info->hitPart == HP_CEIL)
				{
					part = HP_CEIL;
					wallIndex = 0;
				}
			}
		}
	}

	void handleHoverAndSelection(RayHitInfo* info)
	{
		if (info->hitSectorId < 0) { return; }
		s_featureHovered.sector = &s_level.sectors[info->hitSectorId];
		s_featureHovered.featureIndex = -1;

		// TODO: Move to central hotkey list.
		if (s_featureHovered.sector && TFE_Input::keyModDown(KEYMOD_CTRL) && TFE_Input::keyPressed(KEY_G))
		{
			adjustGridHeight(s_featureHovered.sector);
		}

		//////////////////////////////////////
		// SECTOR
		//////////////////////////////////////
		if (s_editMode == LEDIT_SECTOR)
		{
			handleMouseControlSector();
		}

		//////////////////////////////////////
		// VERTEX
		//////////////////////////////////////
		if (s_editMode == LEDIT_VERTEX)
		{
			s32 prevId = s_featureHovered.featureIndex;
			// See if we are close enough to "hover" a vertex
			s_featureHovered.featureIndex = -1;
			if (s_featureHovered.sector)
			{
				EditorSector* hoveredSector = s_featureHovered.sector;

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
						closestVtx = (s32)v;
						finalPos = floorDistSq <= ceilDistSq ? floorVtx : ceilVtx;
					}
				}

				if (closestVtx >= 0)
				{
					s_featureHovered.featureIndex = closestVtx;
					s_hoveredVtxPos = finalPos;
				}

				// Also check for the overlapping wall and store it in a special feature.
				// This is used for features such as vertex insertion even when not in wall mode.
				s_featureCurWall = {};
				checkForWallHit3d(info, s_featureCurWall.sector, s_featureCurWall.featureIndex, s_featureCurWall.part, hoveredSector);
			}

			handleMouseControlVertex({ s_cursor3d.x, s_cursor3d.z });
		}

		//////////////////////////////////////
		// WALL
		//////////////////////////////////////
		if (s_editMode == LEDIT_WALL)
		{
			// See if we are close enough to "hover" a vertex
			const EditorSector* hoveredSector = s_featureHovered.sector;
			s_featureHovered.featureIndex = -1;
			checkForWallHit3d(info, s_featureHovered.sector, s_featureHovered.featureIndex, s_featureHovered.part, hoveredSector);
			handleMouseControlWall({ s_cursor3d.x, s_cursor3d.z });
		}
	}

	void checkForWallHit2d(Vec2f& worldPos, EditorSector*& wallSector, s32& wallIndex, HitPart& part, EditorSector* hoverSector)
	{
		// See if we are close enough to "hover" a wall
		wallIndex = -1;
		wallSector = nullptr;
		if (hoverSector)
		{
			const f32 maxDist = s_zoom2d * 16.0f;
			wallIndex = findClosestWallInSector(hoverSector, &worldPos, maxDist * maxDist, nullptr);
			if (wallIndex >= 0)
			{
				wallSector = hoverSector;
				part = HP_MID;
			}
			else
			{
				wallSector = nullptr;
			}
		}
	}
		
	void shapeToPolygon(s32 count, Vec2f* vtx, Polygon& poly)
	{
		poly.edge.resize(count);
		poly.vtx.resize(count);

		poly.bounds[0] = { FLT_MAX,  FLT_MAX };
		poly.bounds[1] = { -FLT_MAX, -FLT_MAX };

		for (size_t v = 0; v < count; v++, vtx++)
		{
			poly.vtx[v] = *vtx;
			poly.bounds[0].x = std::min(poly.bounds[0].x, vtx->x);
			poly.bounds[0].z = std::min(poly.bounds[0].z, vtx->z);
			poly.bounds[1].x = std::max(poly.bounds[1].x, vtx->x);
			poly.bounds[1].z = std::max(poly.bounds[1].z, vtx->z);
		}

		for (s32 w = 0; w < count; w++)
		{
			s32 a = w;
			s32 b = (w + 1) % count;
			poly.edge[w] = { a, b };
		}

		// Clear out cached triangle data.
		poly.triVtx.clear();
		poly.triIdx.clear();

		TFE_Polygon::computeTriangulation(&poly);
	}

	void createNewSector(EditorSector* sector)
	{
		*sector = {};
		sector->id = (s32)s_level.sectors.size();
		// Just copy for now.
		sector->floorTex.texIndex = s_level.sectors[0].floorTex.texIndex;
		sector->ceilTex.texIndex = s_level.sectors[0].ceilTex.texIndex;

		sector->floorHeight = std::min(s_drawHeight[0], s_drawHeight[1]);
		sector->ceilHeight  = std::max(s_drawHeight[0], s_drawHeight[1]);
		sector->ambient = 20;
		sector->layer   = s_curLayer;
	}
		
	bool edgesColinear(const Vec2f* points, s32& newPointCount, s32* newPoints)
	{
		const f32 eps = 0.00001f;
		newPointCount = 0;
		
		// Is v0 or v1 on v2->v3?
		Vec2f point;
		f32 s = closestPointOnLineSegment(points[2], points[3], points[0], &point);
		if (s > -eps && s < 1.0f + eps && vtxEqual(&points[0], &point)) { newPoints[newPointCount++] = 0 | EDGE1; }

		s = closestPointOnLineSegment(points[2], points[3], points[1], &point);
		if (s > -eps && s < 1.0f + eps && vtxEqual(&points[1], &point)) { newPoints[newPointCount++] = 1 | EDGE1; }
		
		// Is v2 or v3 on v0->v1?
		s = closestPointOnLineSegment(points[0], points[1], points[2], &point);
		if (s > -eps && s < 1.0f + eps && vtxEqual(&points[2], &point)) { newPoints[newPointCount++] = 2 | EDGE0; }

		s = closestPointOnLineSegment(points[0], points[1], points[3], &point);
		if (s > -eps && s < 1.0f + eps && vtxEqual(&points[3], &point)) { newPoints[newPointCount++] = 3 | EDGE0; }

		const bool areColinear = newPointCount >= 2;
		if (newPointCount > 2)
		{
			s32 p0 = newPoints[0] & 255, p1 = newPoints[1] & 255, p2 = newPoints[2] & 255;
			s32 d0 = -1, d1 = -1;
			if (vtxEqual(&points[p0], &points[p1]))
			{
				d0 = 0;
				d1 = 1;
			}
			else if (vtxEqual(&points[p0], &points[p2]))
			{
				d0 = 0;
				d1 = 2;
			}
			else if (vtxEqual(&points[p1], &points[p2]))
			{
				d0 = 1;
				d1 = 2;
			}

			// Disambiguate duplicates.
			if (d0 >= 0 && d1 >= 0)
			{
				// Keep the one that isn't already on the segment.
				s32 p01 = newPoints[d0] & 255;
				s32 p11 = newPoints[d1] & 255;
				s32 edge0 = newPoints[d0] >> 8;
				s32 edge1 = newPoints[d1] >> 8;
				s32 discard = d0;
				if (edge0 != edge1)
				{
					if ((edge1 == 0 && (vtxEqual(&points[p11], &points[0]) || vtxEqual(&points[p11], &points[1]))) ||
					    (edge1 == 1 && (vtxEqual(&points[p11], &points[2]) || vtxEqual(&points[p11], &points[3]))))
					{
						discard = d1;
					}
				}
				for (s32 i = discard; i < newPointCount - 1; i++)
				{
					newPoints[i] = newPoints[i + 1];
				}
				newPointCount--;
			}
		}
		return areColinear && !vtxEqual(&points[newPoints[0]&255], &points[newPoints[1]&255]);
	}

	void mergeAdjoins(EditorSector* sector0)
	{
		const s32 levelSectorCount = (s32)s_level.sectors.size();
		EditorSector* levelSectors = s_level.sectors.data();

		s32 id0 = sector0->id;

		// Build a list ahead of time, instead of traversing through all sectors multiple times.
		std::vector<EditorSector*> mergeList;
		for (s32 i = 0; i < levelSectorCount; i++)
		{
			EditorSector* sector1 = &levelSectors[i];
			if (sector1 == sector0) { continue; }

			// Cannot merge adjoins if the sectors don't overlap in 3D space.
			if (!aabbOverlap3d(sector0->bounds, sector1->bounds))
			{
				continue;
			}
			mergeList.push_back(sector1);
		}

		// Loop through each wall of the new sector and see if it can be adjoined to another sector/wall in the list created above.
		// Once colinear walls are found, properly splits are made and the loop is restarted. Adjoins are only added when exact
		// matches are found.
		bool restart = true;
		while (restart)
		{
			restart = false;
			const s32 sectorCount = (s32)mergeList.size();
			EditorSector** sectorList = mergeList.data();
			sector0 = &s_level.sectors[id0];

			s32 wallCount0 = (s32)sector0->walls.size();
			EditorWall* wall0 = sector0->walls.data();
			bool wallLoop = true;
			for (s32 w0 = 0; w0 < wallCount0 && wallLoop; w0++, wall0++)
			{
				if (wall0->adjoinId >= 0 && wall0->mirrorId >= 0) { continue; }
				const Vec2f* v0 = &sector0->vtx[wall0->idx[0]];
				const Vec2f* v1 = &sector0->vtx[wall0->idx[1]];

				// Now check for overlaps in other sectors in the list.
				bool foundMirror = false;
				for (s32 s1 = 0; s1 < sectorCount && !foundMirror; s1++)
				{
					EditorSector* sector1 = sectorList[s1];
					const s32 wallCount1 = (s32)sector1->walls.size();
					EditorWall* wall1 = sector1->walls.data();

					for (s32 w1 = 0; w1 < wallCount1 && !foundMirror; w1++, wall1++)
					{
						if (wall1->adjoinId >= 0 && wall1->mirrorId >= 0) { continue; }
						const Vec2f* v2 = &sector1->vtx[wall1->idx[0]];
						const Vec2f* v3 = &sector1->vtx[wall1->idx[1]];

						// Do the vertices match exactly?
						if (vtxEqual(v0, v3) && vtxEqual(v1, v2))
						{
							// Found an adjoin!
							wall0->adjoinId = sector1->id;
							wall0->mirrorId = w1;
							wall1->adjoinId = sector0->id;
							wall1->mirrorId = w0;
							// For now, assume one adjoin per wall.
							foundMirror = true;
							continue;
						}

						// Are these edges co-linear?
						s32 newPointCount;
						s32 newPoints[4];
						Vec2f points[] = { *v0, *v1, *v2, *v3 };
						if (edgesColinear(points, newPointCount, newPoints))
						{
							assert(newPointCount == 2);

							// 4 Cases:
							// 1. v0,v1 left of v2,v3
							if ((newPoints[0] >> 8) == 0 && (newPoints[1] >> 8) == 1)
							{
								assert((newPoints[0] & 255) >= 2);
								assert((newPoints[1] & 255) < 2);

								// Insert newPoints[0] & 255 into wall0
								edit_splitWall(sector0->id, w0, points[newPoints[0] & 255]);
								// Insert newPoints[1] & 255 into wall1
								edit_splitWall(sector1->id, w1, points[newPoints[1] & 255]);
							}
							// 2. v0,v1 right of v2,v3
							else if ((newPoints[0] >> 8) == 1 && (newPoints[1] >> 8) == 0)
							{
								assert((newPoints[0] & 255) < 2);
								assert((newPoints[1] & 255) >= 2);

								// Insert newPoints[0] & 255 into wall1
								edit_splitWall(sector1->id, w1, points[newPoints[0] & 255]);
								// Insert newPoints[1] & 255 into wall0
								edit_splitWall(sector0->id, w0, points[newPoints[1] & 255]);
							}
							// 3. v2,v3 inside of v0,v1
							else if ((newPoints[0] >> 8) == 0 && (newPoints[1] >> 8) == 0)
							{
								assert((newPoints[0] & 255) >= 2);
								assert((newPoints[1] & 255) >= 2);

								// Insert newPoints[0] & 255 into wall0
								// Insert newPoints[1] & 255 into wall0
								if (edit_splitWall(sector0->id, w0, points[newPoints[1] & 255])) { w0++; }
								edit_splitWall(sector0->id, w0, points[newPoints[0] & 255]);
							}
							// 4. v0,v1 inside of v2,v3
							else if ((newPoints[0] >> 8) == 1 && (newPoints[1] >> 8) == 1)
							{
								assert((newPoints[0] & 255) < 2);
								assert((newPoints[1] & 255) < 2);

								// Insert newPoints[0] & 255 into wall1
								// Insert newPoints[1] & 255 into wall1
								if (edit_splitWall(sector1->id, w1, points[newPoints[1] & 255])) { w1++; }
								edit_splitWall(sector1->id, w1, points[newPoints[0] & 255]);
							}
							restart = true;
							wallLoop = false;
							break;
						}
					}
				}
			}
		}

		const s32 sectorCount = (s32)mergeList.size();
		EditorSector** sectorList = mergeList.data();
		for (s32 s = 0; s < sectorCount; s++)
		{
			EditorSector* sector = sectorList[s];
			polygonToSector(sector);
		}
		polygonToSector(sector0);
	}

	void createSectorFromRect()
	{
		EditorSector newSector;
		createNewSector(&newSector);

		s32 count = 4;
		newSector.vtx.resize(count);
		newSector.walls.resize(count);

		const Vec2f* shapeVtx = s_shape.data();
		Vec2f corners[2];
		corners[0] = { std::max(shapeVtx[0].x, shapeVtx[1].x), std::min(shapeVtx[0].z, shapeVtx[1].z) };
		corners[1] = { std::min(shapeVtx[0].x, shapeVtx[1].x), std::max(shapeVtx[0].z, shapeVtx[1].z) };

		Vec2f rect[] =
		{
			{ corners[0].x, corners[0].z },
			{ corners[1].x, corners[0].z },
			{ corners[1].x, corners[1].z },
			{ corners[0].x, corners[1].z },
		};

		Vec2f* outVtx = newSector.vtx.data();
		for (s32 v = 0; v < count; v++)
		{
			outVtx[v] = rect[v];
		}

		EditorWall* wall = newSector.walls.data();
		for (s32 w = 0; w < count; w++, wall++)
		{
			s32 a = w;
			s32 b = (w + 1) % count;

			*wall = {};
			wall->idx[0] = a;
			wall->idx[1] = b;

			// Just copy for now...
			wall->tex[WP_MID].texIndex = s_level.sectors[0].walls[0].tex[WP_MID].texIndex;
			wall->tex[WP_TOP].texIndex = wall->tex[WP_MID].texIndex;
			wall->tex[WP_BOT].texIndex = wall->tex[WP_MID].texIndex;
		}

		s_level.sectors.push_back(newSector);
		sectorToPolygon(&s_level.sectors.back());

		mergeAdjoins(&s_level.sectors.back());
	}
		
	void createSectorFromShape()
	{
		EditorSector newSector = {};
		createNewSector(&newSector);

		const s32 count = (s32)s_shape.size();
		newSector.vtx.resize(count);
		newSector.walls.resize(count);

		const f32 area = TFE_Polygon::signedArea((s32)s_shape.size(), s_shape.data());
		Vec2f* outVtx = newSector.vtx.data();
		if (area >= 0.0f)
		{
			// If area is positive, than the polygon winding is clockwise, which is what we want.
			for (s32 v = 0; v < count; v++)
			{
				outVtx[v] = s_shape[v];
			}
		}
		else
		{
			// If area is negative, than the polygon winding is counter-clockwise, so read the vertices in reverse-order.
			for (s32 v = 0; v < count; v++)
			{
				outVtx[v] = s_shape[count - v - 1];
			}
		}

		EditorWall* wall = newSector.walls.data();
		for (s32 w = 0; w < count; w++, wall++)
		{
			s32 a = w;
			s32 b = (w + 1) % count;

			*wall = {};
			wall->idx[0] = a;
			wall->idx[1] = b;

			// Just copy for now...
			wall->tex[WP_MID].texIndex = s_level.sectors[0].walls[0].tex[WP_MID].texIndex;
			wall->tex[WP_TOP].texIndex = wall->tex[WP_MID].texIndex;
			wall->tex[WP_BOT].texIndex = wall->tex[WP_MID].texIndex;
		}

		s_level.sectors.push_back(newSector);
		sectorToPolygon(&s_level.sectors.back());

		mergeAdjoins(&s_level.sectors.back());
	}

	void removeLastShapePoint()
	{
		// Remove the last place vertex.
		if (!s_shape.empty())
		{
			s_shape.pop_back();
		}
		// If no vertices are left, cancel drawing.
		if (s_shape.empty())
		{
			s_drawStarted = false;
		}
	}

	void handleSectorDraw(RayHitInfo* hitInfo)
	{
		EditorSector* hoverSector = nullptr;

		// Get the hover sector in 2D.
		if (s_view == EDIT_VIEW_2D)
		{
			hoverSector = s_featureHovered.sector;
		}
		else // Or the hit sector in 3D.
		{
			hoverSector = hitInfo->hitSectorId < 0 ? nullptr : &s_level.sectors[hitInfo->hitSectorId];
		}

		// Snap the cursor to the grid.
		Vec2f onGrid = { s_cursor3d.x, s_cursor3d.z };
		snapToGrid(&onGrid);

		s_cursor3d.x = onGrid.x;
		s_cursor3d.z = onGrid.z;

		// For now just draw independent sector.

		// Two ways to draw: rectangle (shift + left click and drag, escape to cancel), shape (left click to start, backspace to go back one point, escape to cancel)
		if (s_drawStarted)
		{
			s_drawCurPos = onGrid;
			if (s_drawMode == DMODE_RECT)
			{
				s_shape[1] = onGrid;
				if (TFE_Input::keyPressed(KEY_ESCAPE))
				{
					// CANCEL
					s_drawStarted = false;
				}
				else if (!TFE_Input::mouseDown(MouseButton::MBUTTON_LEFT))
				{
					if (s_view == EDIT_VIEW_3D)
					{
						s_drawMode = DMODE_RECT_VERT;
						s_curVtxPos = { onGrid.x, s_gridHeight, onGrid.z };
					}
					else
					{
						s_drawHeight[1] = s_drawHeight[0] + c_defaultSectorHeight;
						s_drawStarted = false;
						createSectorFromRect();
					}
				}
			}
			else if (s_drawMode == DMODE_SHAPE)
			{
				if (TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
				{
					if (vtxEqual(&onGrid, &s_shape[0]))
					{
						if (s_view == EDIT_VIEW_3D)
						{
							s_drawMode = DMODE_SHAPE_VERT;
							s_curVtxPos = { onGrid.x, s_gridHeight, onGrid.z };
							shapeToPolygon((s32)s_shape.size(), s_shape.data(), s_shapePolygon);
						}
						else
						{
							s_drawHeight[1] = s_drawHeight[0] + c_defaultSectorHeight;
							s_drawStarted = false;
							createSectorFromShape();
						}
					}
					else
					{
						s_shape.push_back(onGrid);
					}
				}
				else if (TFE_Input::keyPressed(KEY_BACKSPACE))
				{
					removeLastShapePoint();
				}
				else if (TFE_Input::keyPressed(KEY_RETURN))
				{
					if (s_view == EDIT_VIEW_3D)
					{
						s_drawMode = DMODE_SHAPE_VERT;
						s_curVtxPos = { onGrid.x, s_gridHeight, onGrid.z };
						shapeToPolygon((s32)s_shape.size(), s_shape.data(), s_shapePolygon);
					}
					else
					{
						s_drawHeight[1] = s_drawHeight[0] + c_defaultSectorHeight;
						s_drawStarted = false;
						createSectorFromShape();
					}
				}
				else if (TFE_Input::keyPressed(KEY_ESCAPE))
				{
					// CANCEL
					s_drawStarted = false;
				}
			}
			else if (s_drawMode == DMODE_RECT_VERT || s_drawMode == DMODE_SHAPE_VERT)
			{
				if (TFE_Input::keyPressed(KEY_ESCAPE))
				{
					// CANCEL
					s_drawStarted = false;
				}
				else if (TFE_Input::keyPressed(KEY_BACKSPACE) && s_drawMode == DMODE_SHAPE_VERT)
				{
					s_drawMode = DMODE_SHAPE;
					s_drawHeight[1] = s_drawHeight[0];
					removeLastShapePoint();
				}
				else if (TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
				{
					s_drawStarted = false;
					if (s_drawMode == DMODE_SHAPE_VERT) { createSectorFromShape(); }
					else { createSectorFromRect(); }
				}

				Vec3f worldPos = moveAlongRail({ 0.0f, 1.0f, 0.0f });
				s_drawHeight[1] = worldPos.y;
				snapToGrid(&s_drawHeight[1]);
			}
		}
		else if (TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
		{
			if (hoverSector)
			{
				s_gridHeight = hoverSector->floorHeight;
			}

			s_drawStarted = true;
			s_drawMode = TFE_Input::keyModDown(KEYMOD_SHIFT) ? DMODE_RECT : DMODE_SHAPE;
			s_drawHeight[0] = s_gridHeight;
			s_drawHeight[1] = s_gridHeight;
			s_drawCurPos = onGrid;

			s_shape.clear();
			if (s_drawMode == DMODE_RECT)
			{
				s_shape.resize(2);
				s_shape[0] = onGrid;
				s_shape[1] = onGrid;
			}
			else
			{
				s_shape.push_back(onGrid);
			}
		}
		else
		{
			s_drawCurPos = onGrid;
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
			s_featureHovered = {};
			return;
		}

		if (s_view == EDIT_VIEW_2D)
		{
			cameraControl2d(mx, my);

			// Selection
			Vec2f worldPos = mouseCoordToWorldPos2d(mx, my);
			s_cursor3d = { worldPos.x, 0.0f, worldPos.z };

			// First check to see if the current hovered sector is still valid.
			if (s_featureHovered.sector)
			{
				if (!isPointInsideSector2d(s_featureHovered.sector, worldPos, s_curLayer))
				{
					s_featureHovered.sector = nullptr;
				}
			}
			// If not, then try to find one.
			if (!s_featureHovered.sector)
			{
				s_featureHovered.sector = findSector2d(worldPos, s_curLayer);
			}
			s_featureHovered.featureIndex = -1;

			// TODO: Move to central hotkey list.
			if (s_featureHovered.sector && TFE_Input::keyModDown(KEYMOD_CTRL) && TFE_Input::keyPressed(KEY_G))
			{
				adjustGridHeight(s_featureHovered.sector);
			}

			if (s_editMode == LEDIT_DRAW)
			{
				handleSectorDraw(nullptr/*2d so no ray hit info*/);
				return;
			}
				
			if (s_editMode == LEDIT_SECTOR)
			{
				handleMouseControlSector();
			}

			if (s_editMode == LEDIT_VERTEX)
			{
				// Keep track of the last vertex hovered sector and use it if no hovered sector is active to
				// make selecting vertices less fiddly.
				EditorSector* hoveredSector = s_featureHovered.sector ? s_featureHovered.sector : s_featureHovered.prevSector;

				// See if we are close enough to "hover" a vertex
				s_featureHovered.featureIndex = -1;
				if (s_featureHovered.sector || s_featureHovered.prevSector)
				{
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
						s_featureHovered.sector = hoveredSector;
						s_featureHovered.prevSector = hoveredSector;
						s_featureHovered.featureIndex = closestVtx;
					}
				}

				// Check for the nearest wall as well.
				checkForWallHit2d(worldPos, s_featureCurWall.sector, s_featureCurWall.featureIndex, s_featureCurWall.part, hoveredSector);

				// Handle mouse control.
				handleMouseControlVertex(worldPos);
			}

			if (s_editMode == LEDIT_WALL)
			{
				// See if we are close enough to "hover" a wall
				s_featureHovered.featureIndex = -1;

				EditorSector* hoverSector = s_featureHovered.sector ? s_featureHovered.sector : s_featureHovered.prevSector;
				checkForWallHit2d(worldPos, s_featureHovered.sector, s_featureHovered.featureIndex, s_featureHovered.part, hoverSector);
				if (s_featureHovered.sector) { s_featureHovered.prevSector = s_featureHovered.sector; }

				handleMouseControlWall(worldPos);
			}

			// DEBUG
			if (s_featureCur.sector && TFE_Input::keyPressed(KEY_T))
			{
				TFE_Polygon::computeTriangulation(&s_featureCur.sector->poly);
			}
		}
		else if (s_view == EDIT_VIEW_3D)
		{
			cameraControl3d(mx, my);
			s_featureHovered = {};
			s32 layer = (s_editFlags & LEF_SHOW_ALL_LAYERS) ? LAYER_ANY : s_curLayer;

			// TODO: Hotkeys.
			if (TFE_Input::keyPressed(KEY_G))
			{
				s_gravity = !s_gravity;
				if (s_gravity) { infoPanelAddMsg(LE_MSG_INFO, "Gravity Enabled."); }
				else { infoPanelAddMsg(LE_MSG_INFO, "Gravity Disabled."); }
			}
			if (s_gravity)
			{
				Ray ray = { s_camera.pos, { 0.0f, -1.0f, 0.0f}, 32.0f, layer };
				RayHitInfo hitInfo;
				if (traceRay(&ray, &hitInfo, false))
				{
					f32 newY = s_level.sectors[hitInfo.hitSectorId].floorHeight + 5.8f;
					f32 blendFactor = std::min(1.0f, (f32)TFE_System::getDeltaTime() * 10.0f);
					s_camera.pos.y = newY * blendFactor + s_camera.pos.y * (1.0f - blendFactor);
				}
			}

			// TODO: Move out to common place for hotkeys.
			bool hitBackfaces = TFE_Input::keyDown(KEY_B);
			s_rayDir = mouseCoordToWorldDir3d(mx, my);

			RayHitInfo hitInfo;
			Ray ray = { s_camera.pos, s_rayDir, 1000.0f, layer };
			const bool rayHit = traceRay(&ray, &hitInfo, hitBackfaces);
			if (rayHit) { s_cursor3d = hitInfo.hitPos; }
			else  		{ s_cursor3d = rayGridPlaneHit(s_camera.pos, s_rayDir); }

			if (s_editMode == LEDIT_DRAW)
			{
				handleSectorDraw(&hitInfo);
				return;
			}
			
			// Trace a ray through the mouse cursor.
			if (rayHit)
			{
				handleHoverAndSelection(&hitInfo);
			}
			else
			{
				s_featureHovered.sector = nullptr;

				if (s_editMode == LEDIT_VERTEX)
				{
					handleMouseControlVertex({ s_cursor3d.x, s_cursor3d.z });
				}
				else if (s_editMode == LEDIT_WALL)
				{
					handleMouseControlWall({ s_cursor3d.x, s_cursor3d.z });
				}
				else if (TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
				{
					s_featureCur = {};
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
					Vec2f worldPos = mouseCoordToWorldPos2d(s_viewportSize.x / 2 + (s32)s_editWinMapCorner.x, s_viewportSize.z / 2 + (s32)s_editWinMapCorner.z);
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
		s_featureCur = {};
		selection_clear();
	}
		
	void drawViewportInfo(s32 index, Vec2i mapPos, const char* info, f32 xOffset, f32 yOffset, f32 alpha)
	{
		char id[256];
		sprintf(id, "##ViewportInfo%d", index);
		ImVec2 size = ImGui::CalcTextSize(info);
		// Padding is 8, so 16 total is added.
		size.x += 16.0f;
		size.y += 16.0f;

		const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;

		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
		// TODO: Tweak padding (and size)?
		//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 4.0f, 4.0f });

		ImGui::PushStyleColor(ImGuiCol_Text, { 0.8f, 0.9f, 1.0f, 0.5f*alpha });
		ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.06f, 0.06f, 0.06f, 0.94f*0.75f*alpha });
		ImGui::PushStyleColor(ImGuiCol_Border, { 0.43f, 0.43f, 0.50f, 0.25f*alpha });

		ImGui::SetNextWindowPos({ (f32)mapPos.x + xOffset, (f32)mapPos.z + yOffset });
		if (ImGui::BeginChild(id, size, true, window_flags))
		{
			ImGui::Text("%s", info);
		}
		ImGui::EndChild();

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
	}

	void floatToString(f32 value, char* str)
	{
		const f32 valueOnGrid = s_gridSize != 0.0f ? floorf(value/s_gridSize) * s_gridSize : 1.0f;
		const f32 eps = 0.00001f;

		if (fabsf(floorf(value)-value) < eps)
		{
			// Integer value.
			sprintf(str, "%d", s32(value));
		}
		else if (s_gridSize < 1.0f && s_gridSize > 0.0f && fabsf(valueOnGrid - value) < eps)
		{
			// Fractional grid-aligned value, show as a fraction so its easier to see the value in terms of grid cells.
			s32 den = s32(1.0f / s_gridSize);
			if (value >= 1.0f)
			{
				sprintf(str, "%d %d/%d", s32(value), s32(value * (f32)den)&(den - 1), den);
			}
			else
			{
				sprintf(str, "%d/%d", s32(value * (f32)den)&(den - 1), den);
			}
		}
		else
		{
			// Floating point, removing extra zeros.
			sprintf(str, "%0.4f", value);
			s32 len = (s32)strlen(str);
			for (s32 i = 1; i <= 4; i++)
			{
				if (str[len - i] == '0') { str[len - i] = 0; }
				else { break; }
			}
		}
	}

	void getWallLengthText(const Vec2f* v0, const Vec2f* v1, char* text, Vec2i& mapPos, s32 index, Vec2f* mapOffset_)
	{
		Vec2f c = { (v0->x + v1->x) * 0.5f, (v0->z + v1->z)*0.5f };
		Vec2f n = { -(v1->z - v0->z), v1->x - v0->x };
		n = TFE_Math::normalize(&n);

		Vec2f offset = { v1->x - v0->x, v1->z - v0->z };
		f32 len = sqrtf(offset.x*offset.x + offset.z*offset.z);

		char num[256];
		floatToString(len, num);

		if (index >= 0) { sprintf(text, "%d: %s", index, num); }
		else { strcpy(text, num); }

		mapPos = worldPos2dToMap(c);
		Vec2f mapOffset = { ImGui::CalcTextSize(text).x + 16.0f, -20 };
		if (n.x < 0.0f)
		{
			mapOffset.x = s32(mapOffset.x * (n.x*0.5f - 0.5f));
		}
		else
		{
			mapOffset.x = s32(mapOffset.x * (-(1.0f - n.x)*0.5f));
		}
		mapOffset.z = s32(n.z * mapOffset.z - 16.0f);

		mapPos.x += mapOffset.x;
		mapPos.z += mapOffset.z;
		if (mapOffset_) { *mapOffset_ = mapOffset; }
	}
	
	void update()
	{
		pushFont(TFE_Editor::FONT_SMALL);
		updateWindowControls();

		// Hotkeys...
		if (TFE_Input::keyPressed(KEY_Z) && TFE_Input::keyModDown(KEYMOD_CTRL))
		{
			levHistory_undo();
		}
		else if (TFE_Input::keyPressed(KEY_Y) && TFE_Input::keyModDown(KEYMOD_CTRL))
		{
			levHistory_redo();
		}
		if (TFE_Input::keyPressed(KEY_TAB))
		{
			s_showAllLabels = !s_showAllLabels;
		}

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
					s_featureCur = {};
					s_featureHovered = {};
					selection_clear();
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

			if (!getMenuActive() && !isUiActive())
			{
				const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
					| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;

				// Display vertex info.
				if (s_featureHovered.sector && s_featureHovered.featureIndex >= 0 && editWinHovered && s_editMode == LEDIT_VERTEX)
				{
					// Give the "world space" vertex position, get back to the pixel position for the UI.
					const Vec2f vtx = s_featureHovered.sector->vtx[s_featureHovered.featureIndex];
					Vec2i mapPos;
					bool showInfo = true;
					if (s_view == EDIT_VIEW_2D)
					{
						mapPos = worldPos2dToMap(vtx);
					}
					else if (s_view == EDIT_VIEW_3D)
					{
						// Project the position.
						Vec2f screenPos;
						if (worldPosToViewportCoord({ vtx.x, s_featureHovered.sector->floorHeight, vtx.z }, &screenPos))
						{
							mapPos = { s32(screenPos.x), s32(screenPos.z) };
						}
						else
						{
							showInfo = false;
						}
					}
					
					if (showInfo)
					{
						char x[256], z[256];
						floatToString(vtx.x, x);
						floatToString(vtx.z, z);

						char info[256];
						sprintf(info, "%d: %s, %s", s_featureHovered.featureIndex, x, z);

						drawViewportInfo(0, mapPos, info, -UI_SCALE(20), -UI_SCALE(20) - 16);
					}
				}
				// Display wall info.
				else if (s_featureHovered.sector && s_featureHovered.featureIndex >= 0 && editWinHovered && s_editMode == LEDIT_WALL)
				{
					const EditorWall* wall = &s_featureHovered.sector->walls[s_featureHovered.featureIndex];
					const Vec2f* v0 = &s_featureHovered.sector->vtx[wall->idx[0]];
					const Vec2f* v1 = &s_featureHovered.sector->vtx[wall->idx[1]];

					Vec2i mapPos;
					bool showInfo = true;
					char lenStr[256];
					if (s_view == EDIT_VIEW_2D)
					{
						getWallLengthText(v0, v1, lenStr, mapPos, s_featureHovered.featureIndex);
					}
					else if (s_view == EDIT_VIEW_3D)
					{
						if (s_featureHovered.part == HP_FLOOR || s_featureHovered.part == HP_CEIL)
						{
							EditorSector* sector = s_featureHovered.sector;
							Vec3f worldPos = { (sector->bounds[0].x + sector->bounds[1].x) * 0.5f, sector->floorHeight + 1.0f, (sector->bounds[0].z + sector->bounds[1].z) * 0.5f };
							Vec2f screenPos;
							if (!s_showAllLabels && !sector->name.empty() && worldPosToViewportCoord(worldPos, &screenPos))
							{
								strcpy(lenStr, sector->name.c_str());
								f32 textOffset = floorf(ImGui::CalcTextSize(lenStr).x * 0.5f);
								mapPos = { s32(screenPos.x - textOffset), s32(screenPos.z) };
							}
							else
							{
								showInfo = false;
							}
						}
						else
						{
							Vec3f worldPos = { (v0->x + v1->x) * 0.5f, s_featureHovered.sector->floorHeight + 1.0f, (v0->z + v1->z) * 0.5f };
							Vec2f screenPos;
							if (worldPosToViewportCoord(worldPos, &screenPos))
							{
								mapPos = { s32(screenPos.x), s32(screenPos.z) - 20 };
								Vec2f offset = { v1->x - v0->x, v1->z - v0->z };
								f32 len = sqrtf(offset.x*offset.x + offset.z*offset.z);

								char num[256];
								floatToString(len, num);
								sprintf(lenStr, "%d: %s", s_featureHovered.featureIndex, num);
							}
							else
							{
								showInfo = false;
							}
						}
					}

					if (showInfo)
					{
						drawViewportInfo(0, mapPos, lenStr, 0, 0);
					}
				}
				// Sector Info.
				else if (!s_showAllLabels && s_featureHovered.sector && editWinHovered && s_editMode == LEDIT_SECTOR)
				{
					const EditorSector* sector = s_featureHovered.sector;
					if (!sector->name.empty())
					{
						bool showInfo = true;
						Vec2i mapPos;
						f32 textOffset = floorf(ImGui::CalcTextSize(sector->name.c_str()).x * 0.5f);
						if (s_view == EDIT_VIEW_2D)
						{
							Vec2f center = { (sector->bounds[0].x + sector->bounds[1].x) * 0.5f, (sector->bounds[0].z + sector->bounds[1].z) * 0.5f };
							
							mapPos = worldPos2dToMap(center);
							mapPos.x -= (textOffset + 12);
							mapPos.z -= 16;
						}
						else if (s_view == EDIT_VIEW_3D)
						{
							Vec3f center = { (sector->bounds[0].x + sector->bounds[1].x) * 0.5f, sector->floorHeight, (sector->bounds[0].z + sector->bounds[1].z) * 0.5f };
							Vec2f screenPos;
							if (worldPosToViewportCoord(center, &screenPos))
							{
								f32 textOffset = floorf(ImGui::CalcTextSize(sector->name.c_str()).x * 0.5f);
								mapPos = { s32(screenPos.x - textOffset), s32(screenPos.z) };
							}
							else
							{
								showInfo = false;
							}
						}
						if (showInfo)
						{
							drawViewportInfo(0, mapPos, sector->name.c_str(), 0, 0);
						}
					}
				}
				else if (s_editMode == LEDIT_DRAW && s_drawStarted)
				{
					if (s_drawMode == DMODE_RECT)
					{
						f32 x0 = std::max(s_shape[0].x, s_shape[1].x);
						f32 x1 = std::min(s_shape[0].x, s_shape[1].x);
						f32 z0 = std::max(s_shape[0].z, s_shape[1].z);
						f32 z1 = std::min(s_shape[0].z, s_shape[1].z);

						f32 width  = fabsf(x1 - x0);
						f32 height = fabsf(z1 - z0);
						f32 midX = (x0 + x1) * 0.5f;
						f32 midZ = (z0 + z1) * 0.5f;
						const Vec2f cen[] =
						{
							{ midX, z0 },
							{ x0, midZ },
						};
						const Vec2i mapPos[] =
						{
							worldPos2dToMap(cen[0]),
							worldPos2dToMap(cen[1]),
						};
						const f32 len[] = { width, height };
						const Vec2f offs[] = { {0.0f, -20.0f}, {25.0f, 0.0f} };

						for (s32 i = 0; i < 2; i++)
						{
							char info[256];
							floatToString(len[i], info);

							f32 textOffset = ImGui::CalcTextSize(info).x;
							if (i == 0) { textOffset = floorf(textOffset * 0.5f); }
							else if (i == 1) { textOffset = 0.0f; }

							if (s_view == EDIT_VIEW_2D)
							{
								drawViewportInfo(i, mapPos[i], info, -16.0f + offs[i].x - textOffset, -16.0f + offs[i].z);
							}
							else if (s_view == EDIT_VIEW_3D)
							{
								Vec2f screenPos;
								if (worldPosToViewportCoord({ cen[i].x, s_gridHeight, cen[i].z }, &screenPos))
								{
									drawViewportInfo(i, { (s32)screenPos.x, (s32)screenPos.z }, info, -16.0f + offs[i].x - textOffset, -16.0f + offs[i].z);
								}
							}
						}
					}
					else if (s_drawMode == DMODE_SHAPE && !s_shape.empty())
					{
						const s32 count = (s32)s_shape.size();
						const Vec2f* vtx = s_shape.data();
						// Only show a limited number of wall lengths, otherwise it just turns into noise.
						for (s32 v = 0; v < count; v++)
						{
							const Vec2f* v0 = &vtx[v];
							const Vec2f* v1 = v+1 < count ? &vtx[v + 1] : &s_drawCurPos;

							char info[256];
							Vec2i mapPos;
							Vec2f mapOffset;
							getWallLengthText(v0, v1, info, mapPos, -1, &mapOffset);

							if (s_view == EDIT_VIEW_2D)
							{
								drawViewportInfo(v, mapPos, info, 0, 0);
							}
							else if (s_view == EDIT_VIEW_3D)
							{
								Vec2f screenPos;
								if (worldPosToViewportCoord({ (v0->x + v1->x)*0.5f, s_gridHeight, (v0->z+v1->z)*0.5f }, &screenPos))
								{
									screenPos.x += mapOffset.x;
									screenPos.z += mapOffset.z;
									drawViewportInfo(v, { (s32)screenPos.x, (s32)screenPos.z }, info, 0, 0);
								}
							}
						}
					}
					else if ((s_drawMode == DMODE_RECT_VERT || s_drawMode == DMODE_SHAPE_VERT) && s_view == EDIT_VIEW_3D)
					{
						f32 height = (s_drawHeight[0] + s_drawHeight[1]) * 0.5f;
						Vec3f pos = { s_curVtxPos.x, height, s_curVtxPos.z };

						Vec2f screenPos;
						if (worldPosToViewportCoord(pos, &screenPos))
						{
							char info[256];
							floatToString(s_drawHeight[1] - s_drawHeight[0], info);
							drawViewportInfo(0, { s32(screenPos.x + 20.0f), s32(screenPos.z - 20.0f) }, info, 0, 0);
						}
					}
				}

				if (s_showAllLabels)
				{
					const s32 count = (s32)s_level.sectors.size();
					EditorSector* sector = s_level.sectors.data();
					s32 layer = ((s_editFlags & LEF_SHOW_ALL_LAYERS) && s_view != EDIT_VIEW_2D) ? LAYER_ANY : s_curLayer;
					for (s32 i = 0; i < count; i++, sector++)
					{
						if (layer != LAYER_ANY && sector->layer != layer) { continue; }
						if (sector->name.empty()) { continue; }

						Vec2i mapPos;
						f32 textOffset = floorf(ImGui::CalcTextSize(sector->name.c_str()).x * 0.5f);
						bool showInfo = true;
						if (s_view == EDIT_VIEW_2D)
						{
							Vec2f center = { (sector->bounds[0].x + sector->bounds[1].x) * 0.5f, (sector->bounds[0].z + sector->bounds[1].z) * 0.5f };

							mapPos = worldPos2dToMap(center);
							mapPos.x -= (textOffset + 12);
							mapPos.z -= 16;
						}
						else if (s_view == EDIT_VIEW_3D)
						{
							Vec3f center = { (sector->bounds[0].x + sector->bounds[1].x) * 0.5f, sector->floorHeight, (sector->bounds[0].z + sector->bounds[1].z) * 0.5f };
							Vec2f screenPos;
							if (worldPosToViewportCoord(center, &screenPos))
							{
								f32 textOffset = floorf(ImGui::CalcTextSize(sector->name.c_str()).x * 0.5f);
								mapPos = { s32(screenPos.x - textOffset), s32(screenPos.z) };
							}
							else
							{
								showInfo = false;
							}
						}
						if (showInfo)
						{
							drawViewportInfo(i, mapPos, sector->name.c_str(), 0, 0);
						}
					}
				}

				// Display Grid Info
				if (s_view == EDIT_VIEW_2D && (s_editFlags & LEF_SHOW_GRID) && !isUiActive() && !isViewportElementHovered())
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
		
	void snapToGrid(f32* value)
	{
		if (!TFE_Input::keyModDown(KEYMOD_ALT) && s_gridSize != 0.0f)
		{
			*value = floorf((*value) / s_gridSize + 0.5f) * s_gridSize;
		}
		else  // Snap to the finest grid.
		{
			*value = floorf((*value) * 65536.0f + 0.5f) / 65536.0f;
		}
	}

	void snapToGrid(Vec2f* pos)
	{
		if (!TFE_Input::keyModDown(KEYMOD_ALT) && s_gridSize != 0.0f)
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
		if (!TFE_Input::keyModDown(KEYMOD_ALT) && s_gridSize != 0.0f)
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

	void selectOverlappingVertices(EditorSector* root, s32 featureIndex, const Vec2f* rootVtx, bool addToSelection, bool addToVtxList/*=false*/)
	{
		s_searchKey++;

		// Which walls share the vertex?
		std::vector<EditorSector*> stack;

		root->searchKey = s_searchKey;
		{
			const size_t wallCount = root->walls.size();
			EditorWall* wall = root->walls.data();
			for (size_t w = 0; w < wallCount; w++, wall++)
			{
				if ((wall->idx[0] == featureIndex || wall->idx[1] == featureIndex) && wall->adjoinId >= 0)
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
					if (addToSelection) { addToVtxList ? vtxSelection_add(id) :  selection_add(id); }
					else { addToVtxList ? vtxSelection_remove(id) : selection_remove(id); }

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

	void gatherVerticesFromWallSelection()
	{
		const s32 count = (s32)s_selectionList.size();
		const FeatureId* list = s_selectionList.data();
		vtxSelection_clear();

		for (s32 i = 0; i < count; i++)
		{
			s32 featureIndex;
			HitPart part;
			EditorSector* sector = unpackFeatureId(list[i], &featureIndex, (s32*)&part);
			EditorWall* wall = &sector->walls[featureIndex];

			// Skip flats.
			if (part == HP_FLOOR || part == HP_CEIL) { continue; }

			// Add front face.
			FeatureId v0 = createFeatureId(sector, wall->idx[0]);
			FeatureId v1 = createFeatureId(sector, wall->idx[1]);
			vtxSelection_add(v0);
			vtxSelection_add(v1);

			// Add mirror (if it exists).
			if (wall->adjoinId >= 0 && wall->mirrorId >= 0)
			{
				EditorSector* nextSector = &s_level.sectors[wall->adjoinId];
				EditorWall* mirror = &nextSector->walls[wall->mirrorId];

				FeatureId v2 = createFeatureId(nextSector, mirror->idx[0]);
				FeatureId v3 = createFeatureId(nextSector, mirror->idx[1]);
				vtxSelection_add(v2);
				vtxSelection_add(v3);
			}
		}

		const s32 vtxCount = (s32)s_vertexList.size();
		for (s32 v = 0; v < vtxCount; v++)
		{
			s32 featureIndex;
			EditorSector* sector = unpackFeatureId(s_vertexList[v], &featureIndex);

			const Vec2f* rootVtx = &sector->vtx[featureIndex];
			selectOverlappingVertices(sector, featureIndex, rootVtx, true, /*addToVtxList*/true);
		}
	}
		
	void selectFromSingleVertex(EditorSector* root, s32 featureIndex, bool clearList)
	{
		const Vec2f* rootVtx = &root->vtx[featureIndex];
		FeatureId rootId = createFeatureId(root, featureIndex, 0, false);
		if (clearList)
		{
			selection_clear();
			selection_add(rootId);
		}
		else if (!selection_add(rootId))
		{
			return;
		}
		selectOverlappingVertices(root, featureIndex, rootVtx, true);
	}

	void toggleWallGroupInclusion(EditorSector* sector, s32 featureIndex, s32 part)
	{
		FeatureId id = createFeatureId(sector, featureIndex, part);
		if (selection_doesFeatureExist(id))
		{
			selection_remove(id);
		}
		else
		{
			selection_add(id);
		}
	}

	void toggleVertexGroupInclusion(EditorSector* sector, s32 featureIndex)
	{
		FeatureId id = createFeatureId(sector, featureIndex);
		if (selection_doesFeatureExist(id))
		{
			// Remove, and all matches.
			const Vec2f* vtx = &sector->vtx[featureIndex];
			// Remove all features were vertices match...
			s32 count = (s32)s_selectionList.size();
			FeatureId* list = s_selectionList.data();
			for (s32 i = count - 1; i >= 0; i--)
			{
				s32 _featureIndex;
				EditorSector* featureSector = unpackFeatureId(list[i], &_featureIndex);

				if (vtxEqual(vtx, &featureSector->vtx[_featureIndex]))
				{
					selection_remove(list[i]);
				}
			}
		}
		else
		{
			// Add
			selectFromSingleVertex(sector, featureIndex, false);
		}
	}

	void edit_setVertexPos(FeatureId id, Vec2f pos)
	{
		s32 featureIndex;
		EditorSector* sector = unpackFeatureId(id, &featureIndex);
		sector->vtx[featureIndex] = pos;

		sectorToPolygon(sector);
	}

	void edit_moveVertices(s32 count, const FeatureId* vtxIds, Vec2f delta)
	{
		s_searchKey++;
		s_sectorChangeList.clear();

		for (s32 i = 0; i < count; i++)
		{
			s32 featureIndex;
			EditorSector* sector = unpackFeatureId(vtxIds[i], &featureIndex);

			sector->vtx[featureIndex].x += delta.x;
			sector->vtx[featureIndex].z += delta.z;

			// Only update a sector once.
			if (sector->searchKey != s_searchKey)
			{
				s_sectorChangeList.push_back(sector);
				sector->searchKey = s_searchKey;
			}
		}

		// Update sectors after changes.
		const size_t changeCount = s_sectorChangeList.size();
		EditorSector** list = s_sectorChangeList.data();
		for (size_t i = 0; i < changeCount; i++)
		{
			EditorSector* sector = list[i];
			sectorToPolygon(sector);
		}
	}
		
	void moveVertex(Vec2f worldPos2d)
	{
		snapToGrid(&worldPos2d);

		// Overall movement since mousedown, for the history.
		if (!s_moveStarted)
		{
			s_moveStarted = true;
			s_moveStartPos = s_featureCur.sector->vtx[s_featureCur.featureIndex];
		}
		s_moveTotalDelta = { worldPos2d.x - s_moveStartPos.x, worldPos2d.z - s_moveStartPos.z };
		
		// Current movement.
		const Vec2f delta = { worldPos2d.x - s_featureCur.sector->vtx[s_featureCur.featureIndex].x, worldPos2d.z - s_featureCur.sector->vtx[s_featureCur.featureIndex].z };
		edit_moveVertices((s32)s_selectionList.size(), s_selectionList.data(), delta);

		s_curVtxPos.x = s_featureCur.sector->vtx[s_featureCur.featureIndex].x;
		s_curVtxPos.z = s_featureCur.sector->vtx[s_featureCur.featureIndex].z;
	}

	Vec3f moveAlongXZPlane(f32 yHeight)
	{
		const f32 eps = 0.001f;
		Vec3f worldPos = s_prevPos;
		if (fabsf(s_rayDir.y) > eps)
		{
			const f32 s = (yHeight - s_camera.pos.y) / s_rayDir.y;
			worldPos = { s_camera.pos.x + s*s_rayDir.x, s_camera.pos.y + s*s_rayDir.y, s_camera.pos.z + s* s_rayDir.z };
		}
		return worldPos;
	}
		
	Vec3f moveAlongRail(Vec3f dir)
	{
		const Vec3f rail[] =
		{
			s_curVtxPos,
			{ s_curVtxPos.x + dir.x, s_curVtxPos.y + dir.y, s_curVtxPos.z + dir.z },
		};
		// Ray through the mouse cursor.
		const Vec3f cameraRay[] =
		{
			s_camera.pos,
			{ s_camera.pos.x + s_rayDir.x, s_camera.pos.y + s_rayDir.y, s_camera.pos.z + s_rayDir.z }
		};

		f32 railInt, cameraInt;
		Vec3f worldPos;
		if (TFE_Math::closestPointBetweenLines(&rail[0], &rail[1], &cameraRay[0], &cameraRay[1], &railInt, &cameraInt))
		{
			worldPos = { rail[0].x + railInt * (rail[1].x - rail[0].x), rail[0].y + railInt * (rail[1].y - rail[0].y), rail[0].z + railInt * (rail[1].z - rail[0].z) };
			s_prevPos = worldPos;
		}
		else  // If the closest point on lines is unresolvable (parallel lines, etc.) - then just don't move.
		{
			worldPos = s_prevPos;
		}
		return worldPos;
	}

	void edit_moveFlats(s32 count, const FeatureId* flatIds, f32 delta)
	{
		s_searchKey++;
		s_sectorChangeList.clear();

		for (s32 i = 0; i < count; i++)
		{
			s32 featureIndex;
			HitPart part;
			EditorSector* sector = unpackFeatureId(flatIds[i], &featureIndex, (s32*)&part);

			if (part == HP_FLOOR)
			{
				sector->floorHeight += delta;
			}
			else if (part == HP_CEIL)
			{
				sector->ceilHeight += delta;
			}

			if (sector->searchKey != s_searchKey)
			{
				sector->searchKey = s_searchKey;
				s_sectorChangeList.push_back(sector);
			}
		}

		// Apply collision to avoid floors and ceilings crossing by looping through a second time.
		// That way if moving both floor and ceiling at the same time, they don't interfere with each other.
		// However a zero-height sector is valid.
		for (s32 i = 0; i < count; i++)
		{
			s32 featureIndex;
			HitPart part;
			EditorSector* sector = unpackFeatureId(flatIds[i], &featureIndex, (s32*)&part);

			if (part == HP_FLOOR)
			{
				sector->floorHeight = std::min(sector->floorHeight, sector->ceilHeight);
			}
			else if (part == HP_CEIL)
			{
				sector->ceilHeight = std::max(sector->floorHeight, sector->ceilHeight);
			}
		}

		const size_t changeCount = s_sectorChangeList.size();
		EditorSector** sectorList = s_sectorChangeList.data();
		for (size_t i = 0; i < changeCount; i++)
		{
			EditorSector* sector = sectorList[i];
			sector->bounds[0].y = std::min(sector->floorHeight, sector->ceilHeight);
			sector->bounds[1].y = std::max(sector->floorHeight, sector->ceilHeight);
		}
	}
		
	void moveFlat()
	{
		if (!s_moveStarted)
		{
			s_moveStarted = true;
			s_moveStartPos.x = s_featureCur.part == HP_FLOOR ? s_featureCur.sector->floorHeight : s_featureCur.sector->ceilHeight;
			s_moveStartPos.z = 0.0f;
			s_moveTotalDelta.x = 0.0f;
			s_prevPos = s_curVtxPos;
		}

		Vec3f worldPos = moveAlongRail({ 0.0f, 1.0f, 0.0f });
		f32 y = worldPos.y;

		snapToGrid(&y);
		f32 heightDelta;
		if (s_featureCur.part == HP_FLOOR)
		{
			heightDelta = y - s_featureCur.sector->floorHeight;
		}
		else
		{
			heightDelta = y - s_featureCur.sector->ceilHeight;
		}

		s_moveTotalDelta.x = y - s_moveStartPos.x;
		edit_moveFlats((s32)s_selectionList.size(), s_selectionList.data(), heightDelta);
	}

	f32 snapAlongPath(const Vec2f& startPos, const Vec2f& path, f32 pathOffset)
	{
		f32 dX = FLT_MAX, dZ = FLT_MAX;
		f32 offset = FLT_MAX;
		if (fabsf(path.x) > FLT_EPSILON)
		{
			f32 nextPos = s_moveStartPos.x + path.x * pathOffset;
			snapToGrid(&nextPos);
			dX = (nextPos - s_moveStartPos.x) / path.x;
		}
		if (fabsf(path.z) > FLT_EPSILON)
		{
			f32 nextPos = s_moveStartPos.z + path.z * pathOffset;
			snapToGrid(&nextPos);
			dZ = (nextPos - s_moveStartPos.z) / path.z;
		}
		if (dX < FLT_MAX && fabsf(dX - pathOffset) < fabsf(dZ - pathOffset))
		{
			offset = dX;
		}
		else if (dZ < FLT_MAX)
		{
			offset = dZ;
		}
		return offset;
	}
		
	void moveWall(Vec3f worldPos)
	{
		// Overall movement since mousedown, for the history.
		const EditorWall* wall = &s_featureCur.sector->walls[s_featureCur.featureIndex];
		const Vec2f& v0 = s_featureCur.sector->vtx[wall->idx[0]];
		const Vec2f& v1 = s_featureCur.sector->vtx[wall->idx[1]];
		if (!s_moveStarted)
		{
			s_moveStarted = true;
			s_moveStartPos  = v0;
			s_moveStartPos1 = v1;

			s_wallNrm = { -(v1.z - v0.z), v1.x - v0.x };
			s_wallNrm = TFE_Math::normalize(&s_wallNrm);

			s_wallTan = { v1.x - v0.x, v1.z - v0.z };
			s_wallTan = TFE_Math::normalize(&s_wallTan);

			if (s_view == EDIT_VIEW_3D)
			{
				s_prevPos = s_curVtxPos;
			}
		}
		
		Vec2f moveDir = s_wallNrm;
		Vec2f startPos = s_moveStartPos;
		if (s_view == EDIT_VIEW_3D)
		{
			if (s_wallMoveMode == WMM_TANGENT)
			{
				worldPos = moveAlongRail({ s_wallTan.x, 0.0f, s_wallTan.z });
				moveDir = s_wallTan;
				startPos = { s_curVtxPos.x, s_curVtxPos.z };
			}
			else if (s_wallMoveMode == WMM_NORMAL)
			{
				worldPos = moveAlongRail({ s_wallNrm.x, 0.0f, s_wallNrm.z });
			}
			else
			{
				worldPos = moveAlongXZPlane(s_curVtxPos.y);
			}
		}
		else
		{
			if (s_wallMoveMode == WMM_TANGENT)
			{
				moveDir = s_wallTan;
				startPos = { s_curVtxPos.x, s_curVtxPos.z };
			}
		}

		if (s_wallMoveMode == WMM_FREE)
		{
			Vec2f delta = { worldPos.x - s_curVtxPos.x, worldPos.z - s_curVtxPos.z };

			if (!TFE_Input::keyModDown(KEYMOD_ALT))
			{
				// Smallest snap distance.
				Vec2f p0 = { s_moveStartPos.x + delta.x, s_moveStartPos.z + delta.z };
				Vec2f p1 = { s_moveStartPos1.x + delta.x, s_moveStartPos1.z + delta.z };

				Vec2f s0 = p0, s1 = p1;
				snapToGrid(&s0);
				snapToGrid(&s1);

				Vec2f d0 = { p0.x - s0.x, p0.z - s0.z };
				Vec2f d1 = { p1.x - s1.x, p1.z - s1.z };
				if (TFE_Math::dot(&d0, &d0) <= TFE_Math::dot(&d1, &d1))
				{
					delta = { s0.x - v0.x, s0.z - v0.z };
				}
				else
				{
					delta = { s1.x - v1.x, s1.z - v1.z };
				}
			}
			else
			{
				delta = { s_moveStartPos.x + delta.x - v0.x, s_moveStartPos.z + delta.z - v0.z };
			}

			// Move all of the vertices by the offset.
			s_moveTotalDelta = { v0.x + delta.x - s_moveStartPos.x, v0.z + delta.z - s_moveStartPos.z };
			edit_moveVertices((u32)s_vertexList.size(), s_vertexList.data(), delta);
		}
		else
		{
			// Compute the movement.
			Vec2f offset = { worldPos.x - startPos.x, worldPos.z - startPos.z };
			f32 nrmOffset = TFE_Math::dot(&offset, &moveDir);

			if (!TFE_Input::keyModDown(KEYMOD_ALT))
			{
				// Find the snap point along the path from vertex 0 and vertex 1.
				f32 v0Offset = snapAlongPath(s_moveStartPos, moveDir, nrmOffset);
				f32 v1Offset = snapAlongPath(s_moveStartPos1, moveDir, nrmOffset);

				// Finally determine which snap to take (whichever is the smallest).
				if (v0Offset < FLT_MAX && fabsf(v0Offset - nrmOffset) < fabsf(v1Offset - nrmOffset))
				{
					nrmOffset = v0Offset;
				}
				else if (v1Offset < FLT_MAX)
				{
					nrmOffset = v1Offset;
				}
			}

			// Move all of the vertices by the offset.
			Vec2f delta = { s_moveStartPos.x + moveDir.x * nrmOffset - v0.x, s_moveStartPos.z + moveDir.z * nrmOffset - v0.z };
			s_moveTotalDelta = { v0.x + delta.x - s_moveStartPos.x, v0.z + delta.z - s_moveStartPos.z };
			edit_moveVertices((u32)s_vertexList.size(), s_vertexList.data(), delta);
		}
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
			const Vec2f worldPos2d = mouseCoordToWorldPos2d(mx, my);
			moveFeatures({worldPos2d.x, 0.0f, worldPos2d.z});
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
			Vec3f worldPos;
			if (s_editMode == LEDIT_VERTEX)
			{
				const f32 u = (s_curVtxPos.y - s_camera.pos.y) / (s_cursor3d.y - s_camera.pos.y);
				worldPos =
				{
					s_camera.pos.x + u * (s_cursor3d.x - s_camera.pos.x),
					0.0f,
					s_camera.pos.z + u * (s_cursor3d.z - s_camera.pos.z)
				};
			}
			else if (s_editMode == LEDIT_WALL)
			{
				// When wall editing is enabled, then what we really want is a plane that passes through the cursor and is oriented along
				// the direction of movement (aka, the normal is perpendicular to this movement).
				worldPos = s_cursor3d;
			}
			moveFeatures(worldPos);
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
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse;

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

	Vec3f transform(const Mat3& mtx, const Vec3f& vec)
	{
		return { TFE_Math::dot(&mtx.m0, &vec), TFE_Math::dot(&mtx.m1, &vec), TFE_Math::dot(&mtx.m2, &vec) };
	}

	bool worldPosToViewportCoord(Vec3f worldPos, Vec2f* screenPos)
	{
		Vec3f cameraRel = { worldPos.x - s_camera.pos.x, worldPos.y - s_camera.pos.y, worldPos.z - s_camera.pos.z };
		Vec3f viewPosXYZ = transform(s_camera.viewMtx, cameraRel);

		Vec4f viewPos = { viewPosXYZ.x, viewPosXYZ.y, viewPosXYZ.z, 1.0f };
		Vec4f proj = transform(s_camera.projMtx, viewPos);

		if (proj.w < FLT_EPSILON)
		{
			return false;
		}

		f32 wScale = 1.0f / proj.w;
		screenPos->x = (proj.x * wScale * 0.5f + 0.5f) * (f32)s_viewportSize.x + (f32)s_editWinMapCorner.x;
		screenPos->z = (proj.y * wScale * 0.5f + 0.5f) * (f32)s_viewportSize.z + (f32)s_editWinMapCorner.z;
		return true;
	}

	Vec3f viewportCoordToWorldDir3d(Vec2i vCoord)
	{
		const Vec4f posScreen =
		{
			f32(vCoord.x) / f32(s_viewportSize.x) * 2.0f - 1.0f,
			f32(vCoord.z) / f32(s_viewportSize.z) * 2.0f - 1.0f,
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

	// Convert from viewport mouse coordinates to world space 3D direction.
	Vec3f mouseCoordToWorldDir3d(s32 mx, s32 my)
	{
		return viewportCoordToWorldDir3d({ mx - (s32)s_editWinMapCorner.x, my - (s32)s_editWinMapCorner.z });
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

	void moveFeatures(const Vec3f& newPos)
	{
		if (s_editMode == LEDIT_VERTEX)
		{
			if (s_featureCur.featureIndex >= 0 && s_featureCur.sector && TFE_Input::mouseDown(MBUTTON_LEFT) && !TFE_Input::keyModDown(KEYMOD_CTRL) && !TFE_Input::keyModDown(KEYMOD_SHIFT))
			{
				moveVertex({ newPos.x, newPos.z });
			}
			else if (s_featureCur.featureIndex >= 0 && s_featureCur.sector && s_moveStarted)
			{
				s_editMove = false;
				s_moveStarted = false;
				cmd_addMoveVertices((s32)s_selectionList.size(), s_selectionList.data(), s_moveTotalDelta);
			}
			else
			{
				s_editMove = false;
				s_moveStarted = false;
			}
		}
		else if (s_editMode == LEDIT_WALL)
		{
			if (s_featureCur.featureIndex >= 0 && s_featureCur.sector && TFE_Input::mouseDown(MBUTTON_LEFT) && !TFE_Input::keyModDown(KEYMOD_CTRL) && !TFE_Input::keyModDown(KEYMOD_SHIFT))
			{
				if (s_featureCur.part == HP_FLOOR || s_featureCur.part == HP_CEIL)
				{
					moveFlat();
				}
				else
				{
					moveWall(newPos);
				}
			}
			else if (s_featureCur.featureIndex >= 0 && s_featureCur.sector && s_moveStarted)
			{
				s_editMove = false;
				s_moveStarted = false;

				if (s_featureCur.part == HP_FLOOR || s_featureCur.part == HP_CEIL)
				{
					cmd_addMoveFlats((s32)s_selectionList.size(), s_selectionList.data(), s_moveTotalDelta.x);
				}
				else // Re-use the move vertices command, but with the name LName_MoveWall.
				{
					cmd_addMoveVertices((s32)s_vertexList.size(), s_vertexList.data(), s_moveTotalDelta, LName_MoveWall);
				}
			}
			else
			{
				s_editMove = false;
				s_moveStarted = false;
			}
		}
	}

	void edit_clearSelections()
	{
		s_selectionList.clear();
		s_featureCur = {};
		s_featureCurWall = {};
		s_featureHovered = {};
	}
}
