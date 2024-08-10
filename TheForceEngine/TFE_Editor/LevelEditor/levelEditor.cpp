#include "levelEditor.h"
#include "levelEditorData.h"
#include "levelEditorHistory.h"
#include "levelEditorInf.h"
#include "contextMenu.h"
#include "hotkeys.h"
#include "lighting.h"
#include "entity.h"
#include "groups.h"
#include "selection.h"
#include "infoPanel.h"
#include "browser.h"
#include "camera.h"
#include "error.h"
#include "sharedState.h"
#include "editEntity.h"
#include "editGeometry.h"
#include "editGuidelines.h"
#include "editNotes.h"
#include "userPreferences.h"
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
#include <TFE_Editor/LevelEditor/Rendering/grid.h>
#include <TFE_Editor/LevelEditor/Rendering/grid2d.h>
#include <TFE_Editor/LevelEditor/Scripting/levelEditorScripts.h>
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
#include <SDL.h>

#include <climits>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;
#define SHOW_EDITOR_FPS 0
#define EDITOR_TEXT_PROMPTS 0

namespace LevelEditor
{
	const f32 c_defaultZoom = 0.25f;
	const f32 c_defaultYaw = PI;
	const f32 c_defaultCameraHeight = 6.0f;
	const f64 c_doubleClickThreshold = 0.25f;
	const s32 c_vanillaDarkForcesNameLimit = 16;
	
	struct VertexWallGroup
	{
		EditorSector* sector;
		s32 vertexIndex;
		s32 leftWall, rightWall;
	};
	
	enum WallMoveMode
	{
		WMM_NORMAL = 0,
		WMM_FREE,
		WMM_COUNT
	};

	static Asset* s_levelAsset = nullptr;
	static WallMoveMode s_wallMoveMode = WMM_NORMAL;
	static s32 s_outputHeight = 26*6;

	// The TFE Level Editor format is different than the base format and contains extra 
	// metadata, etc.
	// So existing levels need to be loaded into that format.
	// If the correct format already exists, though, then it is loaded directly.
	AssetList s_levelTextureList;
	LevelEditMode s_editMode = LEDIT_DRAW;

	u32 s_editFlags = LEF_DEFAULT;
	u32 s_lwinOpen = LWIN_NONE;
	s32 s_curLayer = 0;
	u32 s_gridFlags = GFLAG_NONE;

	// Unified feature hover/cur
	Feature s_featureHovered = {};
	Feature s_featureCur = {};
	Feature s_featureCurWall = {};	// used to access wall data in other modes.
	Feature s_featureTex = {};

	// Vertex
	Vec3f s_hoveredVtxPos;
	Vec3f s_curVtxPos;

	// Texture
	s32 s_selectedTexture = -1;
	Vec2f s_copiedTextureOffset = { 0 };

	// Entity
	s32 s_selectedEntity = -1;
		
	// Search
	u32 s_searchKey = 0;
	static std::vector<VertexWallGroup> s_vertexWallGroups;
	static std::vector<EditorSector*> s_sortedHoverSectors;

	bool s_editMove = false;
	static SectorList s_workList;
	
	EditorView s_view = EDIT_VIEW_2D;
	static Vec2i s_editWinPos = { 0, 69 };
	static Vec2i s_editWinSize = { 0 };
	static Vec2f s_editWinMapCorner = { 0 };
	static Vec3f s_rayDir = { 0.0f, 0.0f, 1.0f };
	static f32 s_zoom = c_defaultZoom;
	static bool s_gravity = false;
	static bool s_showAllLabels = false;
	static bool s_modalUiActive = false;

	// Handle initial mouse movement before feature movement or snapping.
	static const s32 c_moveThreshold = 3;
	static bool s_canMoveFeature = false;
	static Vec2i s_moveInitPos = { 0 };
	
	// General movement.
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
	static s32 s_newWallTexOverride = -1;

	// Texture Alignment
	static bool s_startTexMove = false;
	static Vec3f s_startTexPos;
	static Vec2f s_startTexOffset;
	static bool s_texMoveSign = false;
	static Vec2f s_texNormal, s_texTangent;
	static SelectMode s_selectMode = SELECTMODE_NONE;

	static s32 s_gridIndex = 7;
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
	static TextureGpu* s_boolToolbarData = nullptr;

	////////////////////////////////////////////////////////
	// Forward Declarations
	////////////////////////////////////////////////////////
	void toolbarBegin();
	void toolbarEnd();
	bool drawToolbarButton(void* ptr, u32 imageId, bool highlight, const char* tooltip);
	bool drawToolbarBooleanButton(void* ptr, u32 imageId, bool highlight, const char* tooltip);
	void levelEditWinBegin();
	void levelEditWinEnd();
	void messagePanel(ImVec2 pos);
	void cameraControl2d(s32 mx, s32 my);
	void cameraControl3d(s32 mx, s32 my);
	void resetZoom();
	bool isViewportElementHovered();
	void play();
	Vec2f mouseCoordToWorldPos2d(s32 mx, s32 my);
	Vec3f mouseCoordToWorldDir3d(s32 mx, s32 my);
	Vec3f viewportCoordToWorldDir3d(Vec2i vCoord);
	Vec2i worldPos2dToMap(const Vec2f& worldPos);
	bool worldPosToViewportCoord(Vec3f worldPos, Vec2f* screenPos);
	
	void selectFromSingleVertex(EditorSector* root, s32 featureIndex, bool clearList);
	void selectOverlappingVertices(EditorSector* root, s32 featureIndex, const Vec2f* rootVtx, bool addToSelection, bool addToVtxList=false);
	void toggleVertexGroupInclusion(EditorSector* sector, s32 featureId);
	void toggleWallGroupInclusion(EditorSector* sector, s32 featureIndex, s32 part);
	void gatherVerticesFromWallSelection();
	void moveFeatures(const Vec3f& newPos);
	void handleVertexInsert(Vec2f worldPos);
	void handleTextureAlignment();

	void snapSignToCursor(EditorSector* sector, EditorWall* wall, s32 signTexIndex, Vec2f* signOffset);

	void drawViewportInfo(s32 index, Vec2i mapPos, const char* info, f32 xOffset, f32 yOffset, f32 alpha=1.0f, u32 overrideColor=0u);
	void getWallLengthText(const Vec2f* v0, const Vec2f* v1, char* text, Vec2i& mapPos, s32 index = -1, Vec2f* mapOffset = nullptr);

	void copyToClipboard(const char* str);
	bool copyFromClipboard(char* str);
	void applySurfaceTextures();
	void selectSimilarWalls(EditorSector* rootSector, s32 wallIndex, HitPart part, bool autoAlign=false);

	void handleSelectMode(Vec3f pos);
	void handleSelectMode(EditorSector* sector, s32 wallIndex);
		
	bool isInIntList(s32 value, std::vector<s32>* list);

#if EDITOR_TEXT_PROMPTS == 1
	void handleTextPrompts();
#endif

	void edit_insertShape(const f32* heights, BoolMode mode, s32 firstVertex, bool allowSubsectorExtrude);
	void cleanSectors(const std::vector<s32>& selectedSectors);

	void updateViewportScroll();
	f32 smoothstep(f32 edge0, f32 edge1, f32 x);
	
	////////////////////////////////////////////////////////
	// Public API
	////////////////////////////////////////////////////////
	void loadPaletteAndColormap()
	{
		AssetHandle palHandle = loadPalette(s_level.palette.c_str());
		char colormapName[256];
		sprintf(colormapName, "%s.CMP", s_level.slot.c_str());
		AssetHandle colormapHandle = loadColormap(colormapName);
		LE_INFO("Palette: '%s', Colormap: '%s'", s_level.palette.c_str(), colormapName);
	}

	bool init(Asset* asset)
	{
		registerScriptFunctions();

		s_levelAsset = asset;
		// Initialize editors.
		editGeometry_init();
		editGuidelines_init();

		// Reset output messages.
		infoPanelClearMessages();
		infoPanelSetMsgFilter();

		// Cleanup any existing level data.
		destroy();

		// Load definitions.
		// TODO: Handle different games...
		const char* gameLocalDir = "DarkForces";
		loadVariableData(gameLocalDir);
		loadEntityData(gameLocalDir, false);
		loadLogicData(gameLocalDir);
		groups_init();

		Project* project = project_get();
		if (project && project->active)
		{
			char projEntityDataPath[TFE_MAX_PATH];
			sprintf(projEntityDataPath, "%s/%s.ini", project->path, "CustomEntityDef");
			loadEntityData(projEntityDataPath, true);
		}

		browserLoadIcons();
		clearEntityChanges();

		// Load the new level.
		if (!loadLevelFromAsset(asset))
		{
			return false;
		}
		LE_INFO("Loaded level '%s'", s_level.name.c_str());

		loadPaletteAndColormap();
						
		viewport_init();
		
		viewport_update((s32)UI_SCALE(480) + 16, (s32)UI_SCALE(68) + 18);
		s_gridIndex = 7;
		s_grid.size = c_gridSizeMap[s_gridIndex];

		s_boolToolbarData = loadGpuImage("UI_Images/Boolean_32x3.png");
		if (s_boolToolbarData)
		{
			LE_INFO("Loaded toolbar images 'UI_Images/Boolean_32x3.png'");
		}
		else
		{
			LE_ERROR("Failed to load toolbar images 'UI_Images/Boolean_32x3.png'");
		}

		u32 idx = 0;
		for (s32 i = -15; i < 16; i++, idx += 4)
		{
			s_layerStr[i + 15] = &s_layerMem[idx];
			sprintf(s_layerStr[i + 15], "%d", i);
		}

		s_viewportPos = { -24.0f, 0.0f, -200.0f };
		if (s_level.layerRange[1] < s_level.layerRange[0])
		{
			// This is a new level, so just default to layer 0.
			s_level.layerRange[0] = 0;
			s_level.layerRange[1] = 0;
			s_level.bounds[0] = { 0 };
			s_level.bounds[1] = { 0 };
		}
		s_curLayer = std::min(1, s_level.layerRange[1]);

		s_sectorDrawMode = SDM_WIREFRAME;
		s_gridFlags = GFLAG_NONE;
		s_featureHovered = {};
		s_featureCur = {};
		s_featureTex = {};
		s_editMove = false;
		s_gravity = false;
		s_showAllLabels = false;
		s_doubleClick = false;
		s_singleClick = false;
		s_rightClick = false;
		s_selectedTexture = -1;
		s_selectedEntity = -1;
		s_copiedTextureOffset = { 0 };

		AssetBrowser::getLevelTextures(s_levelTextureList, asset->name.c_str());
		s_camera = { 0 };
		s_camera.pos.y = c_defaultCameraHeight;
		s_camera.yaw = c_defaultYaw;
		computeCameraTransform();
		s_cursor3d = { 0 };
		resetGrid();

		levHistory_init();
		levHistory_createSnapshot("Imported Level");

		infoPanelClearFeatures();
		TFE_RenderShared::init(false);
		return true;
	}

	void destroy()
	{
		s_level.sectors.clear();
		viewport_destroy();
		TFE_RenderShared::destroy();

		TFE_RenderBackend::freeTexture(s_boolToolbarData);
		s_boolToolbarData = nullptr;

		levHistory_destroy();
		browserFreeIcons();
	}
			
	bool sortHoveredSectors(EditorSector* a, EditorSector* b)
	{
		if (a->floorHeight > b->floorHeight) { return true; }
		else if (a->floorHeight == b->floorHeight)
		{
			// 2D area.
			f32 areaA = (a->bounds[1].x - a->bounds[0].x) * (a->bounds[1].z - a->bounds[0].z);
			f32 areaB = (b->bounds[1].x - b->bounds[0].x) * (b->bounds[1].z - b->bounds[0].z);
			return areaA < areaB;
		}
		return false;
	}

	EditorSector* findSector2d(Vec2f pos, s32 layer)
	{
		const size_t sectorCount = s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		s_sortedHoverSectors.clear();
		for (size_t s = 0; s < sectorCount; s++, sector++)
		{
			// Make sure the sector is in a visible/unlocked group.
			if (!sector_isInteractable(sector)) { continue; }
			if (isPointInsideSector2d(sector, pos, layer))
			{
				// Gather all of the potentially selected sectors in a list.
				s_sortedHoverSectors.push_back(sector);
			}
		}
		if (s_sortedHoverSectors.empty()) { return nullptr; }
		
		// Then sort:
		// 1. floor height.
		// 2. approximate area (if the floor height is the same).
		std::sort(s_sortedHoverSectors.begin(), s_sortedHoverSectors.end(), sortHoveredSectors);

		// Finally select the beginning.
		// Note we may walk forward if stepping through the selection.
		return s_sortedHoverSectors[0];
	}
	
	bool isUiModal()
	{
		return getMenuActive() || s_modalUiActive || isPopupOpen();
	}

	bool isViewportElementHovered()
	{
		return s_featureHovered.featureIndex >= 0 || s_featureCur.featureIndex >= 0;
	}
	
	void adjustGridHeight(EditorSector* sector)
	{
		if (!sector) { return; }
		s_grid.height = sector->floorHeight;
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
				if (!sector_isInteractable(sector)) { continue; }
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
			if (traceRay(&ray, &hitInfo, false, false) && hitInfo.hitSectorId >= 0)
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
				if (!sector_isInteractable(sector)) { continue; }

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
				if (!sector_isInteractable(sector)) { continue; }
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
			if (traceRay(&ray, &hitInfo, false, false) && hitInfo.hitSectorId >= 0)
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
				if (!sector_isInteractable(sector)) { continue; }

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
			
	void handleMouseControlVertex(Vec2f worldPos)
	{
		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);

		// Short names to make the logic easier to follow.
		const bool selAdd    = TFE_Input::keyModDown(KEYMOD_SHIFT);
		const bool selRem    = TFE_Input::keyModDown(KEYMOD_ALT);
		const bool selToggle = TFE_Input::keyModDown(KEYMOD_CTRL);
		const bool selToggleDrag = selAdd && selToggle;

		const bool mousePressed = s_singleClick;
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
					const FeatureId id = createFeatureId(s_featureHovered.sector, s_featureHovered.featureIndex);
					const bool doesItemExist = selection_doesFeatureExist(id);

					handleSelectMode(s_featureHovered.sector, -1);

					s_featureCur.sector = s_featureHovered.sector;
					s_featureCur.featureIndex = s_featureHovered.featureIndex;
					s_curVtxPos = s_hoveredVtxPos;
					adjustGridHeight(s_featureCur.sector);
					s_editMove = true;

					// Clear the selection if this is a new vertex and Ctrl isn't held.
					selectFromSingleVertex(s_featureCur.sector, s_featureCur.featureIndex, !doesItemExist);
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

		// Compute the u offset.
		Vec2f v0 = sector->vtx[srcWall->idx[0]];
		Vec2f v1 = sector->vtx[srcWall->idx[1]];
		Vec2f dir = { v1.x - v0.x, v1.z - v0.z };
		f32 len = sqrtf(dir.x*dir.x + dir.z*dir.z);
		f32 uSplit = 0.0f;
		if (fabsf(dir.x) >= fabsf(dir.z))
		{
			uSplit = (newPos.x - v0.x) / dir.x;
		}
		else
		{
			uSplit = (newPos.z - v0.z) / dir.z;
		}
		uSplit *= len;

		// Copy wall attributes (textures, flags, etc.).
		EditorWall newWall = *srcWall;
		// Then setup indices.
		srcWall->idx[0] = idx[0];
		srcWall->idx[1] = idx[1];
		newWall.idx[0] = idx[1];
		newWall.idx[1] = idx[2];
		for (s32 i = 0; i < WP_COUNT; i++)
		{
			if (newWall.tex[i].texIndex >= 0)
			{
				newWall.tex[i].offset.x += uSplit;
			}
		}

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
		const s32 levelSectorCount = (s32)s_level.sectors.size();
		EditorWall* wall = sector->walls.data();
		for (size_t w = 0; w < wallCount; w++, wall++)
		{
			if (wall->adjoinId < 0 || wall->adjoinId >= levelSectorCount) { continue; }
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

	bool canSplitWall(EditorSector* sector, s32 wallIndex, Vec2f newPos)
	{
		// If a vertex at the desired position already exists, do not split the wall (early-out).
		const size_t vtxCount = sector->vtx.size();
		const Vec2f* vtx = sector->vtx.data();
		for (size_t v = 0; v < vtxCount; v++)
		{
			if (TFE_Polygon::vtxEqual(&newPos, &vtx[v]))
			{
				return false;
			}
		}
		return true;
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
		if (!canSplitWall(sector, wallIndex, newPos)) { return false; }

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
				if (TFE_Polygon::vtxEqual(rootVtx, &sector->vtx[wall->idx[0]]))
				{
					i = 0;
					rightIdx = w;
					vtxId = wall->idx[0];
				}
				else if (TFE_Polygon::vtxEqual(rootVtx, &sector->vtx[wall->idx[1]]))
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

			// *TODO*
			// Is the sector degenerate (aka, has no area)?
			if (sector->vtx.size() >= 3) { continue; }
						
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

						if (TFE_Polygon::vtxEqual(v0, v3) && TFE_Polygon::vtxEqual(v1, v2))
						{
							// Found an adjoin!
							wall0->adjoinId = sector1->id;
							wall0->mirrorId = w1;
							wall1->adjoinId = sector0->id;
							wall1->mirrorId = w0;
							// For now, assume one adjoin per wall.
							foundMirror = true;
						}
						else if (TFE_Polygon::vtxEqual(v0, v2) && TFE_Polygon::vtxEqual(v1, v3))
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

			if ((TFE_Polygon::vtxEqual(v0, v3) && TFE_Polygon::vtxEqual(v1, v2)) || (TFE_Polygon::vtxEqual(v0, v2) && TFE_Polygon::vtxEqual(v1, v3)))
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
			if (sector->searchKey != s_searchKey)
			{
				sector->searchKey = s_searchKey;
				s_sectorChangeList.push_back(sector);
			}

			EditorWall* left  = group->leftWall  >= 0 ? &sector->walls[group->leftWall]  : nullptr;
			EditorWall* right = group->rightWall >= 0 ? &sector->walls[group->rightWall] : nullptr;
			if (left && right)
			{
				// Merge the left and right walls.
				left->idx[1] = right->idx[1];
				// Make the right wall degenerate - this will be removed during the "clean" pass.
				right->idx[0] = right->idx[1];
			}
			else if (left)
			{
				// Make the wall degenerate - this will be removed during the "clean" pass.
				left->idx[1] = left->idx[0];
			}
			else if (right)
			{
				// Make the wall degenerate - this will be removed during the "clean" pass.
				right->idx[1] = right->idx[0];
			}
		}

		// Clean sectors - this will remove collapsed walls and clean up the sectors.
		std::vector<s32> selectedSectors;
		const s32 count = (s32)s_sectorChangeList.size();
		EditorSector** sectorList = s_sectorChangeList.data();
		for (s32 i = 0; i < count; i++)
		{
			selectedSectors.push_back(sectorList[i]->id);
		}
		cleanSectors(selectedSectors);

		// Remove any invalid sectors.
		deleteInvalidSectors(s_sectorChangeList);

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
		const bool mousePressed = s_singleClick;
		const bool mouseDown = TFE_Input::mouseDown(MouseButton::MBUTTON_LEFT);
		// TODO: Hotkeys.
		const bool insertVtx = TFE_Input::keyPressed(KEY_INSERT);
		const bool snapToGridEnabled = !TFE_Input::keyModDown(KEYMOD_ALT) && s_grid.size > 0.0f;

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

		f32 s = TFE_Polygon::closestPointOnLineSegment(v0, v1, worldPos, &newPos);
		if (snapToGridEnabled)
		{
			s = snapToEdgeGrid(v0, v1, newPos);
		}

		if (s > FLT_EPSILON && s < 1.0f - FLT_EPSILON)
		{
			// Split the wall.
			cmd_addInsertVertex(sector->id, wallIndex, newPos);
			edit_splitWall(sector->id, wallIndex, newPos);
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

			FeatureId id = createFeatureId(sector, 0, part);
			selection_add(id);

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
						FeatureId id = createFeatureId(sector, nextIndex, HP_MID);
						if (!selection_add(id))
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
							FeatureId id = createFeatureId(sector, nextIndex, HP_TOP);
							if (selection_add(id))
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
							FeatureId id = createFeatureId(sector, nextIndex, HP_BOT);
							if (selection_add(id))
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
							FeatureId id = createFeatureId(sector, nextIndex, HP_MID);
							if (selection_add(id))
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

	// Hacky version for now, clean up once it works... (separate selection lists).
	void edit_autoAlign(s32 sectorId, s32 wallIndex, HitPart part)
	{
		EditorSector* rootSector = &s_level.sectors[sectorId];
		// Save the selection and clear.
		SelectionList curSelection = s_selectionList;
		s_selectionList.clear();

		// Find similar walls.
		selectSimilarWalls(rootSector, wallIndex, part, true);

		// Restore the selection.
		s_selectionList = curSelection;
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

		f32 leftAlign  = 0.0f;
		f32 rightAlign = autoAlign ? getWallLength(rootSector, rootWall) : 0.0f;
		Vec2f texOffsetLeft  = getPartTextureOffset(rootWall, part);
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
				FeatureId id = createFeatureId(rootSector, wallIndex, HP_MID);
				selection_add(id);
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
					FeatureId id = createFeatureId(rootSector, wallIndex, HP_TOP);
					selection_add(id);
					if (autoAlign) { setPartTextureOffset(rootWall, HP_TOP, { texOffset[1].x, texOffset[1].z + next->ceilHeight - baseHeight }); }
				}
			}
			if (next->floorHeight > rootSector->floorHeight)
			{
				s32 curTexId = getPartTextureIndex(rootWall, HP_BOT);
				if (texId < 0 || curTexId == texId)
				{
					FeatureId id = createFeatureId(rootSector, wallIndex, HP_BOT);
					selection_add(id);
					if (autoAlign) { setPartTextureOffset(rootWall, HP_BOT, { texOffset[1].x, texOffset[1].z + rootSector->floorHeight - baseHeight }); }
				}
			}
			if (rootWall->flags[0] & WF1_ADJ_MID_TEX)
			{
				s32 curTexId = getPartTextureIndex(rootWall, HP_MID);
				if (texId < 0 || curTexId == texId)
				{
					FeatureId id = createFeatureId(rootSector, wallIndex, HP_MID);
					selection_add(id);
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

	void handleMouseControlWall(Vec2f worldPos)
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

		if (del && (!s_selectionList.empty() || s_featureCur.featureIndex >= 0 || s_featureHovered.featureIndex >= 0))
		{
			s32 sectorId, wallIndex;
			HitPart part;
			EditorSector* featureSector = nullptr;
			if (!s_selectionList.empty())
			{
				// TODO: Currently, you can only delete one vertex at a time. It should be possible to delete multiple.
				featureSector = unpackFeatureId(s_selectionList[0], &wallIndex, (s32*)&part);
				sectorId = featureSector->id;
			}
			else if (s_featureCur.featureIndex >= 0 && s_featureCur.sector)
			{
				featureSector = s_featureCur.sector;
				sectorId = s_featureCur.sector->id;
				wallIndex = s_featureCur.featureIndex;
				part = s_featureCur.part;
			}
			else if (s_featureHovered.featureIndex >= 0 && s_featureHovered.sector)
			{
				featureSector = s_featureHovered.sector;
				sectorId = s_featureHovered.sector->id;
				wallIndex = s_featureHovered.featureIndex;
				part = s_featureHovered.part;
			}

			if (part == HP_FLOOR || part == HP_CEIL)
			{
				edit_deleteSector(sectorId);
				cmd_addDeleteSector(sectorId);
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
					cmd_addClearTexture(1, &id);
				}
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
					const FeatureId id = createFeatureId(s_featureHovered.sector, s_featureHovered.featureIndex, s_featureHovered.part);
					const bool doesItemExist = selection_doesFeatureExist(id);

					handleSelectMode(s_featureHovered.sector,
						(s_featureHovered.part == HP_FLOOR || s_featureHovered.part == HP_CEIL) ? -1 : s_featureHovered.featureIndex);

					s_featureCur.sector = s_featureHovered.sector;
					s_featureCur.featureIndex = s_featureHovered.featureIndex;
					s_featureCur.part = s_featureHovered.part;
					// Set this to the 3D cursor position, so the wall doesn't "pop" when grabbed.
					s_curVtxPos = s_cursor3d;
					adjustGridHeight(s_featureCur.sector);
					s_editMove = true;

					// TODO: Hotkeys...
					// TODO: Should these just be removed to simplify things a bit?
					s_wallMoveMode = WMM_FREE;
					if (TFE_Input::keyDown(KEY_N))
					{
						s_wallMoveMode = WMM_NORMAL;
					}

					if (!doesItemExist) { selection_clear(); }
					selection_add(id);
					// TODO: Handle with derived sets.
					gatherVerticesFromWallSelection();
				}
				else if (!s_editMove)
				{
					startDragSelect(mx, my, DSEL_SET);
				}
			}
		}
		else if (s_doubleClick)
		{
			s_featureCur.sector = nullptr;
			s_featureCur.featureIndex = -1;
			if (s_featureHovered.sector && s_featureHovered.featureIndex >= 0)
			{
				if (!TFE_Input::keyModDown(KEYMOD_SHIFT))
				{
					s_selectionList.clear();
				}
				if (s_featureHovered.part == HP_FLOOR || s_featureHovered.part == HP_CEIL)
				{
					selectSimilarFlats(s_featureHovered.sector, s_featureHovered.part);
				}
				else
				{
					selectSimilarWalls(s_featureHovered.sector, s_featureHovered.featureIndex, s_featureHovered.part);
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

		// Handle copy and paste.
		if (s_view == EDIT_VIEW_3D && s_featureHovered.sector && s_featureHovered.featureIndex >= 0)
		{
			applySurfaceTextures();
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

		if (s_singleClick)
		{
			handleSelectMode(s_featureHovered.sector, -1);
			s_featureCur.sector = s_featureHovered.sector;
			s_featureCur.featureIndex = 0;
			adjustGridHeight(s_featureCur.sector);
		}
		if (s_featureHovered.sector)
		{
			s_featureHovered.featureIndex = 0;
		}

		// Handle copy and paste.
		if (s_view == EDIT_VIEW_2D && s_featureHovered.sector)
		{
			applySurfaceTextures();
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

	struct EditorWallRaw
	{
		LevelTexture tex[WP_COUNT] = {};

		Vec2f vtx[2] = { 0 };
		s32 adjoinId = -1;
		s32 mirrorId = -1;

		u32 flags[3] = { 0 };
		s32 wallLight = 0;
	};

	bool addWallToList(const Vec2f* vtx, const EditorWall* wall, std::vector<EditorWallRaw>* wallList)
	{
		const Vec2f v0 = vtx[wall->idx[0]];
		const Vec2f v1 = vtx[wall->idx[1]];
		// Remove degenerate walls.
		if (TFE_Polygon::vtxEqual(&v0, &v1))
		{
			return false;
		}

		// Determine if the wall is already included and what its previous and next connections are.
		const s32 wallCount = (s32)wallList->size();
		EditorWallRaw* wallRaw = wallList->data();
		s32 prevWall = -1, nextWall = -1;
		for (s32 w = 0; w < wallCount; w++, wallRaw++)
		{
			if (TFE_Polygon::vtxEqual(&wallRaw->vtx[0], &v0) && TFE_Polygon::vtxEqual(&wallRaw->vtx[1], &v1))
			{
				return false;
			}
			if (TFE_Polygon::vtxEqual(&wallRaw->vtx[1], &v0))
			{
				prevWall = w;
			}
			if (TFE_Polygon::vtxEqual(&wallRaw->vtx[0], &v1))
			{
				nextWall = w;
			}
		}

		// Create a new wall but copy the parameters from the source wall.
		s32 id = (s32)wallList->size();
		EditorWallRaw newWall = { 0 };
		for (s32 i = 0; i < 3; i++)
		{
			newWall.flags[i] = wall->flags[i];
		}
		for (s32 i = 0; i < WP_COUNT; i++)
		{
			newWall.tex[i] = wall->tex[i];
		}
		newWall.wallLight = wall->wallLight;
		newWall.vtx[0] = v0;
		newWall.vtx[1] = v1;
		newWall.adjoinId = wall->adjoinId;
		newWall.mirrorId = wall->mirrorId;
		
		// Determine where to place the wall.
		if (prevWall >= 0)
		{
			// Insert the new wall after the previous connection.
			wallList->insert(wallList->begin() + prevWall + 1, newWall);
			// If the next wall is *before* the previous wall and the new wall connects the two loops together,
			// then resort so everything is in the correct order.
			const s32 loopLen = prevWall + 2 - nextWall;	// curIndex = prevWall + 1, len = curIndex - nextWall + 1
			if (nextWall >= 0 && nextWall < prevWall && loopLen < (s32)wallList->size())
			{
				// Capture the next loop.
				std::vector<EditorWallRaw> nextLoop;
				std::vector<s32> nextLoopIdx;
				EditorWallRaw* edge = &(*wallList)[nextWall];
				while (edge)
				{
					nextLoop.push_back(*edge);
					nextLoopIdx.push_back(nextWall);
					if (nextWall == (s32)wallList->size() - 1 || !TFE_Polygon::vtxEqual(&edge->vtx[1], &(edge + 1)->vtx[0]))
					{
						break;
					}
					edge++;
					nextWall++;
				}

				// Delete the next loop.
				const s32 nextLoopCount = (s32)nextLoop.size();
				const s32* list = nextLoopIdx.data();
				for (s32 i = nextLoopCount - 1; i >= 0; i--)
				{
					wallList->erase(wallList->begin() + list[i]);
				}

				// Finally re-insert the next loop again after the previous loop or at the end.
				const s32 newWallCount = (s32)wallList->size();
				wallRaw = wallList->data();
				const Vec2f* v0 = &nextLoop[0].vtx[0];
				s32 insertIdx = -1;
				for (s32 w = 0; w < newWallCount; w++, wallRaw++)
				{
					if (TFE_Polygon::vtxEqual(&wallRaw->vtx[1], v0))
					{
						// +1 = insert *after*.
						insertIdx = w + 1;
						break;
					}
				}
				if (insertIdx < 0)
				{
					// This can happen when there are "islands" - such as holes.
					// In this case, simply append the loop at the end.
					insertIdx = (s32)wallList->size();
				}

				for (s32 i = 0; i < nextLoopCount; i++)
				{
					wallList->insert(wallList->begin() + insertIdx + i, nextLoop[i]);
				}
			}
		}
		else if (nextWall >= 0)
		{
			// Insert the wall *before* the next connection.
			wallList->insert(wallList->begin() + nextWall, newWall);
		}
		else
		{
			// Insert the wall at the end (no connections).
			wallList->push_back(newWall);
		}
		return true;
	}

	s32 findMatchingMirror(EditorSector* sector, EditorSector* next, Vec2f v0, Vec2f v1)
	{
		// Find the correct mirror.
		EditorWall* wallnext = next->walls.data();
		const s32 nextWallCount = (s32)next->walls.size();
		for (s32 n = 0; n < nextWallCount; n++, wallnext++)
		{
			const Vec2f n0 = next->vtx[wallnext->idx[0]];
			const Vec2f n1 = next->vtx[wallnext->idx[1]];
			if (TFE_Polygon::vtxEqual(&v0, &n1) && TFE_Polygon::vtxEqual(&v1, &n0))
			{
				// Update the mirrored wall's mirror and adjoinId.
				return n;
			}
		}
		return -1;
	}

	void cleanSectors(const std::vector<s32>& selectedSectors)
	{
		// Re-check to make sure we have at least one sector.
		const s32 sectorCount = (s32)selectedSectors.size();
		if (sectorCount < 1)
		{
			LE_WARNING("Clean Sector(s) - no sectors selected.");
			return;
		}

		const s32* indices = selectedSectors.data();
		for (s32 s = 0; s < sectorCount; s++)
		{
			EditorSector* sector = &s_level.sectors[indices[s]];

			// 1. Record all of the walls we are keeping.
			std::vector<EditorWallRaw> wallsToKeep;
			{
				const s32 wallCount = (s32)sector->walls.size();
				EditorWall* wall = sector->walls.data();
				for (s32 w = 0; w < wallCount; w++, wall++)
				{
					if (!addWallToList(sector->vtx.data(), wall, &wallsToKeep))
					{
						if (wall->adjoinId >= 0 && wall->mirrorId >= 0)
						{
							EditorSector* next = &s_level.sectors[wall->adjoinId];
							if (wall->mirrorId < (s32)next->walls.size())
							{
								next->walls[wall->mirrorId].adjoinId = -1;
								next->walls[wall->mirrorId].mirrorId = -1;
							}
						}
					}
				}
			}

			// 2. Recreate the sector walls and vertices.
			const s32 wallCount = (s32)wallsToKeep.size();
			sector->walls.resize(wallCount);
			sector->vtx.clear();

			EditorWall* wall = sector->walls.data();
			const EditorWallRaw* wallSrc = wallsToKeep.data();
			for (s32 w = 0; w < wallCount; w++, wall++, wallSrc++)
			{
				wall->idx[0] = insertVertexIntoSector(sector, wallSrc->vtx[0]);
				wall->idx[1] = insertVertexIntoSector(sector, wallSrc->vtx[1]);
				for (s32 i = 0; i < 3; i++)
				{
					wall->flags[i] = wallSrc->flags[i];
				}
				for (s32 i = 0; i < WP_COUNT; i++)
				{
					wall->tex[i] = wallSrc->tex[i];
				}
				wall->wallLight = wallSrc->wallLight;

				if (wallSrc->adjoinId < 0)
				{
					wall->adjoinId = -1;
					wall->mirrorId = -1;
				}
				else
				{
					EditorSector* next = &s_level.sectors[wallSrc->adjoinId];
					const Vec2f v0 = wallSrc->vtx[0];
					const Vec2f v1 = wallSrc->vtx[1];
					if (wallSrc->mirrorId >= 0 && wallSrc->mirrorId < (s32)next->walls.size())
					{
						// Does the mirror match?
						const Vec2f nv0 = next->vtx[next->walls[wallSrc->mirrorId].idx[0]];
						const Vec2f nv1 = next->vtx[next->walls[wallSrc->mirrorId].idx[1]];
						if (TFE_Polygon::vtxEqual(&v0, &nv1) && TFE_Polygon::vtxEqual(&v1, &nv0))
						{
							// Update the mirrored wall's mirror and adjoinId.
							next->walls[wallSrc->mirrorId].adjoinId = sector->id;
							next->walls[wallSrc->mirrorId].mirrorId = w;
							wall->adjoinId = wallSrc->adjoinId;
							wall->mirrorId = wallSrc->mirrorId;
						}
						else
						{
							// If not, try to find the matching mirror.
							const s32 matchId = findMatchingMirror(sector, next, v0, v1);
							if (matchId >= 0)
							{
								// Update the mirrored wall's mirror and adjoinId.
								next->walls[matchId].adjoinId = sector->id;
								next->walls[matchId].mirrorId = w;
								wall->adjoinId = wallSrc->adjoinId;
								wall->mirrorId = matchId;
							}
							else
							{
								// If that fails, clear out the adjoin.
								wall->adjoinId = -1;
								wall->mirrorId = -1;
							}
						}
					}
					else
					{
						const s32 matchId = findMatchingMirror(sector, next, v0, v1);
						if (matchId >= 0)
						{
							// Update the mirrored wall's mirror and adjoinId.
							next->walls[matchId].adjoinId = sector->id;
							next->walls[matchId].mirrorId = w;
							wall->adjoinId = wallSrc->adjoinId;
							wall->mirrorId = matchId;
						}
						else
						{
							// If that fails, clear out the adjoin.
							wall->adjoinId = -1;
							wall->mirrorId = -1;
							LE_ERROR("Clean Sector(s) - invalid mirrorId in sector %d, wall %d.", sector->id, w);
						}
					}
				}
			}
			sectorToPolygon(sector);
		}
	}

	void edit_cleanSectors()
	{
		std::vector<s32> selectedSectors;

		// We must be in the wall (in 3D) or sector mode.
		bool canSelectSectors = (s_editMode == LEDIT_WALL && s_view == EDIT_VIEW_3D) || s_editMode == LEDIT_SECTOR;
		if (!canSelectSectors)
		{
			LE_WARNING("Clean Sector(s) - no sectors selected.");
			return;
		}

		// At least one sector must be selected.
		// If only one is selected, this will act as a "clean" - removing degenerate walls, re-ordering, etc.
		const s32 count = (s32)s_selectionList.size();
		if (count < 1)
		{
			LE_WARNING("Clean Sector(s) - no sectors selected.");
			return;
		}

		const FeatureId* feature = s_selectionList.data();
		for (s32 i = 0; i < count; i++)
		{
			s32 index;
			HitPart part;
			EditorSector* sector = unpackFeatureId(feature[i], &index, (s32*)&part);
			if (part == HP_FLOOR || part == HP_CEIL || s_editMode == LEDIT_SECTOR)
			{
				selectedSectors.push_back(sector->id);
			}
		}

		cleanSectors(selectedSectors);
	}

	void edit_joinSectors()
	{
		std::vector<s32> selectedSectors;

		// We must be in the wall (in 3D) or sector mode.
		bool canSelectSectors = (s_editMode == LEDIT_WALL && s_view == EDIT_VIEW_3D) || s_editMode == LEDIT_SECTOR;
		if (!canSelectSectors)
		{
			LE_WARNING("Join Sectors - no sectors selected.");
			return;
		}

		// At least one sector must be selected.
		// If only one is selected, this will act as a "clean" - removing degenerate walls, re-ordering, etc.
		const s32 count = (s32)s_selectionList.size();
		if (count < 2)
		{
			LE_WARNING("Join Sectors - at least two(2) sectors need to be selected.");
			return;
		}

		const FeatureId* feature = s_selectionList.data();
		for (s32 i = 0; i < count; i++)
		{
			s32 index;
			HitPart part;
			EditorSector* sector = unpackFeatureId(feature[i], &index, (s32*)&part);
			if (part == HP_FLOOR || part == HP_CEIL || s_editMode == LEDIT_SECTOR)
			{
				selectedSectors.push_back(sector->id);
			}
		}
		// Re-check to make sure we have at least two sectors.
		const s32 sectorCount = (s32)selectedSectors.size();
		if (sectorCount < 2)
		{
			LE_WARNING("Join Sectors - at least two(2) sectors need to be selected.");
			return;
		}

		std::sort(selectedSectors.begin() + 1, selectedSectors.end());
		const s32* indices = selectedSectors.data();
		// 1. Record all of the walls we are keeping.
		std::vector<EditorWallRaw> wallsToKeep;
		for (s32 s = 0; s < sectorCount; s++)
		{
			EditorSector* sector = &s_level.sectors[indices[s]];
			const s32 wallCount = (s32)sector->walls.size();
			EditorWall* wall = sector->walls.data();

			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				if (wall->adjoinId < 0 || !isInIntList(wall->adjoinId, &selectedSectors))
				{
					if (!addWallToList(sector->vtx.data(), wall, &wallsToKeep))
					{
						if (wall->adjoinId >= 0 && wall->mirrorId >= 0)
						{
							EditorSector* next = &s_level.sectors[wall->adjoinId];
							if (wall->mirrorId < (s32)next->walls.size())
							{
								next->walls[wall->mirrorId].adjoinId = -1;
								next->walls[wall->mirrorId].mirrorId = -1;
							}
						}
					}
				}
			}
		}

		// 2. Recreate the sector walls and vertices.
		const s32 primaryId = indices[0];
		const s32 wallCount = (s32)wallsToKeep.size();
		EditorSector* sector = &s_level.sectors[primaryId];
		sector->walls.resize(wallCount);
		sector->vtx.clear();

		EditorWall* wall = sector->walls.data();
		const EditorWallRaw* wallSrc = wallsToKeep.data();
		for (s32 w = 0; w < wallCount; w++, wall++, wallSrc++)
		{
			wall->idx[0] = insertVertexIntoSector(sector, wallSrc->vtx[0]);
			wall->idx[1] = insertVertexIntoSector(sector, wallSrc->vtx[1]);
			for (s32 i = 0; i < 3; i++)
			{
				wall->flags[i] = wallSrc->flags[i];
			}
			for (s32 i = 0; i < WP_COUNT; i++)
			{
				wall->tex[i] = wallSrc->tex[i];
			}
			wall->wallLight = wallSrc->wallLight;

			if (wallSrc->adjoinId < 0)
			{
				wall->adjoinId = -1;
				wall->mirrorId = -1;
			}
			else
			{
				EditorSector* next = &s_level.sectors[wallSrc->adjoinId];
				if (wallSrc->mirrorId >= 0 && wallSrc->mirrorId < (s32)next->walls.size())
				{
					// Update the mirrored wall's mirror and adjoinId.
					next->walls[wallSrc->mirrorId].adjoinId = primaryId;
					next->walls[wallSrc->mirrorId].mirrorId = w;
					wall->adjoinId = wallSrc->adjoinId;
					wall->mirrorId = wallSrc->mirrorId;
				}
				else
				{
					wall->adjoinId = -1;
					wall->mirrorId = -1;
					LE_ERROR("Join Sectors - invalid mirrorId in sector %d, wall %d.", sector->id, w);
				}
			}
		}
		sectorToPolygon(sector);

		// 3. Delete the extra sectors, note we don't delete sector[0]
		for (s32 s = sectorCount - 1; s > 0; s--)
		{
			edit_deleteSector(indices[s]);
		}
	}

	s32 findNextEdge(s32 wallCount, const EditorWall* walls, s32 idxToFind, s32 side)
	{
		for (s32 w = 0; w < wallCount; w++)
		{
			if (walls[w].idx[side] == idxToFind)
			{
				return w;
			}
		}
		return -1;
	}

	void insertEdgesIntoShape(s32 count, const s32* edgeIndices, const EditorSector* sector)
	{
		const Vec2f* vtx = sector->vtx.data();
		const EditorWall* walls = sector->walls.data();

		s32 start = -1;
		s32 i0 = -1;
		// edgeIndices[0] <-> s_geoEdit.shape[0]
		if (TFE_Polygon::vtxEqual(&s_geoEdit.shape[0], &vtx[walls[edgeIndices[0]].idx[0]]))
		{
			start = 0;
			i0 = 1;
		}
		else if (TFE_Polygon::vtxEqual(&s_geoEdit.shape[0], &vtx[walls[edgeIndices[0]].idx[1]]))
		{
			start = 0;
			i0 = 0;
		}
		// edgeIndices[1] <-> s_geoEdit.shape[0]
		else if (TFE_Polygon::vtxEqual(&s_geoEdit.shape[0], &vtx[walls[edgeIndices[count-1]].idx[0]]))
		{
			start = count - 1;
			i0 = 1;
		}
		else if (TFE_Polygon::vtxEqual(&s_geoEdit.shape[0], &vtx[walls[edgeIndices[count-1]].idx[1]]))
		{
			start = count - 1;
			i0 = 0;
		}
		// Can't find it...
		if (start < 0 || i0 < 0)
		{
			return;
		}

		s32 countToAdd = 0;
		Vec2f vtxToAdd[64];
		if (start == 0)
		{
			for (s32 i = start; i < count - 1; i++)
			{
				vtxToAdd[countToAdd++] = vtx[walls[edgeIndices[i]].idx[i0]];
			}
		}
		else
		{
			for (s32 i = start; i > 0; i--)
			{
				vtxToAdd[countToAdd++] = vtx[walls[edgeIndices[i]].idx[i0]];
			}
		}

		for (s32 i = countToAdd - 1; i >= 0; i--)
		{
			s_geoEdit.shape.push_back(vtxToAdd[i]);
		}
	}

	// Add all edges encountered as: [start, end)
	// dir = 1 or -1 (clockwise and counterclockwise).
	s32 walkEdges(const EditorSector* sector, s32 startEdge, s32 endEdge, s32* edges, s32 dir)
	{
		const s32 wallCount = (s32)sector->walls.size();
		const Vec2f* vtx = sector->vtx.data();
		const EditorWall* walls = sector->walls.data();

		const EditorWall* curWall = &walls[startEdge];
		const s32 i = dir > 0 ? 1 : 0;
		const s32 side = dir > 0 ? 0 : 1;
		s32 count = 0;
		s32 nextEdge = startEdge;

		// Do not include the first edge in the solution when walking backwards.
		if (dir == 1)
		{
			edges[count++] = nextEdge;
		}
		while (nextEdge != endEdge && count < 64)
		{
			nextEdge = findNextEdge(wallCount, walls, curWall->idx[i], side);
			if (nextEdge < 0)
			{
				return -1;
			}
			else if (nextEdge == endEdge && dir < 1)
			{
				break;
			}

			edges[count++] = nextEdge;
			curWall = &walls[nextEdge];
		}
		return count >= 64 ? -1 : count;
	}

	void edit_deleteObject(EditorSector* sector, s32 index)
	{
		if (sector && index >= 0 && index < (s32)sector->obj.size())
		{
			sector->obj.erase(sector->obj.begin() + index);
			clearEntityChanges();
		}
	}

	void edit_deleteLevelNote(s32 index)
	{
		s32 count = (s32)s_level.notes.size();
		if (index >= 0 && index < count)
		{
			s_level.notes.erase(s_level.notes.begin() + index);
			count--;
			// Reset IDs.
			LevelNote* note = s_level.notes.data();
			for (s32 i = 0; i < count; i++, note++)
			{
				note->id = i;
			}
		}
	}

	Vec2f getTextureOffset(EditorSector* sector, HitPart part, s32 index)
	{
		Vec2f offset = { 0 };
		if (part == HP_FLOOR)
		{
			offset = sector->floorTex.offset;
		}
		else if (part == HP_CEIL)
		{
			offset = sector->ceilTex.offset;
		}
		else if (part == HP_MID)
		{
			EditorWall* wall = &sector->walls[index];
			offset = wall->tex[WP_MID].offset;
		}
		else if (part == HP_BOT)
		{
			EditorWall* wall = &sector->walls[index];
			offset = wall->tex[WP_BOT].offset;
		}
		else if (part == HP_TOP)
		{
			EditorWall* wall = &sector->walls[index];
			offset = wall->tex[WP_TOP].offset;
		}
		else if (part == HP_SIGN)
		{
			EditorWall* wall = &sector->walls[index];
			offset = wall->tex[WP_SIGN].offset;
		}
		return offset;
	}

	void modifyTextureOffset(EditorSector* sector, HitPart part, s32 index, Vec2f delta)
	{
		if (part == HP_FLOOR)
		{
			sector->floorTex.offset.x += delta.x;
			sector->floorTex.offset.z += delta.z;
		}
		else if (part == HP_CEIL)
		{
			sector->ceilTex.offset.x += delta.x;
			sector->ceilTex.offset.z += delta.z;
		}
		else if (part == HP_MID)
		{
			EditorWall* wall = &sector->walls[index];
			wall->tex[WP_MID].offset.x += delta.x;
			wall->tex[WP_MID].offset.z += delta.z;
		}
		else if (part == HP_BOT)
		{
			EditorWall* wall = &sector->walls[index];
			wall->tex[WP_BOT].offset.x += delta.x;
			wall->tex[WP_BOT].offset.z += delta.z;
		}
		else if (part == HP_TOP)
		{
			EditorWall* wall = &sector->walls[index];
			wall->tex[WP_TOP].offset.x += delta.x;
			wall->tex[WP_TOP].offset.z += delta.z;
		}
		else if (part == HP_SIGN)
		{
			EditorWall* wall = &sector->walls[index];
			wall->tex[WP_SIGN].offset.x += delta.x;
			wall->tex[WP_SIGN].offset.z += delta.z;
		}
	}
				
	void updateWindowControls()
	{
		if (isUiModal()) { return; }

		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		if (!TFE_Input::relativeModeEnabled() && (mx < s_editWinPos.x || mx >= s_editWinPos.x + s_editWinSize.x || my < s_editWinPos.z || my >= s_editWinPos.z + s_editWinSize.z))
		{
			// Nothing is "hovered" if the mouse is not in the window.
			s_featureHovered = {};
			return;
		}

		// Handle Escape, this gives a priority of what is being escaped.
		if (TFE_Input::keyPressed(KEY_ESCAPE))
		{
			if (s_geoEdit.drawStarted)
			{
				s_geoEdit.drawStarted = false;
			}
			else if (s_editGuidelines.drawStarted)
			{
				s_editGuidelines.drawStarted = false;
			}
			else if (!s_startTexMove)
			{
				edit_clearSelections();
			}
		}

		// 2D extrude - hover over line, left click and drag.
		// 3D extrude - hover over wall, left click to add points (or shift to drag a box), double-click to just extrude the full wall.
		const bool extrude = s_geoEdit.extrudeEnabled && s_editMode == LEDIT_DRAW;

		// General movement handling - only allow movement once the mouse has moved a certain distance initially so that
		// it is possible to select features off the grid without them snapping to the grid.
		if (s_singleClick)
		{
			s_moveInitPos = { mx, my };
			s_canMoveFeature = false;
		}
		else if (s_leftMouseDown)
		{
			const Vec2i delta = { ::abs(mx - s_moveInitPos.x), ::abs(my - s_moveInitPos.z) };
			if (delta.x > c_moveThreshold || delta.z > c_moveThreshold)
			{
				s_canMoveFeature = true;
			}
		}

		if (s_view == EDIT_VIEW_2D)
		{
			cameraControl2d(mx, my);

			// Selection
			Vec2f worldPos = mouseCoordToWorldPos2d(mx, my);
			s_cursor3d = { worldPos.x, 0.0f, worldPos.z };
			
			// Always check for the hovered sector, since sectors can overlap.
			s_featureHovered.sector = findSector2d(worldPos, s_curLayer);
			s_featureHovered.featureIndex = -1;

			if (s_singleClick)
			{
				handleSelectMode({ s_cursor3d.x, s_featureHovered.sector ? s_featureHovered.sector->floorHeight : 0.0f, s_cursor3d.z });
			}

			// TODO: Move to central hotkey list.
			if (s_featureHovered.sector && TFE_Input::keyModDown(KEYMOD_CTRL) && TFE_Input::keyPressed(KEY_G))
			{
				adjustGridHeight(s_featureHovered.sector);
			}

			handleLevelNoteEdit(nullptr/*2d so no ray hit info*/, s_rayDir);
			if (s_editMode == LEDIT_NOTES)
			{
				return;
			}
						
			if (s_editMode == LEDIT_DRAW)
			{
				if (extrude) { handleSectorExtrude(nullptr/*2d so no ray hit info*/); }
				else { handleSectorDraw(nullptr/*2d so no ray hit info*/); }
				return;
			}
			else if (s_editMode == LEDIT_GUIDELINES)
			{
				handleGuidelinesEdit(nullptr/*2d so no ray hit info*/);
				return;
			}
			else if (s_editMode == LEDIT_ENTITY)
			{
				handleEntityEdit(nullptr/*2d so no ray hit info*/, s_rayDir);
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
				if (hoveredSector && !sector_isInteractable(hoveredSector))
				{
					hoveredSector = nullptr;
					s_featureHovered = {};
				}

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
				if (hoverSector && !sector_isInteractable(hoverSector))
				{
					hoverSector = nullptr;
					s_featureHovered = {};
				}

				handleMouseControlWall(worldPos);
			}

			// Texture alignment.
			handleTextureAlignment();
		}
		else if (s_view == EDIT_VIEW_3D)
		{
			viewport_clearRail();

			cameraControl3d(mx, my);
			s_featureHovered = {};
			s32 layer = (s_editFlags & LEF_SHOW_ALL_LAYERS) ? LAYER_ANY : s_curLayer;

			// TODO: Hotkeys.
			if (TFE_Input::keyPressed(KEY_G) && !TFE_Input::keyModDown(KEYMOD_CTRL))
			{
				s_gravity = !s_gravity;
				if (s_gravity) { LE_INFO("Gravity Enabled."); }
				else { LE_INFO("Gravity Disabled."); }
			}
			if (s_gravity)
			{
				Ray ray = { s_camera.pos, { 0.0f, -1.0f, 0.0f}, 32.0f, layer };
				RayHitInfo hitInfo;
				if (traceRay(&ray, &hitInfo, false, false))
				{
					EditorSector* colSector = &s_level.sectors[hitInfo.hitSectorId];
					f32 floorHeight = colSector->floorHeight;
					if (colSector->secHeight < 0) { floorHeight += colSector->secHeight; }
					else if (colSector->secHeight > 0)
					{
						f32 secHeight = colSector->floorHeight + colSector->secHeight;
						if (ray.origin.y > secHeight - 1.0f)
						{
							floorHeight = secHeight;
						}
					}

					f32 newY = floorHeight + 5.8f;
					f32 blendFactor = std::min(1.0f, (f32)TFE_System::getDeltaTime() * 10.0f);
					s_camera.pos.y = newY * blendFactor + s_camera.pos.y * (1.0f - blendFactor);
				}
			}
					
			// TODO: Move out to common place for hotkeys.
			bool hitBackfaces = TFE_Input::keyDown(KEY_B);
			s_rayDir = mouseCoordToWorldDir3d(mx, my);

			RayHitInfo hitInfo;
			Ray ray = { s_camera.pos, s_rayDir, 1000.0f, layer };
			const bool rayHit = traceRay(&ray, &hitInfo, hitBackfaces, s_sectorDrawMode == SDM_TEXTURED_CEIL || s_sectorDrawMode == SDM_TEXTURED_FLOOR);
			if (rayHit) { s_cursor3d = hitInfo.hitPos; }
			else  		{ s_cursor3d = rayGridPlaneHit(s_camera.pos, s_rayDir); }

			// Note that the 3D cursor must be adjusted if the grid is shown over the geometry.
			if ((s_gridFlags & GFLAG_OVER) && hitInfo.hitWallId < 0)
			{
				s_cursor3d = rayGridPlaneHit(s_camera.pos, s_rayDir);
			}

			if (s_singleClick)
			{
				handleSelectMode(s_cursor3d);
			}

			handleLevelNoteEdit(&hitInfo, s_rayDir);
			if (s_editMode == LEDIT_NOTES)
			{
				return;
			}

			if (s_editMode == LEDIT_DRAW)
			{
				if (extrude) { handleSectorExtrude(&hitInfo); }
				else { handleSectorDraw(&hitInfo); }
				return;
			}
			else if (s_editMode == LEDIT_GUIDELINES)
			{
				handleGuidelinesEdit(&hitInfo);
				return;
			}
			else if (s_editMode == LEDIT_ENTITY)
			{
				handleEntityEdit(&hitInfo, s_rayDir);
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
				else if (s_singleClick)
				{
					s_featureCur = {};
					selection_clear();
				}
			}
			
			// Texture alignment.
			handleTextureAlignment();
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
				saveLevel();
			}
			if (ImGui::MenuItem("Reload", "Ctrl+R", (bool*)NULL))
			{
				loadLevelFromAsset(s_levelAsset);
			}
			if (ImGui::MenuItem("Save Snapshot", "Ctrl+N", (bool*)NULL))
			{
				// Bring up a pop-up where the snapshot can be named.
			}
			if (ImGui::MenuItem("Load Snapshot", "Ctrl+L", (bool*)NULL))
			{
				// Bring up a pop-up where the desired named snapshot can be found.
			}
			if (!projectActive) { enableNextItem(); }

			if (ImGui::MenuItem("Close", NULL, (bool*)NULL))
			{
				// TODO: If the level has changed, pop up a warning and allow the level to be saved.
				disableAssetEditor();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("User Preferences", NULL, (bool*)NULL))
			{
				openEditorPopup(POPUP_LEV_USER_PREF);
			}
			if (ImGui::MenuItem("Test Options", NULL, (bool*)NULL))
			{
				// TODO
			}
			ImGui::Separator();
			if (ImGui::MenuItem("INF Items", NULL, (bool*)NULL))
			{
				// TODO
			}
			disableNextItem(); // TODO
			if (ImGui::MenuItem("Goals", NULL, (bool*)NULL))
			{
				// TODO
			}
			if (ImGui::MenuItem("Mission Briefing", NULL, (bool*)NULL))
			{
				// TODO
			}
			enableNextItem(); // End TODO
			ImGui::Separator();
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
		if (ImGui::BeginMenu("Tools"))
		{
			menuActive = true;
			if (ImGui::MenuItem("Lighting", NULL, (bool*)NULL))
			{
				openEditorPopup(POPUP_LIGHTING);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Join Sectors", NULL, (bool*)NULL))
			{
				edit_joinSectors();
			}
			if (ImGui::MenuItem("Clean Sector(s)", NULL, (bool*)NULL))
			{
				edit_cleanSectors();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Find Sector", NULL, (bool*)NULL))
			{
				// TODO
			}
			if (ImGui::MenuItem("Find/Replace Texture", NULL, (bool*)NULL))
			{
				// TODO
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Grid"))
		{
			menuActive = true;
			if (ImGui::MenuItem("Reset", ""))
			{
				s_gridFlags = GFLAG_NONE;
				resetGrid();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Show Over Sectors", "", s_gridFlags & GFLAG_OVER))
			{
				if (s_gridFlags & GFLAG_OVER) { s_gridFlags &= ~GFLAG_OVER; }
				else { s_gridFlags |= GFLAG_OVER; }
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Align To Edge", ""))
			{
				// If an edge is selected.
				if (s_featureCur.part != HP_FLOOR && s_featureCur.part != HP_CEIL && s_featureCur.featureIndex >= 0 && s_featureCur.sector && !s_featureCur.isObject)
				{
					const Vec2f* vtx = s_featureCur.sector->vtx.data();
					const EditorWall* wall = &s_featureCur.sector->walls[s_featureCur.featureIndex];
					alignGridToEdge(vtx[wall->idx[0]], vtx[wall->idx[1]]);
				}
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
					Vec2f mapPos = { s_camera.pos.x, -s_camera.pos.z };
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
				s_view = EDIT_VIEW_3D_GAME;
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
			if (ImGui::MenuItem("Group Color", "Ctrl+F3", s_sectorDrawMode == SDM_GROUP_COLOR))
			{
				s_sectorDrawMode = SDM_GROUP_COLOR;
			}
			if (ImGui::MenuItem("Textured (Floor)", "Ctrl+F4", s_sectorDrawMode == SDM_TEXTURED_FLOOR))
			{
				s_sectorDrawMode = SDM_TEXTURED_FLOOR;
			}
			if (ImGui::MenuItem("Textured (Ceiling)", "Ctrl+F5", s_sectorDrawMode == SDM_TEXTURED_CEIL))
			{
				s_sectorDrawMode = SDM_TEXTURED_CEIL;
			}
			ImGui::Separator();
			bool fullbright = (s_editFlags & LEF_FULLBRIGHT) != 0;
			if (ImGui::MenuItem("Fullbright", "Ctrl+F6", fullbright))
			{
				fullbright = !fullbright;
				if (fullbright) { s_editFlags |= LEF_FULLBRIGHT; }
				else { s_editFlags &= ~LEF_FULLBRIGHT; }
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

	s32 getSectorNameLimit()
	{
		return c_vanillaDarkForcesNameLimit;
	}

	struct SelectSaveState
	{
		LevelEditMode editMode;
		u32 editFlags;
		u32 lwinOpen;
		s32 curLayer;

		Feature featureHovered;
		Feature featureCur;
		Feature featureCurWall;

		SelectionList selectionList;
	};
	static SelectSaveState s_selectSaveState;
	static bool s_selectSaveStateSaved = false;
		
	void setSelectMode(SelectMode mode)
	{
		s_selectMode = mode;
		if (s_selectMode != SELECTMODE_NONE)
		{
			// Save select state.
			s_selectSaveState.editMode = s_editMode;
			s_selectSaveState.editFlags = s_editFlags;
			s_selectSaveState.lwinOpen = s_lwinOpen;
			s_selectSaveState.curLayer = s_curLayer;
			s_selectSaveState.featureHovered = s_featureHovered;
			s_selectSaveState.featureCur = s_featureCur;
			s_selectSaveState.featureCurWall = s_featureCurWall;
			s_selectSaveState.selectionList = s_selectionList;
			s_selectSaveStateSaved = true;

			s_selectionList.clear();
			s_featureHovered = {};
			s_featureCur = {};
			s_featureCurWall = {};
		}
		else if (s_selectSaveStateSaved)
		{
			s_editMode = s_selectSaveState.editMode;
			s_editFlags = s_selectSaveState.editFlags;
			s_lwinOpen = s_selectSaveState.lwinOpen;
			s_curLayer = s_selectSaveState.curLayer;
			s_featureHovered = s_selectSaveState.featureHovered;
			s_featureCur = s_selectSaveState.featureCur;
			s_featureCurWall = s_selectSaveState.featureCurWall;
			s_selectionList = s_selectSaveState.selectionList;
			s_selectSaveStateSaved = false;
		}
	}

	SelectMode getSelectMode()
	{
		return s_selectMode;
	}

	void handleSelectMode(Vec3f pos)
	{
		if (s_selectMode == SELECTMODE_POSITION)
		{
			Vec2f posXZ = { pos.x, pos.z };
			snapToGrid(&posXZ);
			
			editor_handleSelection({ posXZ.x, pos.y, posXZ.z });
		}
	}

	void handleSelectMode(EditorSector* sector, s32 wallIndex)
	{
		if (sector && wallIndex < 0 && (s_selectMode == SELECTMODE_SECTOR || s_selectMode == SELECTMODE_SECTOR_OR_WALL))
		{
			editor_handleSelection(sector);
		}
		else if (sector && wallIndex >= 0 && (s_selectMode == SELECTMODE_WALL || s_selectMode == SELECTMODE_SECTOR_OR_WALL))
		{
			editor_handleSelection(sector, wallIndex);
		}
	}
		
	void drawViewportInfo(s32 index, Vec2i mapPos, const char* info, f32 xOffset, f32 yOffset, f32 alpha, u32 overrideColor)
	{
		char id[256];
		sprintf(id, "##ViewportInfo%d", index);
		ImVec2 size = ImGui::CalcTextSize(info);
		// Padding is 8, so 16 total is added.
		size.x += 16.0f;
		size.y += 16.0f;

		const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav;

		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);

		if (overrideColor)
		{
			u32 a = (overrideColor >> 24u) & 0xff;
			u32 b = (overrideColor >> 16u) & 0xff;
			u32 g = (overrideColor >>  8u) & 0xff;
			u32 r = (overrideColor) & 0xff;
			const f32 scale = 1.0f / 255.0f;

			ImGui::PushStyleColor(ImGuiCol_Text, { f32(r)*scale, f32(g)*scale, f32(b)*scale, f32(a)*scale*alpha });
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, { 0.8f, 0.9f, 1.0f, 0.5f*alpha });
		}
		ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.06f, 0.06f, 0.06f, 0.94f*0.75f*alpha });
		ImGui::PushStyleColor(ImGuiCol_Border, { 0.43f, 0.43f, 0.50f, 0.25f*alpha });

		ImGui::SetNextWindowPos({ (f32)mapPos.x + xOffset, (f32)mapPos.z + yOffset });
		if (ImGui::BeginChild(id, size, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border, window_flags))
		{
			ImGui::Text("%s", info);
		}
		ImGui::EndChild();

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
	}

	void floatToString(f32 value, char* str)
	{
		const f32 valueOnGrid = s_grid.size != 0.0f ? floorf(value/s_grid.size) * s_grid.size : 1.0f;
		const f32 eps = 0.00001f;

		if (fabsf(floorf(value)-value) < eps)
		{
			// Integer value.
			sprintf(str, "%d", s32(value));
		}
		else if (s_grid.size < 1.0f && s_grid.size > 0.0f && fabsf(valueOnGrid - value) < eps)
		{
			// Fractional grid-aligned value, show as a fraction so its easier to see the value in terms of grid cells.
			s32 den = s32(1.0f / s_grid.size);
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
			mapOffset.x = floorf(mapOffset.x * (n.x*0.5f - 0.5f));
		}
		else
		{
			mapOffset.x = floorf(mapOffset.x * (-(1.0f - n.x)*0.5f));
		}
		mapOffset.z = floorf(n.z * mapOffset.z - 16.0f);

		mapPos.x += s32(mapOffset.x);
		mapPos.z += s32(mapOffset.z);
		if (mapOffset_) { *mapOffset_ = mapOffset; }
	}

	f32 getEntityLabelPos(const Vec3f pos, const Entity* entity, const EditorSector* sector)
	{
		f32 height = entity->size.z;
		f32 y = pos.y;
		if (entity->type == ETYPE_3D)
		{
			y = pos.y + entity->obj3d->bounds[1].y;
		}
		else if (entity->type != ETYPE_SPIRIT && entity->type != ETYPE_SAFE)
		{
			f32 offset = -(entity->offset.y + fabsf(entity->st[1].z - entity->st[0].z)) * 0.1f;
			// If the entity is on the floor, make sure it doesn't stick through it for editor visualization.
			if (fabsf(pos.y - sector->floorHeight) < 0.5f) { offset = max(0.0f, offset); }
			y = pos.y + offset;
		}
		if (entity->type != ETYPE_3D)
		{
			y += entity->size.z;
		}
		return y;
	}

	void handleEditorActions()
	{
		if (getEditAction(ACTION_UNDO))
		{
			levHistory_undo();
		}
		else if (getEditAction(ACTION_REDO))
		{
			levHistory_redo();
		}
		if (getEditAction(ACTION_SHOW_ALL_LABELS))
		{
			s_showAllLabels = !s_showAllLabels;
		}
	}

	EditorSector* sectorHoveredOrSelected()
	{
		EditorSector* sector = nullptr;
		if (s_editMode == LEDIT_SECTOR)
		{
			sector = s_featureCur.sector;
			if (!sector && !s_selectionList.empty())
			{
				FeatureId featureId = s_selectionList[0];
				s32 featureIndex, part;
				sector = unpackFeatureId(featureId, &featureIndex, &part);
			}
			if (!sector)
			{
				sector = s_featureHovered.sector;
			}
		}
		else if (s_editMode == LEDIT_WALL)
		{
			if (s_featureCur.sector && (s_featureCur.part == HP_CEIL || s_featureCur.part == HP_FLOOR))
			{
				sector = s_featureCur.sector;
			}
			else if (s_featureHovered.sector && (s_featureHovered.part == HP_CEIL || s_featureHovered.part == HP_FLOOR))
			{
				sector = s_featureHovered.sector;
			}
		}
		return sector;
	}
		
	bool copyableItemHoveredOrSelected()
	{
		if (s_editMode == LEDIT_SECTOR)
		{
			return s_featureHovered.sector || s_featureCur.sector || !s_selectionList.empty();
		}
		else if (s_editMode == LEDIT_WALL)
		{
			return (s_featureHovered.sector && (s_featureHovered.part == HP_CEIL || s_featureHovered.part == HP_FLOOR)) ||
				(s_featureCur.sector && (s_featureCur.part == HP_CEIL || s_featureCur.part == HP_FLOOR));// || selectionHasSectors();
		}
		else if (s_editMode == LEDIT_ENTITY)
		{
			return (s_featureHovered.sector && s_featureHovered.featureIndex >= 0 && s_featureHovered.isObject) ||
				(s_featureCur.sector && s_featureCur.featureIndex >= 0 && s_featureCur.isObject);
		}
		return false;
	}

	bool itemHoveredOrSelected()
	{
		if (s_editMode == LEDIT_SECTOR)
		{
			return s_featureHovered.sector || s_featureCur.sector || !s_selectionList.empty();
		}
		else if (s_editMode == LEDIT_WALL)
		{
			return (s_featureHovered.sector && (s_featureHovered.featureIndex >= 0 || s_featureHovered.part == HP_CEIL || s_featureHovered.part == HP_FLOOR)) ||
				(s_featureCur.sector && (s_featureCur.featureIndex >= 0 || s_featureCur.part == HP_CEIL || s_featureCur.part == HP_FLOOR));
		}
		else if (s_editMode == LEDIT_VERTEX)
		{
			return (s_featureHovered.sector && s_featureHovered.featureIndex >= 0) || (s_featureCur.sector && s_featureCur.featureIndex >= 0) ||
				!s_selectionList.empty();
		}
		else if (s_editMode == LEDIT_ENTITY)
		{
			return (s_featureHovered.sector && s_featureHovered.featureIndex >= 0 && s_featureHovered.isObject) ||
				(s_featureCur.sector && s_featureCur.featureIndex >= 0 && s_featureCur.isObject);
		}
		return false;
	}

	void openViewportContextWindow()
	{
		s_contextMenu = CONTEXTMENU_VIEWPORT;
		s_modalUiActive = true;
	}

	void updateContextWindow()
	{
		const bool escapePressed = TFE_Input::keyPressed(KEY_ESCAPE);
		if (s_contextMenu == CONTEXTMENU_NONE || s_editMode == LEDIT_DRAW || escapePressed)
		{
			// "Eat" the escape key to avoid accidentally de-selecting a group.
			if (escapePressed && s_contextMenu != CONTEXTMENU_NONE && s_editMode != LEDIT_DRAW)
			{
				TFE_Input::clearKeyPressed(KEY_ESCAPE);
			}
			// Clear out the context menu.
			s_contextMenu = CONTEXTMENU_NONE;
			s_modalUiActive = false;
			return;
		}

		bool closeMenu = false;
		const u32 flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
		ImGui::SetNextWindowPos({(f32)s_rightMousePos.x, (f32)s_rightMousePos.z});
		bool open = true;
		bool leftClick = TFE_Input::mousePressed(MBUTTON_LEFT);
		if (ImGui::Begin("##ContextMenuFrame", &open, flags))
		{
			EditorSector* curSector = sectorHoveredOrSelected();
			if (curSector)
			{
				ImGui::MenuItem("Add To Current Group", NULL, (bool*)NULL);
				if (leftClick && mouseInsideItem())
				{
					curSector->groupId = groups_getCurrentId();
					closeMenu = true;
				}
				ImGui::Separator();
			}
			if (copyableItemHoveredOrSelected())
			{
				ImGui::MenuItem("Cut", "Ctrl+X", (bool*)NULL);
				if (leftClick && mouseInsideItem())
				{
					// TODO
					closeMenu = true;
				}
				ImGui::MenuItem("Copy", "Ctrl+C", (bool*)NULL);
				if (leftClick && mouseInsideItem())
				{
					// TODO
					closeMenu = true;
				}
				ImGui::MenuItem("Duplicate", "Ctrl+D", (bool*)NULL);
				if (leftClick && mouseInsideItem())
				{
					// TODO
					closeMenu = true;
				}
			}
			if (itemHoveredOrSelected())
			{
				ImGui::MenuItem("Delete", "Del", (bool*)NULL);
				if (leftClick && mouseInsideItem())
				{
					// TODO
					closeMenu = true;
				}
			}
			ImGui::MenuItem("Paste", "Ctrl+V", (bool*)NULL);
			if (leftClick && mouseInsideItem())
			{
				// TODO
				closeMenu = true;
			}
		}
		ImGui::End();

		// Close the context menu on left-click, right-click, and menu item selection.
		if (leftClick || TFE_Input::mousePressed(MBUTTON_RIGHT) || closeMenu)
		{
			// Clear the context menu and modal UI.
			s_contextMenu = CONTEXTMENU_NONE;
			s_modalUiActive = false;
			// Clear mouse clicks.
			s_singleClick = false;
			s_doubleClick = false;
			s_rightClick = false;
			// "Eat" the left click to avoid click-through.
			// Note right-click is intentionally not treated this way to make camera movement/rotation after context menu
			// feel better.
			TFE_Input::clearMouseButtonPressed(MBUTTON_LEFT);
			// Clear out accumulated mouse move that happened while the menu was open.
			TFE_Input::clearAccumulatedMouseMove();
		}
	}

	void updateOutput()
	{
		s_outputHeight = infoPanelOutput(s_viewportSize.x + 16);
	}

	void setEditMode(LevelEditMode mode)
	{
		s_editMode = mode;
		s_editMove = false;
		s_startTexMove = false;
		s_featureCur = {};
		s_featureHovered = {};
		s_featureTex = {};
		selection_clear();
		commitCurEntityChanges();
	}

	void update()
	{
		levelScript_update();
		updateViewportScroll();
		handleHotkeys();
		
		pushFont(TFE_Editor::FONT_SMALL);
		updateContextWindow();
		updateWindowControls();
		handleEditorActions();
		updateOutput();

		u32 viewportRenderFlags = 0u;
		if (s_drawActions & DRAW_ACTION_CURVE)
		{
			viewportRenderFlags |= VRF_CURVE_MOD;
		}

		viewport_update((s32)UI_SCALE(480) + 16, (s32)UI_SCALE(68) + 18 + s_outputHeight);
		viewport_render(s_view, viewportRenderFlags);

		// Toolbar
		s_modalUiActive = s_contextMenu != CONTEXTMENU_NONE;
		toolbarBegin();
		{
			const char* toolbarTooltips[]=
			{
				"Play the level",
				"Sector draw mode",
				"Vertex mode",
				"Wall/Surface mode",
				"Sector mode",
				"Entity mode",
				"Guideline draw mode",
				"Note placement",
			};
			const char* csgTooltips[] =
			{
				"Draw independent sectors",
				"Merge sectors",
				"Subtract sectors"
			};
			const IconId c_toolbarIcon[] = { ICON_PLAY, ICON_DRAW, ICON_VERTICES, ICON_EDGES, ICON_CUBE, ICON_ENTITY, ICON_GUIDELINES, ICON_NOTES };
			if (iconButton(c_toolbarIcon[0], toolbarTooltips[0], false))
			{
				commitCurEntityChanges();
				play();
			}
			ImGui::SameLine(0.0f, 32.0f);

			// Guidelines and Notes.
			for (u32 i = 0; i < 2; i++)
			{
				LevelEditMode mode = LevelEditMode(i + LEDIT_GUIDELINES);
				if (iconButton(c_toolbarIcon[mode], toolbarTooltips[mode], mode == s_editMode))
				{
					setEditMode(mode);
				}
				ImGui::SameLine();
			}
			ImGui::SameLine(0.0f, 32.0f);

			// Level Data.
			for (u32 i = 1; i < 6; i++)
			{
				LevelEditMode mode = LevelEditMode(i);
				if (iconButton(c_toolbarIcon[mode], toolbarTooltips[mode], mode == s_editMode))
				{
					setEditMode(mode);
				}
				ImGui::SameLine();
			}

			// CSG Toolbar.
			ImGui::SameLine(0.0f, 32.0f);
			void* gpuPtr = TFE_RenderBackend::getGpuPtr(s_boolToolbarData);
			for (u32 i = 0; i < 3; i++)
			{
				if (drawToolbarBooleanButton(gpuPtr, i, i == s_geoEdit.boolMode, csgTooltips[i]))
				{
					s_geoEdit.boolMode = BoolMode(i);
				}
				ImGui::SameLine();
			}

			ImGui::SameLine(0.0f, 32.0f);
			ImGui::PushItemWidth(64.0f);
			if (ImGui::BeginCombo("Grid Size", c_gridSizes[s_gridIndex]))
			{
				s_modalUiActive = true;
				for (int n = 0; n < TFE_ARRAYSIZE(c_gridSizes); n++)
				{
					if (ImGui::Selectable(c_gridSizes[n], n == s_gridIndex))
					{
						s_gridIndex = n;
						s_grid.size = c_gridSizeMap[s_gridIndex];
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
				s_modalUiActive = true;
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
		drawInfoPanel(s_view);

		// Browser
		drawBrowser(s_editMode == LEDIT_ENTITY ? BROWSE_ENTITY : BROWSE_TEXTURE);

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

			if (!isUiModal())
			{
				const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
					| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;

				if (s_rightClick && editWinHovered)
				{
					openViewportContextWindow();
				}
				// Display vertex info.
				else if (s_featureHovered.sector && s_featureHovered.featureIndex >= 0 && editWinHovered && s_editMode == LEDIT_VERTEX)
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
							mapPos.x -= s32(textOffset + 12);
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
				else if (s_editMode == LEDIT_ENTITY)
				{
					const EditorSector* sector = s_featureHovered.sector;
					if (sector && s_featureHovered.featureIndex >= 0 && s_featureHovered.isObject)
					{
						const EditorObject* obj = &sector->obj[s_featureHovered.featureIndex];
						const Entity* entity = &s_level.entities[obj->entityId];

						bool showInfo = true;
						Vec2i mapPos;
						if (s_view == EDIT_VIEW_2D)
						{
							Vec2f labelPos = { obj->pos.x, obj->pos.z + entity->size.x*0.5f };
							Vec2i screenPos = worldPos2dToMap(labelPos);

							s32 textOffset = (s32)floorf(ImGui::CalcTextSize(entity->name.c_str()).x * 0.5f);
							mapPos = { screenPos.x - textOffset, screenPos.z - 40 };
						}
						else if (s_view == EDIT_VIEW_3D)
						{
							Vec3f labelPos = { obj->pos.x, getEntityLabelPos(obj->pos, entity, sector), obj->pos.z };
							Vec2f screenPos;
							if (worldPosToViewportCoord(labelPos, &screenPos))
							{
								f32 textOffset = floorf(ImGui::CalcTextSize(entity->name.c_str()).x * 0.5f);
								mapPos = { s32(screenPos.x - textOffset), s32(screenPos.z) - 40 };
							}
							else
							{
								showInfo = false;
							}
						}
						if (showInfo)
						{
							drawViewportInfo(0, mapPos, entity->name.c_str(), 0, 0);
						}
					}
				}
 				else if (s_editMode == LEDIT_DRAW && s_geoEdit.drawStarted)
				{
					if (s_geoEdit.drawMode == DMODE_RECT)
					{
						Vec2f cen[2];
						f32 width, height;

						// Get the rect size, taking into account the grid.
						// Note that the behavior is different if extrude is enabled -
						// since in that case the rect is *already* in "grid space".
						if (s_geoEdit.extrudeEnabled)
						{
							const f32 x0 = std::max(s_geoEdit.shape[0].x, s_geoEdit.shape[1].x);
							const f32 x1 = std::min(s_geoEdit.shape[0].x, s_geoEdit.shape[1].x);
							const f32 z0 = std::max(s_geoEdit.shape[0].z, s_geoEdit.shape[1].z);
							const f32 z1 = std::min(s_geoEdit.shape[0].z, s_geoEdit.shape[1].z);
							const f32 midX = (x0 + x1) * 0.5f;
							const f32 midZ = (z0 + z1) * 0.5f;
							
							width  = fabsf(x1 - x0);
							height = fabsf(z1 - z0);
							cen[0] = { midX, z0 },
							cen[1] = { x0, midZ };
						}
						else
						{
							const Vec2f g[] = { posToGrid(s_geoEdit.shape[0]), posToGrid(s_geoEdit.shape[1]) };

							Vec2f vtx[4];
							getGridOrientedRect(s_geoEdit.shape[0], s_geoEdit.shape[1], vtx);

							width  = fabsf(g[1].x - g[0].x);
							height = fabsf(g[1].z - g[0].z);
							cen[0] = { (vtx[0].x + vtx[1].x) * 0.5f, (vtx[0].z + vtx[1].z) * 0.5f };
							cen[1] = { (vtx[1].x + vtx[2].x) * 0.5f, (vtx[1].z + vtx[2].z) * 0.5f };
						}

						// Once we have the rect sizes and edge center positions,
						// draw the length labels.
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
								Vec3f worldPos = { cen[i].x, s_grid.height, cen[i].z };
								if (s_geoEdit.extrudeEnabled)
								{
									worldPos = extrudePoint2dTo3d(cen[i], s_geoEdit.drawHeight[0]);
								}

								Vec2f screenPos;
								if (worldPosToViewportCoord(worldPos, &screenPos))
								{
									drawViewportInfo(i, { (s32)screenPos.x, (s32)screenPos.z }, info, -16.0f + offs[i].x - textOffset, -16.0f + offs[i].z);
								}
							}
						}
					}
					else if (s_geoEdit.drawMode == DMODE_SHAPE && !s_geoEdit.shape.empty())
					{
						const s32 count = (s32)s_geoEdit.shape.size();
						const Vec2f* vtx = s_geoEdit.shape.data();
						// Only show a limited number of wall lengths, otherwise it just turns into noise.
						for (s32 v = 0; v < count; v++)
						{
							const Vec2f* v0 = &vtx[v];
							const Vec2f* v1 = v+1 < count ? &vtx[v + 1] : &s_geoEdit.drawCurPos;

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
								Vec3f worldPos = { (v0->x + v1->x)*0.5f, s_grid.height, (v0->z + v1->z)*0.5f };
								if (s_geoEdit.extrudeEnabled)
								{
									Vec3f w0 = extrudePoint2dTo3d(*v0, s_geoEdit.drawHeight[0]);
									Vec3f w1 = extrudePoint2dTo3d(*v1, s_geoEdit.drawHeight[0]);
									worldPos.x = (w0.x + w1.x)*0.5f;
									worldPos.y = (w0.y + w1.y)*0.5f;
									worldPos.z = (w0.z + w1.z)*0.5f;
								}

								Vec2f screenPos;
								if (worldPosToViewportCoord(worldPos, &screenPos))
								{
									screenPos.x += mapOffset.x;
									screenPos.z += mapOffset.z;
									drawViewportInfo(v, { (s32)screenPos.x, (s32)screenPos.z }, info, 0, 0);
								}
							}
						}
					}
					else if ((s_geoEdit.drawMode == DMODE_RECT_VERT || s_geoEdit.drawMode == DMODE_SHAPE_VERT) && s_view == EDIT_VIEW_3D)
					{
						f32 height = (s_geoEdit.drawHeight[0] + s_geoEdit.drawHeight[1]) * 0.5f;
						if (s_geoEdit.extrudeEnabled)
						{
							height = s_curVtxPos.y;
						}

						Vec3f pos = { s_curVtxPos.x, height, s_curVtxPos.z };

						Vec2f screenPos;
						if (worldPosToViewportCoord(pos, &screenPos))
						{
							char info[256];
							floatToString(s_geoEdit.drawHeight[1] - s_geoEdit.drawHeight[0], info);
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
							mapPos.x -= s32(textOffset + 12);
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

				// Level Notes
				if (!(s_editorConfig.interfaceFlags & PIF_HIDE_NOTES))
				{
					const s32 count = (s32)s_level.notes.size();
					LevelNote* note = s_level.notes.data();
					for (s32 i = 0; i < count; i++, note++)
					{
						Group* group = levelNote_getGroup(note);
						if (group->flags & GRP_HIDDEN) { continue; }
						if ((note->flags & LNF_2D_ONLY) && s_view != EDIT_VIEW_2D) { continue; }
						if (!(note->flags & LNF_TEXT_ALWAYS_SHOW) && s_hoveredLevelNote != i) { continue; }

						Vec2i mapPos;
						bool showInfo = true;
						f32 alpha = 1.0f;
						if (s_view == EDIT_VIEW_2D)
						{
							Vec2f center = { note->pos.x + c_levelNoteRadius2d * 1.25f, note->pos.z };

							mapPos = worldPos2dToMap(center);
							mapPos.z -= 16;
						}
						else if (s_view == EDIT_VIEW_3D)
						{
							Vec3f cameraRgt = { s_camera.viewMtx.m0.x,  s_camera.viewMtx.m0.y, s_camera.viewMtx.m0.z };
							Vec3f center = { note->pos.x + cameraRgt.x * c_levelNoteRadius3d * 1.25f, note->pos.y, note->pos.z + cameraRgt.z * c_levelNoteRadius3d * 1.25f };
							Vec2f screenPos;
							if (worldPosToViewportCoord(center, &screenPos))
							{
								mapPos = { s32(screenPos.x), s32(screenPos.z) };
							}
							else
							{
								showInfo = false;
							}

							if (!(note->flags & LNF_3D_NO_FADE))
							{
								Vec3f offset = { note->pos.x - s_camera.pos.x, note->pos.y - s_camera.pos.y, note->pos.z - s_camera.pos.z };
								f32 distSq = offset.x*offset.x + offset.y*offset.y + offset.z*offset.z;
								if (distSq > note->fade.z*note->fade.z)
								{
									continue;
								}
								alpha = clamp(1.0f - (sqrt(distSq) - note->fade.x) / (note->fade.z - note->fade.x), 0.0f, 1.0f);
							}
						}
						if (showInfo)
						{
							drawViewportInfo(i, mapPos, note->note.c_str(), 0, 0, alpha, note->textColor);
						}
					}
				}

				// Display Grid Info
				if (s_view == EDIT_VIEW_2D && (s_editFlags & LEF_SHOW_GRID) && !isUiModal() && !isViewportElementHovered())
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

	#if EDITOR_TEXT_PROMPTS == 1
		handleTextPrompts();
	#endif

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
				if (TFE_Polygon::vtxEqual(rootVtx, &sector->vtx[wall->idx[0]]))
				{
					idx = 0;
				}
				else if (TFE_Polygon::vtxEqual(rootVtx, &sector->vtx[wall->idx[1]]))
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

	// TODO: This won't be necessary once derived selections work (derive vertex selection from walls).
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
				EditorWall* mirror = wall->mirrorId < (s32)nextSector->walls.size() ? &nextSector->walls[wall->mirrorId] : nullptr;

				if (mirror)
				{
					FeatureId v2 = createFeatureId(nextSector, mirror->idx[0]);
					FeatureId v3 = createFeatureId(nextSector, mirror->idx[1]);
					vtxSelection_add(v2);
					vtxSelection_add(v3);
				}
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

			if (s_featureCur.sector == sector && s_featureCur.featureIndex == featureIndex && s_featureCur.part == (HitPart)part)
			{
				s_featureCur = {};
			}
		}
		else
		{
			selection_add(id);
			s_featureCur = {};
			handleSelectMode(s_featureHovered.sector, s_featureHovered.featureIndex);
			s_featureCur.featureIndex = featureIndex;
			s_featureCur.sector = sector;
			s_featureCur.part = (HitPart)part;
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

				if (TFE_Polygon::vtxEqual(vtx, &featureSector->vtx[_featureIndex]))
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
		worldPos2d = snapToGridOrGuidelines2d(worldPos2d);

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
			
			Vec3f rail[] = { worldPos, { worldPos.x + 1.0f, worldPos.y, worldPos.z }, { worldPos.x, worldPos.y, worldPos.z + 1.0f },
						   { worldPos.x - 1.0f, worldPos.y, worldPos.z }, { worldPos.x, worldPos.y, worldPos.z - 1.0f } };

			Vec3f moveDir = { worldPos.x - s_prevPos.x, worldPos.y - s_prevPos.y, worldPos.z - s_prevPos.z };
			if (moveDir.x*moveDir.x + moveDir.y*moveDir.y + moveDir.z*moveDir.z > 0.0001f)
			{
				moveDir = TFE_Math::normalize(&moveDir);
				viewport_setRail(rail, 4, &moveDir);
			}
			else
			{
				viewport_setRail(rail, 4);
			}

			s_prevPos = worldPos;
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
		Vec3f delta = { 0 };
		if (TFE_Math::closestPointBetweenLines(&rail[0], &rail[1], &cameraRay[0], &cameraRay[1], &railInt, &cameraInt))
		{
			worldPos = { rail[0].x + railInt * (rail[1].x - rail[0].x), rail[0].y + railInt * (rail[1].y - rail[0].y), rail[0].z + railInt * (rail[1].z - rail[0].z) };
			delta = { worldPos.x - s_prevPos.x, worldPos.y - s_prevPos.y, worldPos.z - s_prevPos.z };
			s_prevPos = worldPos;
		}
		else  // If the closest point on lines is unresolvable (parallel lines, etc.) - then just don't move.
		{
			worldPos = s_prevPos;
		}

		f32 visCurY = fabsf(dir.y) > 0.75f ? s_cursor3d.y : s_curVtxPos.y;
		Vec3f railVis[] =
		{
			{ s_curVtxPos.x, visCurY, s_curVtxPos.z },
			{ s_curVtxPos.x + dir.x, visCurY + dir.y, s_curVtxPos.z + dir.z },
			{ s_curVtxPos.x - dir.x, visCurY - dir.y, s_curVtxPos.z - dir.z },
		};
		if (delta.x*delta.x + delta.y*delta.y + delta.z*delta.z > 0.0001f)
		{
			delta = TFE_Math::normalize(&delta);
			viewport_setRail(railVis, 2, &delta);
		}
		else
		{
			viewport_setRail(railVis, 2);
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

		snapToGridY(&y);
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
			if (s_wallMoveMode == WMM_NORMAL)
			{
				worldPos = moveAlongRail({ s_wallNrm.x, 0.0f, s_wallNrm.z });
			}
			else
			{
				worldPos = moveAlongXZPlane(s_curVtxPos.y);
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
				f32 v0Offset = snapAlongPath(s_moveStartPos, moveDir, s_moveStartPos, nrmOffset);
				f32 v1Offset = snapAlongPath(s_moveStartPos1, moveDir, s_moveStartPos, nrmOffset);

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
		if (!TFE_Input::keyModDown(KEYMOD_CTRL))
		{
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
		if (dy != 0 && !TFE_Input::keyModDown(KEYMOD_CTRL))
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

		if (!TFE_Input::keyModDown(KEYMOD_CTRL))
		{
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

	bool drawToolbarButton(void* ptr, u32 imageId, bool highlight, const char* tooltip)
	{
		const f32 imageHeightScale = 1.0f / 192.0f;
		const f32 y0 = f32(imageId) * 32.0f;
		const f32 y1 = y0 + 32.0f;

		ImGui::PushID(imageId);
		bool res = ImGui::ImageButton(ptr, ImVec2(32, 32), ImVec2(0.0f, y0 * imageHeightScale), ImVec2(1.0f, y1 * imageHeightScale), 1, ImVec4(0, 0, 0, highlight ? 0.75f : 0.25f), ImVec4(1, 1, 1, 1));
		setTooltip(tooltip);
		ImGui::PopID();

		return res;
	}

	bool drawToolbarBooleanButton(void* ptr, u32 imageId, bool highlight, const char* tooltip)
	{
		const f32 imageHeightScale = 1.0f / 96.0f;
		const f32 y0 = f32(imageId) * 32.0f;
		const f32 y1 = y0 + 32.0f;

		ImGui::PushID(imageId);
		bool res = ImGui::ImageButton(ptr, ImVec2(32, 32), ImVec2(0.0f, y0 * imageHeightScale), ImVec2(1.0f, y1 * imageHeightScale), 1, ImVec4(0, 0, 0, highlight ? 0.75f : 0.25f), ImVec4(1, 1, 1, 1));
		setTooltip(tooltip);
		ImGui::PopID();

		return res;
	}

	void levelEditWinBegin()
	{
		bool gridActive = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);
		s_editWinSize = { (s32)displayInfo.width - (s32)UI_SCALE(480), (s32)displayInfo.height - (s32)UI_SCALE(68) - s_outputHeight };

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

	Vec4f viewportBoundsWS2d(f32 padding)
	{
		Vec4f bounds = { 0 };
		bounds.x =  s_viewportPos.x - padding;
		bounds.w = -s_viewportPos.z + padding;
		bounds.z =  s_viewportPos.x + s_viewportSize.x * s_zoom2d + padding;
		bounds.y =-(s_viewportPos.z + s_viewportSize.z * s_zoom2d) - padding;
		return bounds;
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
		ImGui::BeginChild("MsgPanel", { 280.0f, 20.0f }, 0, window_flags);

		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		if (!isUiModal() && mx >= s_editWinPos.x && mx < s_editWinPos.x + s_editWinSize.x && my >= s_editWinPos.z && my < s_editWinPos.z + s_editWinSize.z)
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

	void moveFeatures(const Vec3f& newPos)
	{
		if (s_editMode == LEDIT_VERTEX)
		{
			if (s_featureCur.featureIndex >= 0 && s_featureCur.sector && TFE_Input::mouseDown(MBUTTON_LEFT) && !TFE_Input::keyModDown(KEYMOD_CTRL) && !TFE_Input::keyModDown(KEYMOD_SHIFT))
			{
				if (s_canMoveFeature)
				{
					moveVertex({ newPos.x, newPos.z });
				}
			}
			else if (s_featureCur.featureIndex >= 0 && s_featureCur.sector && s_moveStarted)
			{
				s_editMove = false;
				s_moveStarted = false;
				if (s_canMoveFeature)
				{
					cmd_addMoveVertices((s32)s_selectionList.size(), s_selectionList.data(), s_moveTotalDelta);
				}
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
				if (s_canMoveFeature)
				{
					if (s_featureCur.part == HP_FLOOR || s_featureCur.part == HP_CEIL) { moveFlat(); }
					else { moveWall(newPos); }
				}
			}
			else if (s_featureCur.featureIndex >= 0 && s_featureCur.sector && s_moveStarted)
			{
				s_editMove = false;
				s_moveStarted = false;

				if (s_canMoveFeature)
				{
					if (s_featureCur.part == HP_FLOOR || s_featureCur.part == HP_CEIL)
					{
						cmd_addMoveFlats((s32)s_selectionList.size(), s_selectionList.data(), s_moveTotalDelta.x);
					}
					else // Re-use the move vertices command, but with the name LName_MoveWall.
					{
						cmd_addMoveVertices((s32)s_vertexList.size(), s_vertexList.data(), s_moveTotalDelta, LName_MoveWall);
					}
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
		s_featureTex = {};
		clearLevelNoteSelection();
		guideline_clearSelection();
	}

	static Vec2f s_texDelta = { 0 };

	void edit_moveTexture(s32 count, const FeatureId* featureList, Vec2f delta)
	{
		for (s32 i = 0; i < count; i++)
		{
			s32 featureIndex;
			HitPart part;
			EditorSector* sector = unpackFeatureId(featureList[i], &featureIndex, (s32*)&part);
			modifyTextureOffset(sector, part, featureIndex, delta);
		}
	}

	void handleTextureAlignment()
	{
		// TODO: Remove code duplication between the 2D and 3D versions.
		if (s_view == EDIT_VIEW_2D && s_editMode == LEDIT_SECTOR && s_featureHovered.sector && (s_sectorDrawMode == SDM_TEXTURED_CEIL || s_sectorDrawMode == SDM_TEXTURED_FLOOR))
		{
			HitPart part = s_sectorDrawMode == SDM_TEXTURED_CEIL ? HP_CEIL : HP_FLOOR;
			s_featureHovered.part = part;
			if (s_featureTex.featureIndex >= 0)
			{
				s_featureHovered = s_featureTex;
			}

			FeatureId id = createFeatureId(s_featureHovered.sector, s_featureHovered.featureIndex, s_featureHovered.part);
			bool doesItemExist = selection_doesFeatureExist(id);
			if (s_selectionList.size() <= 1 || doesItemExist)
			{
				Vec2f delta = { 0 };

				// Cannot move using the arrow keys and the mouse at the same time.
				if (!s_startTexMove)
				{
					const f32 move = TFE_Input::keyModDown(KEYMOD_SHIFT) ? 1.0f : 1.0f / 8.0f;

					if (TFE_Input::keyPressedWithRepeat(KEY_RIGHT)) { delta.x += move; }
					else if (TFE_Input::keyPressedWithRepeat(KEY_LEFT)) { delta.x -= move; }

					if (TFE_Input::keyPressedWithRepeat(KEY_UP)) { delta.z += move; }
					else if (TFE_Input::keyPressedWithRepeat(KEY_DOWN)) { delta.z -= move; }

					if (delta.x || delta.z)
					{
						// Add command.
						const s32 count = (s32)s_selectionList.size();
						if (!count)
						{
							cmd_addMoveTexture(1, &id, delta);
						}
						else
						{
							cmd_addMoveTexture(count, s_selectionList.data(), delta);
						}
					}
				}

				EditorSector* sector = s_featureHovered.sector;
				Vec2f cursor = { s_cursor3d.x, s_cursor3d.z };

				if (!s_startTexMove)
				{
					snapToGrid(&cursor);
				}

				bool middleMousePressed = TFE_Input::mousePressed(MBUTTON_MIDDLE);
				bool middleMouseDown = TFE_Input::mouseDown(MBUTTON_MIDDLE);
				if (middleMousePressed && !s_startTexMove)
				{
					s_startTexMove = true;
					s_startTexPos = { cursor.x, 0.0f, cursor.z };
					s_startTexOffset = getTextureOffset(s_featureHovered.sector, part, s_featureHovered.featureIndex);
					snapToGrid(&s_startTexOffset);
					s_featureTex = s_featureHovered;
					// Cancel out any key-based movement, cannot do both at once.
					delta = { 0 };
					s_texDelta = { 0 };
				}
				else if (middleMouseDown && s_startTexMove)
				{
					Vec2f offset = { 0 };
					const f32 texScale = 1.0f;
					
					snapToGrid(&cursor);
					offset = { cursor.x - s_startTexPos.x, cursor.z - s_startTexPos.z };

					delta.x = offset.x * texScale;
					delta.z = offset.z * texScale;
					s_texDelta = delta;

					Vec2f curOffset = getTextureOffset(s_featureHovered.sector, s_featureHovered.part, s_featureHovered.featureIndex);
					delta.x = (s_startTexOffset.x + delta.x) - curOffset.x;
					delta.z = (s_startTexOffset.z + delta.z) - curOffset.z;
				}
				else if (!middleMousePressed && !middleMouseDown)
				{
					// Add command.
					const s32 count = (s32)s_selectionList.size();
					if (!count)
					{
						cmd_addMoveTexture(1, &id, s_texDelta);
					}
					else
					{
						cmd_addMoveTexture(count, s_selectionList.data(), s_texDelta);
					}

					s_startTexMove = false;
					s_featureTex = {};
				}

				if (delta.x || delta.z)
				{
					const s32 count = (s32)s_selectionList.size();
					if (!count) { edit_moveTexture(1, &id, delta); }
					else { edit_moveTexture(count, s_selectionList.data(), delta); }
				} // delta.x || delta.z
			}
		}
		else if (s_view == EDIT_VIEW_3D && s_editMode == LEDIT_WALL && (s_featureHovered.featureIndex >= 0 || s_featureTex.featureIndex >= 0))
		{
			if (s_featureTex.featureIndex >= 0)
			{
				s_featureHovered = s_featureTex;
			}

			FeatureId id = createFeatureId(s_featureHovered.sector, s_featureHovered.featureIndex, s_featureHovered.part);
			bool doesItemExist = selection_doesFeatureExist(id);
			if (s_selectionList.size() <= 1 || doesItemExist)
			{
				Vec2f delta = { 0 };

				if (!s_startTexMove)
				{
					f32 move = TFE_Input::keyModDown(KEYMOD_SHIFT) ? 1.0f : 1.0f / 8.0f;
					if (TFE_Input::keyPressedWithRepeat(KEY_RIGHT)) { delta.x -= move; }
					else if (TFE_Input::keyPressedWithRepeat(KEY_LEFT)) { delta.x += move; }
					if (TFE_Input::keyPressedWithRepeat(KEY_UP)) { delta.z -= move; }
					else if (TFE_Input::keyPressedWithRepeat(KEY_DOWN)) { delta.z += move; }

					if (delta.x || delta.z)
					{
						if (s_featureHovered.part == HP_SIGN)
						{
							delta.x = -delta.x;
						}

						// Add command.
						const s32 count = (s32)s_selectionList.size();
						if (!count)
						{
							cmd_addMoveTexture(1, &id, delta);
						}
						else
						{
							cmd_addMoveTexture(count, s_selectionList.data(), delta);
						}
					}
				}

				EditorSector* sector = s_featureHovered.sector;
				HitPart part = s_featureHovered.part;
				Vec3f cursor = s_cursor3d;

				if (!s_startTexMove)
				{
					if (part == HP_FLOOR || part == HP_CEIL)
					{
						snapToGrid(&cursor);
					}
					// Snap to the surface grid.
					else
					{
						EditorWall* wall = &sector->walls[s_featureHovered.featureIndex];
						snapToSurfaceGrid(sector, wall, cursor);
					}
				}
										
				bool middleMousePressed = TFE_Input::mousePressed(MBUTTON_MIDDLE);
				bool middleMouseDown = TFE_Input::mouseDown(MBUTTON_MIDDLE);
				if (middleMousePressed && !s_startTexMove)
				{
					s_startTexMove = true;
					s_startTexPos = cursor;
					s_startTexOffset = getTextureOffset(s_featureHovered.sector, s_featureHovered.part, s_featureHovered.featureIndex);
					s_texMoveSign = s_featureHovered.part == HP_SIGN;
					if (s_texMoveSign)
					{
						// TODO: Revisit sign snapping.
						// This will re-center, but that isn't really what we want...
						//EditorWall* wall = &sector->walls[s_featureHovered.featureIndex];
						//snapSignToCursor(s_featureHovered.sector, wall, wall->tex[HP_SIGN].texIndex, &s_startTexOffset);

						s_startTexOffset.x = -s_startTexOffset.x;
					}
					else
					{
						snapToGrid(&s_startTexOffset);
					}
					s_featureTex = s_featureHovered;

					if (part != HP_FLOOR && part != HP_CEIL)
					{
						EditorWall* wall = &sector->walls[s_featureHovered.featureIndex];
						const Vec2f* v0 = &sector->vtx[wall->idx[0]];
						const Vec2f* v1 = &sector->vtx[wall->idx[1]];
						const Vec2f t = { -(v1->x - v0->x), -(v1->z - v0->z) };
						const Vec2f n = { -(v1->z - v0->z), v1->x - v0->x };

						s_texTangent = TFE_Math::normalize(&t);
						s_texNormal = TFE_Math::normalize(&n);
					}

					// Cancel out any key-based movement, cannot do both at once.
					delta = { 0 };
					s_texDelta = { 0 };
				}
				else if (middleMouseDown && s_startTexMove)
				{
					Vec3f offset = { 0 };
					const f32 texScale = 1.0f;
					if (part == HP_FLOOR || part == HP_CEIL)
					{
						// Intersect the ray from the camera, through the plane.
						f32 planeHeight = (part == HP_FLOOR) ? sector->floorHeight : sector->ceilHeight;
						f32 s = (planeHeight - s_camera.pos.y) / (cursor.y - s_camera.pos.y);
						cursor.x = s_camera.pos.x + s * (cursor.x - s_camera.pos.x);
						cursor.z = s_camera.pos.z + s * (cursor.z - s_camera.pos.z);

						snapToGrid(&cursor);
						offset = { cursor.x - s_startTexPos.x, cursor.y - s_startTexPos.y, cursor.z - s_startTexPos.z };

						delta.x = offset.x * texScale;
						delta.z = offset.z * texScale;
						s_texDelta = delta;

						Vec2f curOffset = getTextureOffset(s_featureHovered.sector, s_featureHovered.part, s_featureHovered.featureIndex);
						delta.x = (s_startTexOffset.x + delta.x) - curOffset.x;
						delta.z = (s_startTexOffset.z + delta.z) - curOffset.z;
					}
					else
					{
						// Intersect the ray with the plane.
						const Vec2f off0 = { s_camera.pos.x - s_startTexPos.x, s_camera.pos.z - s_startTexPos.z };
						const Vec2f off1 = { cursor.x - s_startTexPos.x, cursor.z - s_startTexPos.z };
						const f32 d0 = TFE_Math::dot(&off0, &s_texNormal);
						const f32 d1 = TFE_Math::dot(&off1, &s_texNormal);
						const f32 t = -d0 / (d1 - d0);
						cursor.x = s_camera.pos.x + t * (cursor.x - s_camera.pos.x);
						cursor.y = s_camera.pos.y + t * (cursor.y - s_camera.pos.y);
						cursor.z = s_camera.pos.z + t * (cursor.z - s_camera.pos.z);

						EditorWall* wall = &sector->walls[s_featureHovered.featureIndex];
						snapToSurfaceGrid(sector, wall, cursor);
						offset = { cursor.x - s_startTexPos.x, cursor.y - s_startTexPos.y, cursor.z - s_startTexPos.z };

						delta.x = (offset.x * s_texTangent.x + offset.z * s_texTangent.z) * texScale;
						delta.z = -offset.y * texScale;
						s_texDelta = delta;

						Vec2f curOffset = getTextureOffset(s_featureHovered.sector, s_featureHovered.part, s_featureHovered.featureIndex);
						if (part == HP_SIGN)
						{
							delta.x = -(s_startTexOffset.x + delta.x) - curOffset.x;
						}
						else
						{
							delta.x = (s_startTexOffset.x + delta.x) - curOffset.x;
						}
						delta.z = (s_startTexOffset.z + delta.z) - curOffset.z;
					}
				}
				else if (!middleMousePressed && !middleMouseDown)
				{
					// Add command.
					if (s_startTexMove)
					{
						const s32 count = (s32)s_selectionList.size();
						if (!count)
						{
							cmd_addMoveTexture(1, &id, s_texDelta);
						}
						else
						{
							cmd_addMoveTexture(count, s_selectionList.data(), s_texDelta);
						}
					}

					s_startTexMove = false;
					s_featureTex = {};
				}

				if (delta.x || delta.z)
				{
					const s32 count = (s32)s_selectionList.size();
					if (!count) { edit_moveTexture(1, &id, delta); }
					else { edit_moveTexture(count, s_selectionList.data(), delta); }
				} // delta.x || delta.z
			}
		}
		else
		{
			if (s_startTexMove)
			{
				// command...
				s_startTexMove = false;
			}
			s_featureTex = {};
		}
	}

	void applyTextureToItem(EditorSector* sector, HitPart part, s32 index, s32 texIndex, Vec2f* offset)
	{
		if (s_view == EDIT_VIEW_2D)
		{
			if (s_sectorDrawMode == SDM_TEXTURED_FLOOR)
			{
				sector->floorTex.texIndex = texIndex;
				if (offset) { sector->floorTex.offset = *offset; }
			}
			else if (s_sectorDrawMode == SDM_TEXTURED_CEIL)
			{
				sector->ceilTex.texIndex = texIndex;
				if (offset) { sector->ceilTex.offset = *offset; }
			}
		}
		else
		{
			if (part == HP_FLOOR) { sector->floorTex.texIndex = texIndex; if (offset) { sector->floorTex.offset = *offset; } }
			else if (part == HP_CEIL) { sector->ceilTex.texIndex = texIndex; if (offset) { sector->ceilTex.offset = *offset; } }
			else
			{
				EditorWall* wall = &sector->walls[index];
				if (part == HP_MID) { wall->tex[WP_MID].texIndex = texIndex; if (offset) { wall->tex[WP_MID].offset = *offset; } }
				else if (part == HP_TOP) { wall->tex[HP_TOP].texIndex = texIndex;  if (offset) { wall->tex[HP_TOP].offset = *offset; } }
				else if (part == HP_BOT) { wall->tex[HP_BOT].texIndex = texIndex;  if (offset) { wall->tex[HP_BOT].offset = *offset; } }
				else if (part == HP_SIGN) { wall->tex[HP_SIGN].texIndex = texIndex;  if (offset) { wall->tex[HP_SIGN].offset = *offset; } }
			}
		}
	}

	void edit_clearTexture(s32 count, const FeatureId* feature)
	{
		for (s32 i = 0; i < count; i++)
		{
			HitPart part;
			s32 index;
			EditorSector* sector = unpackFeatureId(feature[i], &index, (s32*)&part);
			applyTextureToItem(sector, part, index, -1, nullptr);
		}
	}

	void edit_setTexture(s32 count, const FeatureId* feature, s32 texIndex, Vec2f* offset)
	{
		texIndex = getTextureIndex(s_levelTextureList[texIndex].name.c_str());
		if (texIndex < 0) { return; }

		for (s32 i = 0; i < count; i++)
		{
			HitPart part;
			s32 index;
			EditorSector* sector = unpackFeatureId(feature[i], &index, (s32*)&part);
			applyTextureToItem(sector, part, index, texIndex, offset);
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

	void applySignToSelection(s32 signIndex)
	{
		s32 texIndex = getTextureIndex(s_levelTextureList[signIndex].name.c_str());
		if (texIndex < 0) { return; }
		EditorTexture* signTex = (EditorTexture*)getAssetData(s_level.textures[texIndex].handle);
		if (!signTex) { return; }

		// For now, this only works for hovered features.
		FeatureId id = createFeatureId(s_featureHovered.sector, s_featureHovered.featureIndex, WP_SIGN);
		
		// Center the sign on the mouse cursor.
		EditorSector* sector = s_featureHovered.sector;
		EditorWall* wall = &sector->walls[s_featureHovered.featureIndex];

		Vec2f offset = { 0 };
		snapSignToCursor(sector, wall, texIndex, &offset);

		edit_setTexture(1, &id, signIndex, &offset);
		cmd_addSetTexture(1, &id, signIndex, &offset);
	}

	void applyTextureToSelection(s32 texIndex, Vec2f* offset)
	{
		FeatureId id = createFeatureId(s_featureHovered.sector, s_featureHovered.featureIndex, s_featureHovered.part);
		bool doesItemExist = selection_doesFeatureExist(id);
		const s32 count = (s32)s_selectionList.size();
		if (count <= 1 || doesItemExist)
		{
			s32 index = s_featureHovered.featureIndex;
			if (doesItemExist)
			{
				edit_setTexture(count, s_selectionList.data(), texIndex, offset);
				cmd_addSetTexture(count, s_selectionList.data(), texIndex, offset);
			}
			else
			{
				edit_setTexture(1, &id, texIndex, offset);
				cmd_addSetTexture(1, &id, texIndex, offset);
			}
		}
	}

	void applySurfaceTextures()
	{
		EditorSector* sector = s_featureHovered.sector;
		HitPart part = s_featureHovered.part;
		s32 index = s_featureHovered.featureIndex;
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
			applySignToSelection(s_selectedTexture);
		}
		else if (TFE_Input::keyPressed(KEY_T) && s_selectedTexture >= 0)
		{
			applyTextureToSelection(s_selectedTexture, nullptr);
		}
		else if (TFE_Input::keyPressed(KEY_V) && TFE_Input::keyModDown(KEYMOD_CTRL) && s_selectedTexture >= 0)
		{
			applyTextureToSelection(s_selectedTexture, &s_copiedTextureOffset);
		}
		else if (TFE_Input::keyPressed(KEY_A) && TFE_Input::keyModDown(KEYMOD_CTRL))
		{
			edit_autoAlign(s_featureHovered.sector->id, s_featureHovered.featureIndex, s_featureHovered.part);
			cmd_addAutoAlign(s_featureHovered.sector->id, s_featureHovered.featureIndex, s_featureHovered.part);
		}
	}

	void play()
	{
		StartPoint start = {};
		start.pos = s_camera.pos;
		start.yaw = s_camera.yaw;
		start.pitch = s_camera.pitch;
		start.sector = findSector3d(start.pos, s_curLayer);
		if (!start.sector)
		{
			LE_ERROR("Cannot test level, camera must be inside of a sector.");
			return;
		}

		char exportPath[TFE_MAX_PATH];
		getTempDirectory(exportPath);
		exportLevel(exportPath, s_level.slot.c_str(), &start);
	}
	
	void copyToClipboard(const char* str)
	{
		SDL_SetClipboardText(str);
	}

	bool copyFromClipboard(char* str)
	{
		bool hasText = SDL_HasClipboardText();
		if (hasText)
		{
			char* text = SDL_GetClipboardText();
			hasText = false;
			if (text && text[0])
			{
				strcpy(str, text);
				hasText = true;
			}
			SDL_free(text);
		}
		return hasText;
	}

	s32 getDefaultTextureIndex(WallPart part)
	{
		s32 index = getTextureIndex("DEFAULT.BM");
		const s32 count = (s32)s_level.sectors.size();
		const EditorSector* sector = s_level.sectors.data();
		for (s32 i = 0; i < count; i++, sector++)
		{
			if (!sector->walls.empty())
			{
				const s32 newIndex = sector->walls[0].tex[part].texIndex;
				if (newIndex >= 0)
				{
					index = newIndex;
					break;
				}
			}
		}
		return index;
	}

	void getGridOrientedRect(const Vec2f p0, const Vec2f p1, Vec2f* rect)
	{
		const Vec2f g[] = { posToGrid(p0), posToGrid(p1) };
		const Vec2f gridCorner[] =
		{
			{ g[0].x, g[0].z },
			{ g[1].x, g[0].z },
			{ g[1].x, g[1].z },
			{ g[0].x, g[1].z }
		};
		rect[0] = gridToPos(gridCorner[0]);
		rect[1] = gridToPos(gridCorner[1]);
		rect[2] = gridToPos(gridCorner[2]);
		rect[3] = gridToPos(gridCorner[3]);
	}

	///////////////////////////////////////////////
	// Viewport scrolling
	///////////////////////////////////////////////
	enum ViewportScrollMode
	{
		VSCROLL_2D = 0,
		VSCROLL_3D,
	};

	const f32 c_scrollEps = 0.001f;
	const f32 c_scrollMinScpeed = 128.0f;
	const f32 c_scrollAngularSpd = 3.0f;
	
	static ViewportScrollMode s_viewScrollMode;
	static bool s_scrollView = false;
	static Vec2f s_scrollSrc;
	static Vec2f s_scrollTarget;
	static Vec2f s_scrollDelta;

	// 3d/camera
	static Vec3f s_scrollSrc3d;
	static Vec3f s_scrollTarget3d;
	static Vec3f s_scrollDelta3d;
	static Vec2f s_scrollSrcAngles;
	static Vec2f s_scrollDstAngles;

	static f32 s_scrollLen;
	static f32 s_scrollPos;
	static f32 s_scrollAngle;
	static f32 s_scrollSpeed;

	void setViewportScrollTarget2d(Vec2f target, f32 speed)
	{
		// Don't bother if already there.
		Vec2f delta = { target.x - s_viewportPos.x, target.z - s_viewportPos.z };
		if (fabsf(delta.x) < c_scrollEps && fabsf(delta.z) < c_scrollEps)
		{
			return;
		}
		// Setup the scroll.
		s_scrollTarget = target;
		s_scrollSrc = { s_viewportPos.x, s_viewportPos.z };
		s_scrollLen = sqrtf(delta.x*delta.x + delta.z*delta.z);
		s_scrollDelta = delta;
		s_scrollPos = 0.0f;
		s_scrollSpeed = speed == 0.0f ? max(c_scrollMinScpeed, s_scrollLen) : speed;
		s_scrollView = true;
		s_viewScrollMode = VSCROLL_2D;
	}

	void setViewportScrollTarget3d(Vec3f target, f32 targetYaw, f32 targetPitch, f32 speed)
	{
		// Don't bother if already there.
		const Vec3f delta = { target.x - s_camera.pos.x, target.y - s_camera.pos.y, target.z - s_camera.pos.z };
		const Vec2f deltaAng = { targetYaw - s_camera.yaw, targetPitch - s_camera.pitch };
		if (fabsf(delta.x) < c_scrollEps && fabsf(delta.y) < c_scrollEps && fabsf(delta.z) < c_scrollEps &&
			fabsf(deltaAng.x) < c_scrollEps && fabsf(deltaAng.z) < c_scrollEps)
		{
			return;
		}
		// Setup the scroll.
		s_scrollTarget3d = target;
		s_scrollSrc3d = s_camera.pos;
		s_scrollLen = sqrtf(delta.x*delta.x + delta.y*delta.y + delta.z*delta.z);
		s_scrollDelta3d = delta;
		s_scrollPos = 0.0f;
		s_scrollSpeed = speed == 0.0f ? max(c_scrollMinScpeed, s_scrollLen) : speed;

		s_scrollSrcAngles = { fmodf(s_camera.yaw + 2.0f*PI, 2.0f*PI), s_camera.pitch };
		s_scrollDstAngles = { targetYaw, targetPitch };
		s_scrollAngle = 0.0f;

		// Shortest angle.
		if (s_scrollDstAngles.x - s_scrollSrcAngles.x > PI)
		{
			s_scrollDstAngles.x -= 2.0f*PI;
		}
		else if (s_scrollDstAngles.x - s_scrollSrcAngles.x < -PI)
		{
			s_scrollDstAngles.x += 2.0f*PI;
		}
		
		s_scrollView = true;
		s_viewScrollMode = VSCROLL_3D;
	}

	void updateViewportScroll()
	{
		if (!s_scrollView) { return; }

		if (s_viewScrollMode == VSCROLL_2D)
		{
			s_scrollPos += s_scrollSpeed * (f32)TFE_System::getDeltaTime();
			if (s_scrollPos >= s_scrollLen)
			{
				s_viewportPos.x = s_scrollTarget.x;
				s_viewportPos.z = s_scrollTarget.z;
				s_scrollView = false;
			}
			else
			{
				const f32 scale = smoothstep(0.0f, s_scrollLen, s_scrollPos);
				s_viewportPos.x = s_scrollSrc.x + s_scrollDelta.x * scale;
				s_viewportPos.z = s_scrollSrc.z + s_scrollDelta.z * scale;
			}
		}
		else if (s_viewScrollMode == VSCROLL_3D)
		{
			// First the angles.
			f32 dt = (f32)TFE_System::getDeltaTime();
			if (s_scrollAngle < 1.0f)
			{
				s_scrollAngle += c_scrollAngularSpd * dt;
				if (s_scrollAngle >= 1.0f)
				{
					s_scrollAngle = 1.0f;
					s_camera.yaw = s_scrollDstAngles.x;
					s_camera.pitch = s_scrollDstAngles.z;
				}
				else
				{
					const f32 blend = smoothstep(0.0f, 1.0f, s_scrollAngle);
					s_camera.yaw   = s_scrollSrcAngles.x * (1.0f - blend) + s_scrollDstAngles.x * blend;
					s_camera.pitch = s_scrollSrcAngles.z * (1.0f - blend) + s_scrollDstAngles.z * blend;
				}
			}
			else
			{
				s_scrollPos += s_scrollSpeed * dt;
				if (s_scrollPos >= s_scrollLen)
				{
					s_camera.pos = s_scrollTarget3d;
					s_scrollView = false;
				}
				else
				{
					const f32 scale = smoothstep(0.0f, s_scrollLen, s_scrollPos);
					s_camera.pos.x = s_scrollSrc3d.x + s_scrollDelta3d.x * scale;
					s_camera.pos.y = s_scrollSrc3d.y + s_scrollDelta3d.y * scale;
					s_camera.pos.z = s_scrollSrc3d.z + s_scrollDelta3d.z * scale;
				}
			}
			computeCameraTransform();
		}
	}

	f32 smoothstep(f32 edge0, f32 edge1, f32 x)
	{
		x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
		return x * x * (3.0f - 2.0f*x);
	}

	/////////////////////////////////////////////////////
	// Tutorial/Update UI helpers.
	/////////////////////////////////////////////////////
#if EDITOR_TEXT_PROMPTS == 1
	const f32 c_textPromptDt[] =
	{
		0.5f,	// TXT_STATE_ANIM_ON
		10.0f,	// TXT_STATE_ON_SCREEN
		0.25f,	// TXT_STATE_ANIM_OFF
	};
	const f32 c_textPromptWidth = 1024.0f;
	const f32 c_textPromptHeight = 256.0f;
	const f32 c_textPromptY = 386.0f;

	enum TextPromptAnimState
	{
		TXT_STATE_ANIM_ON = 0,
		TXT_STATE_ON_SCREEN,
		TXT_STATE_ANIM_OFF,
		TXT_STATE_COUNT
	};

	struct TextPromptAnim
	{
		f32 curAlpha = 0.0f;
		f32 animParam = 0.0f;
		s32 promptId = -1;
		TextPromptAnimState state = TXT_STATE_ANIM_ON;
	};

	struct TextPrompt
	{
		std::string text;
		Vec4f color;
	};
	std::vector<TextPrompt> s_textPrompts;
	TextPromptAnim s_textPromptAnim = {};
	bool s_textPromptInit = false;
		
	s32 addTextPrompt(std::string text, Vec4f color)
	{
		s32 id = (s32)s_textPrompts.size();
		s_textPrompts.push_back({ text, color });
		return id;
	}

	void beginTextPrompt(s32 id)
	{
		s_textPromptAnim = {};
		s_textPromptAnim.promptId = id;
	}

	void updateTextPrompt()
	{
		if (s_textPromptAnim.promptId < 0) { return; }
		s_textPromptAnim.animParam += TFE_System::getDeltaTime();
		if (s_textPromptAnim.animParam >= c_textPromptDt[s_textPromptAnim.state])
		{
			s_textPromptAnim.animParam = 0.0f;
			s_textPromptAnim.state = TextPromptAnimState(s_textPromptAnim.state + 1);
			if (s_textPromptAnim.state == TXT_STATE_COUNT)
			{
				s_textPromptAnim.promptId = -1;
				return;
			}
		}

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		f32 y = displayInfo.height - c_textPromptY;
		if (s_textPromptAnim.state == TXT_STATE_ANIM_ON)
		{
			y = displayInfo.height - c_textPromptY * smoothstep(0.0f, 1.0f, s_textPromptAnim.animParam / c_textPromptDt[TXT_STATE_ANIM_ON]);
		}
		else if (s_textPromptAnim.state == TXT_STATE_ANIM_OFF)
		{
			y = displayInfo.height - c_textPromptY * smoothstep(0.0f, 1.0f, 1.0f - s_textPromptAnim.animParam / c_textPromptDt[TXT_STATE_ANIM_OFF]);
		}

		ImVec2 pos = { (s_viewportSize.x - c_textPromptWidth)*0.5f, y };

		u32 flags = ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs;
		ImGui::SetNextWindowSize({ c_textPromptWidth, c_textPromptHeight });
		ImGui::SetNextWindowPos(pos);
		pushFont(TFE_Editor::FONT_LARGE);
		ImGui::Begin("##Prompt", NULL, flags);

		ImVec2 size = ImGui::CalcTextSize(s_textPrompts[s_textPromptAnim.promptId].text.c_str());
		ImGui::SetCursorPosX((c_textPromptWidth  - size.x) * 0.5f);
		ImGui::SetCursorPosY((c_textPromptHeight - size.y) * 0.5f);

		const ImVec4 color = { s_textPrompts[s_textPromptAnim.promptId].color.x, s_textPrompts[s_textPromptAnim.promptId].color.y,
			s_textPrompts[s_textPromptAnim.promptId].color.z, s_textPrompts[s_textPromptAnim.promptId].color.w };
		ImGui::TextColored(color, s_textPrompts[s_textPromptAnim.promptId].text.c_str());

		ImGui::End();
		popFont();
	}

	// Initialize for tutorials, etc.
	// For now just hack int.
	void textPromptInit()
	{
		const char* textPrompts[] =
		{
			/*1*/ "Independent draw mode allows sectors\n"
				  "to be drawn that overlap with\n"
			      "existing sectors in 3 dimensions\n"
			      "without merging.",
			/*2*/ "Merge draw mode merges drawn shapes\n"
			      "with existing sectors, splitting them\n"
			      "as needed. But merging only occurs if\n"
			      "they overlap in X, Y and Z.",
			/*3*/ "Subtract draw mode removes regions of\n"
			      "sectors that overlap with the drawn\n"
			      "shape. This enables fast clipping and\n"
			      "speeds up drawing columns and voids.",
			/*4*/ "In the 3D view, you can draw on the\n"
			      "floor and extrude sub-sectors. You\n"
			      "can also draw outside the current\n"
			      "sector or cross sector boundaries.",
			/*5*/ "When drawing sectors in 3D, the first\n"
			      "vertex drawn determines which sector\n"
			      "to reference (if any).",
			/*6*/ "It is also possible to draw directly\n"
			      "on the walls and push or pull to\n"
			      "extrude new sectors.",
			/*7*/ "To help add more contrast to walls\n"
			      "you can apply directional lighting\n"
			      "that modifies the wall light offset\n"
			      "based on the wall normal.",
			/*8*/ "Groups act like editor-only layers\n"
			      "allowing groups to be hidden, locked\n"
			      "or removed from export.",
			/*9*/ "Each group has a name and color,\n"
			      "making them a great way to help\n"
				  "organize the level.",
			/*0*/ "",
		};
		for (s32 i = 0; i < TFE_ARRAYSIZE(textPrompts); i++)
		{
			addTextPrompt(textPrompts[i], {0.8f, 1.0f, 1.0f, 1.0f});
		}
	}

	void handleTextPrompts()
	{
		if (!s_textPromptInit)
		{
			textPromptInit();
			s_textPromptInit = true;
		}

		KeyboardCode key = TFE_Input::getKeyPressed();
		if (key >= KEY_1 && key <= KEY_0)
		{
			s32 index = key - KEY_1;
			s_textPromptAnim = {};
			s_textPromptAnim.promptId = index;
		}

		updateTextPrompt();
	}
#endif
}
