#include "levelEditor.h"
#include "Help/helpWindow.h"
#include "levelEditorData.h"
#include <DXL2_System/system.h>
#include <DXL2_System/math.h>
#include <DXL2_Renderer/renderer.h>
#include <DXL2_FileSystem/fileutil.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Archive/archive.h>
#include <DXL2_Input/input.h>
#include <DXL2_Game/geometry.h>
#include <DXL2_Asset/assetSystem.h>
#include <DXL2_Asset/imageAsset.h>
#include <DXL2_Asset/levelAsset.h>
#include <DXL2_Asset/infAsset.h>
#include <DXL2_Asset/paletteAsset.h>
#include <DXL2_Asset/colormapAsset.h>
#include <DXL2_Asset/textureAsset.h>
#include <DXL2_Asset/spriteAsset.h>
#include <DXL2_Asset/fontAsset.h>
#include <DXL2_Asset/modelAsset.h>
#include <DXL2_Asset/gameMessages.h>
#include <DXL2_Asset/levelList.h>
#include <DXL2_Asset/levelObjectsAsset.h>
#include <DXL2_Ui/markdown.h>
#include <DXL2_RenderBackend/renderBackend.h>

//Help
#include "help/helpWindow.h"

#include <DXL2_Editor/Rendering/editorRender.h>
//Rendering 2d
#include <DXL2_Editor/Rendering/lineDraw2d.h>
#include <DXL2_Editor/Rendering/grid2d.h>
#include <DXL2_Editor/Rendering/trianglesColor2d.h>
#include <DXL2_Editor/Rendering/trianglesTextured2d.h>
//Rendering 3d
#include <DXL2_Editor/Rendering/grid3d.h>
#include <DXL2_Editor/Rendering/lineDraw3d.h>
#include <DXL2_Editor/Rendering/trianglesColor3d.h>

// Triangulation
#include <DXL2_Polygon/polygon.h>

// UI
#include <DXL2_Ui/imGUI/imgui_file_browser.h>
#include <DXL2_Ui/imGUI/imgui.h>

// Game
#include <DXL2_Game/gameLoop.h>

#include <vector>
#include <algorithm>

// TODO: Add features to the file browser:
// 1. Filter does not exclude directories.
// 2. Ability to auto-set filter.
// 3. Ability to show only directories and select them.

using TrianglesColor3d::Tri3dTrans;

namespace LevelEditor
{
	#define TEXTURES_GOB_START_TEX 41
	
	enum EditorView
	{
		EDIT_VIEW_2D = 0,
		EDIT_VIEW_3D,
		EDIT_VIEW_3D_GAME,
		EDIT_VIEW_PLAY,
	};

	enum SectorDrawMode
	{
		SDM_WIREFRAME = 0,
		SDM_LIGHTING,
		SDM_TEXTURED_FLOOR,
		SDM_TEXTURED_CEIL,
		SDM_COUNT
	};

	enum LevelEditMode
	{
		LEDIT_DRAW = 1,
		LEDIT_VERTEX,
		LEDIT_WALL,
		LEDIT_SECTOR,
		LEDIT_ENTITY
	};

	enum BooleanMode
	{
		BOOL_SET = 0,
		BOOL_SUBTRACT,
		BOOL_INV_SUBTRACT,
		BOOL_ADD,
		BOOL_INTERSECT,
		BOOL_INV_INTERSECT,
		BOOL_COUNT
	};

	struct LevelFilePath
	{
		char gobName[64];
		char levelFilename[64];
		char levelName[64];
		char gobPath[DXL2_MAX_PATH];
	};

	static s32 s_gridIndex = 5;
	static f32 c_gridSizeMap[] =
	{
		1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 256.0f
	};
	static const char* c_gridSizes[] =
	{
		"1",
		"2",
		"4",
		"8",
		"16",
		"32",
		"64",
		"128",
		"256",
	};

	static u32 s_recentCount = 10;
	static LevelFilePath s_recentLevels[10] =
	{
		{"DARK.GOB", "SECBASE.LEV",  "Secret Base",       "DARK.GOB"},
		{"DARK.GOB", "TALAY.LEV",    "Talay: Tak Base",   "DARK.GOB"},
		{"DARK.GOB", "SEWERS.LEV",   "Anoat City",        "DARK.GOB"},
		{"DARK.GOB", "TESTBASE.LEV", "Research Facility", "DARK.GOB"},
		{"DARK.GOB", "GROMAS.LEV",   "Gromas Mines",      "DARK.GOB"},
		{"DARK.GOB", "DTENTION.LEV", "Detention Center",  "DARK.GOB"},
		{"DARK.GOB", "RAMSHED.LEV",  "Ramsees Hed",       "DARK.GOB"},
		{"DARK.GOB", "ROBOTICS.LEV", "Robotics Facility", "DARK.GOB"},
		{"DARK.GOB", "NARSHADA.LEV", "Nar Shaddaa",       "DARK.GOB"},
		{"DARK.GOB", "JABSHIP.LEV",  "Jabba's Ship",      "DARK.GOB"}
	};
	
	static EditorView s_editView = EDIT_VIEW_2D;
	static bool s_showLowerLayers = true;
	static bool s_enableInfEditor = false;
		
	static char s_gobFile[DXL2_MAX_PATH] = "";
	static char s_levelFile[DXL2_MAX_PATH] = "";
	static char s_levelName[64] = "";
	static bool s_loadLevel = false;
	static bool s_showSectorColors = true;
	static DXL2_Renderer* s_renderer;
	static void* s_editCtrlToolbarData = nullptr;
	static void* s_booleanToolbarData  = nullptr;
	static RenderTargetHandle s_view3d = nullptr;

	static EditorLevel* s_levelData = nullptr;

	static Vec2f s_editWinPos = { 0.0f, 67.0f };
	static Vec2f s_editWinSize;
	static Vec2f s_editWinMapCorner;

	static f32 s_gridSize = 32.0f;
	static f32 s_subGridSize = 32.0f;
	// 2D Camera
	static f32 s_zoom = 0.25f;
	static f32 s_zoomVisual = s_zoom;
	static f32 s_gridOpacity = 0.25f;
	static Vec2f s_offset = { 0 };
	static Vec2f s_offsetVis = { 0 };
	// 3D Camera
	struct Camera
	{
		Vec3f pos;
		Mat3  viewMtx;
		Mat4  projMtx;
		Mat4  invProj;

		f32 yaw = 0.0f;
		f32 pitch = 0.0f;
	};
	static Camera s_camera;

	static char s_layerMem[4 * 31];
	static char* s_layerStr[31];
	static const s32 s_layerMin = -15;
	static s32 s_layerIndex = 1 - s_layerMin;
	static s32 s_infoHeight = 300;
		
	// Sector Rendering
	static SectorDrawMode s_sectorDrawMode = SDM_WIREFRAME;
	static bool s_fullbright = false;

	static u32 s_editMode = LEDIT_DRAW;
	static u32 s_boolMode = BOOL_SET;
	static s32 s_selectedSector = -1;
	static s32 s_hoveredSector = -1;
	static s32 s_selectedWall = -1;
	static s32 s_hoveredWall = -1;
	static s32 s_selectedEntity = -1;
	static s32 s_hoveredEntity = -1;
	static s32 s_selectedEntitySector = -1;
	static s32 s_hoveredEntitySector = -1;
	static RayHitPart s_selectWallPart;
	static RayHitPart s_hoveredWallPart;
	static s32 s_selectedWallSector = -1;
	static s32 s_hoveredWallSector = -1;
	static s32 s_selectedVertex = -1;
	static s32 s_hoveredVertex = -1;
	static s32 s_selectedVertexSector = -1;
	static s32 s_hoveredVertexSector = -1;
	static RayHitPart s_selectVertexPart;
	static RayHitPart s_hoveredVertexPart;

	static Vec3f s_cursor3d = { 0 };
	static Vec3f s_rayDir = { 0 };
	static s32 s_hideFrames = 0;
	static const Palette256* s_palette;
	static bool s_runLevel = false;

	// Error message
	static bool s_showError = false;
	static char s_errorMessage[DXL2_MAX_PATH];

	void drawSectorPolygon(const EditorSector* sector, bool hover, u32 colorOverride = 0);
	void drawTexturedSectorPolygon(const EditorSector* sector, u32 color, bool floorTex);

	void loadLevel();
	void toolbarBegin();
	void toolbarEnd();
	void levelEditWinBegin();
	void levelEditWinEnd();
	void infoToolBegin(s32 height);
	void infoToolEnd();
	void browserBegin(s32 offset);
	void browserEnd();
	void messagePanel(ImVec2 pos);

	// Info panels
	void infoPanelMap();
	void infoPanelVertex();
	void infoPanelWall();
	void infoPanelInfWall(EditorSector* sector, u32 wallId);
	void infoPanelSector();
	void infoPanelEntity();
	// Browser panels
	void browseTextures();
	void browseEntities();

	// Draw
	void drawSector3d(const EditorSector* sector, const SectorTriangles* poly, bool overlay = false, bool hover = false);
	void drawSector3d_Lines(const EditorSector* sector, f32 width, u32 color, bool overlay = false, bool hover = false);
	void drawInfWalls3d(f32 width, u32 color);
	void drawWallColor(const EditorSector* sector, const Vec2f* vtx, const EditorWall* wall, const u32* color, bool blend = false, RayHitPart part = HIT_PART_UNKNOWN);

	// Error Handling
	bool isValidName(const char* name);
	void popupErrorMessage(const char* errorMessage);
	void showErrorPopup();

	void* loadGpuImage(const char* localPath)
	{
		char imagePath[DXL2_MAX_PATH];
		DXL2_Paths::appendPath(DXL2_PathType::PATH_PROGRAM, localPath, imagePath, DXL2_MAX_PATH);
		Image* image = DXL2_Image::get(imagePath);
		if (image)
		{
			TextureGpu* editCtrlHandle = DXL2_RenderBackend::createTexture(image->width, image->height, image->data);
			return DXL2_RenderBackend::getGpuPtr(editCtrlHandle);
		}
		return nullptr;
	}
		
	void init(DXL2_Renderer* renderer)
	{
		DXL2_EditorRender::init();
		
		s_renderer = renderer;
		s_renderer->enableScreenClear(true);
				
		// Load UI images.
		s_editCtrlToolbarData = loadGpuImage("UI_Images/EditCtrl_32x6.png");
		s_booleanToolbarData  = loadGpuImage("UI_Images/Boolean_32x6.png");

		u32 idx = 0;
		for (s32 i = -15; i < 16; i++, idx += 4)
		{
			s_layerStr[i + 15] = &s_layerMem[idx];
			sprintf(s_layerStr[i + 15], "%d", i);
		}

		// Create a basic 3d camera.
		s_camera.pos = { 0.0f, -2.0f, 0.0f };
		s_camera.yaw = 0.0f;
		s_camera.pitch = 0.0f;
	}

	void disable()
	{
		DXL2_EditorRender::destroy();
	}
		
	bool render3dView()
	{
		if (s_runLevel)
		{
			DXL2_GameLoop::update();
			DXL2_GameLoop::draw();

			if (DXL2_Input::keyPressed(KEY_ESCAPE))
			{
				s_runLevel = false;
				DXL2_Input::enableRelativeMode(false);
				s_renderer->enableScreenClear(true);
			}

			return true;
		}
		return false;
	}

	bool isFullscreen()
	{
		return false;
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

	void cameraControl2d(s32 mx, s32 my)
	{
		f32 moveSpd = s_zoomVisual * 16.0f;
		if (DXL2_Input::keyDown(KEY_W))
		{
			s_offset.z -= moveSpd;
		}
		else if (DXL2_Input::keyDown(KEY_S))
		{
			s_offset.z += moveSpd;
		}

		if (DXL2_Input::keyDown(KEY_A))
		{
			s_offset.x -= moveSpd;
		}
		else if (DXL2_Input::keyDown(KEY_D))
		{
			s_offset.x += moveSpd;
		}

		s32 dx, dy;
		DXL2_Input::getMouseWheel(&dx, &dy);
		if (dy != 0)
		{
			// We want to zoom into the mouse position.
			s32 relX = s32(mx - s_editWinMapCorner.x);
			s32 relY = s32(my - s_editWinMapCorner.z);
			// Old position in world units.
			Vec2f worldPos;
			worldPos.x = s_offset.x + f32(relX) * s_zoomVisual;
			worldPos.z = s_offset.z + f32(relY) * s_zoomVisual;

			s_zoom = std::max(s_zoom - f32(dy) * s_zoom * 0.1f, 0.001f);
			s_zoomVisual = floorf(s_zoom * 1000.0f) * 0.001f;

			// We want worldPos to stay put as we zoom
			Vec2f newWorldPos;
			newWorldPos.x = s_offset.x + f32(relX) * s_zoomVisual;
			newWorldPos.z = s_offset.z + f32(relY) * s_zoomVisual;
			s_offset.x += (worldPos.x - newWorldPos.x);
			s_offset.z += (worldPos.z - newWorldPos.z);
		}
	}

	// return true to use relative mode.
	bool cameraControl3d(s32 mx, s32 my)
	{
		// Movement
		f32 moveSpd = 16.0f / 60.0f;
		if (DXL2_Input::keyDown(KEY_LSHIFT) || DXL2_Input::keyDown(KEY_RSHIFT))
		{
			moveSpd *= 10.0f;
		}

		if (DXL2_Input::keyDown(KEY_W))
		{
			s_camera.pos.x -= s_camera.viewMtx.m2.x * moveSpd;
			s_camera.pos.y -= s_camera.viewMtx.m2.y * moveSpd;
			s_camera.pos.z -= s_camera.viewMtx.m2.z * moveSpd;
		}
		else if (DXL2_Input::keyDown(KEY_S))
		{
			s_camera.pos.x += s_camera.viewMtx.m2.x * moveSpd;
			s_camera.pos.y += s_camera.viewMtx.m2.y * moveSpd;
			s_camera.pos.z += s_camera.viewMtx.m2.z * moveSpd;
		}

		if (DXL2_Input::keyDown(KEY_A))
		{
			s_camera.pos.x -= s_camera.viewMtx.m0.x * moveSpd;
			s_camera.pos.y -= s_camera.viewMtx.m0.y * moveSpd;
			s_camera.pos.z -= s_camera.viewMtx.m0.z * moveSpd;
		}
		else if (DXL2_Input::keyDown(KEY_D))
		{
			s_camera.pos.x += s_camera.viewMtx.m0.x * moveSpd;
			s_camera.pos.y += s_camera.viewMtx.m0.y * moveSpd;
			s_camera.pos.z += s_camera.viewMtx.m0.z * moveSpd;
		}

		// Turning.
		if (DXL2_Input::mouseDown(MBUTTON_RIGHT))
		{
			s32 dx, dy;
			DXL2_Input::getMouseMove(&dx, &dy);

			const f32 turnSpeed = 0.01f;
			s_camera.yaw += f32(dx) * turnSpeed;
			s_camera.pitch -= f32(dy) * turnSpeed;

			if (s_camera.yaw < 0.0f) { s_camera.yaw += PI * 2.0f; }
			else { s_camera.yaw = fmodf(s_camera.yaw, PI * 2.0f); }

			if (s_camera.pitch < -1.55f) { s_camera.pitch = -1.55f; }
			else if (s_camera.pitch > 1.55f) { s_camera.pitch = 1.55f; }

			s_cursor3d = { 0 };
			return true;
		}
		else if (DXL2_Input::relativeModeEnabled())
		{
			s_hideFrames = 3;
		}
		return false;
	}

	void editWinControls2d(s32 mx, s32 my)
	{
		// We want to zoom into the mouse position.
		s32 relX = s32(mx - s_editWinMapCorner.x);
		s32 relY = s32(my - s_editWinMapCorner.z);
		// Old position in world units.
		Vec2f worldPos;
		worldPos.x = s_offset.x + f32(relX) * s_zoomVisual;
		worldPos.z = -(s_offset.z + f32(relY) * s_zoomVisual);

		// Select sector.
		if (DXL2_Input::mousePressed(MBUTTON_LEFT) && s_levelData && s_editMode == LEDIT_SECTOR)
		{
			s_selectedSector = LevelEditorData::findSector(s_layerIndex + s_layerMin, &worldPos);
		}
		else if (s_levelData && s_editMode == LEDIT_SECTOR)
		{
			s_hoveredSector = LevelEditorData::findSector(s_layerIndex + s_layerMin, &worldPos);
		}
		if (s_editMode != LEDIT_SECTOR) { s_selectedSector = -1; }
		if (s_editMode != LEDIT_ENTITY) { s_selectedEntity = -1; }

		// Select Wall.
		if (DXL2_Input::mousePressed(MBUTTON_LEFT) && s_levelData && s_editMode == LEDIT_WALL)
		{
			s_selectedWallSector = LevelEditorData::findSector(s_layerIndex + s_layerMin, &worldPos);
			s_selectedWall = LevelEditorData::findClosestWall(&s_selectedWallSector, s_layerIndex + s_layerMin, &worldPos, s_zoomVisual * 32.0f);
		}
		else if (s_levelData && s_editMode == LEDIT_WALL)
		{
			s_hoveredWallSector = LevelEditorData::findSector(s_layerIndex + s_layerMin, &worldPos);
			s_hoveredWall = LevelEditorData::findClosestWall(&s_hoveredWallSector, s_layerIndex + s_layerMin, &worldPos, s_zoomVisual * 32.0f);
		}
		if (s_editMode != LEDIT_WALL)
		{
			s_selectedWall = -1;
			s_selectedWallSector = -1;
		}

		// Find the closest vertex (if in vertex mode)...
		// Note this should be optimized.
		s_hoveredVertex = -1;
		if (s_editMode == LEDIT_VERTEX && s_levelData)
		{
			const s32 sectorCount = (s32)s_levelData->sectors.size();
			const EditorSector* sector = s_levelData->sectors.data();
			const f32 maxValidDistSq = s_zoomVisual * s_zoomVisual * 256.0f;
			f32 minDistSq = maxValidDistSq;
			for (s32 s = 0; s < sectorCount; s++, sector++)
			{
				if (sector->layer != s_layerIndex + s_layerMin) { continue; }

				const Vec2f* vtx = sector->vertices.data();
				const s32 vtxCount = (s32)sector->vertices.size();
				for (s32 v = 0; v < vtxCount; v++, vtx++)
				{
					const Vec2f diff = { vtx->x - worldPos.x, vtx->z - worldPos.z };
					const f32 distSq = diff.x*diff.x + diff.z*diff.z;
					if (distSq < minDistSq && distSq < maxValidDistSq)
					{
						minDistSq = distSq;
						s_hoveredVertex = v;
						s_hoveredVertexSector = s;
					}
				}
			}
		}

		s_hoveredEntity = -1;
		if (s_editMode == LEDIT_ENTITY && s_levelData)
		{
			s_hoveredEntitySector = LevelEditorData::findSector(s_layerIndex + s_layerMin, &worldPos);
			if (s_hoveredEntitySector >= 0)
			{
				const EditorSector* sector = s_levelData->sectors.data() + s_hoveredEntitySector;
				const u32 count = (u32)sector->objects.size();
				const EditorLevelObject* obj = sector->objects.data();
				for (u32 i = 0; i < count; i++, obj++)
				{
					const f32 width = obj->display ? (f32)obj->display->width : 1.0f;
					const f32 height = obj->display ? (f32)obj->display->height : 1.0f;
					// Half width
					f32 w;
					if (obj->oclass == CLASS_SPIRIT || obj->oclass == CLASS_SAFE || obj->oclass == CLASS_SOUND) { w = 1.0f; }
					else if (obj->oclass == CLASS_3D && obj->displayModel)
					{
						if (worldPos.x >= obj->displayModel->localAabb[0].x + obj->pos.x && worldPos.x < obj->displayModel->localAabb[1].x + obj->pos.x &&
							worldPos.z >= obj->displayModel->localAabb[0].z + obj->pos.z && worldPos.z < obj->displayModel->localAabb[1].z + obj->pos.z)
						{
							s_hoveredEntity = i;
							break;
						}
					}
					else { w = obj->display ? (f32)obj->display->width * obj->display->scale.x / 16.0f : 1.0f; }

					if (obj->oclass != CLASS_3D)
					{
						const f32 x0 = obj->pos.x - w, x1 = obj->pos.x + w;
						const f32 z0 = obj->pos.z - w, z1 = obj->pos.z + w;
						if (worldPos.x >= x0 && worldPos.x < x1 && worldPos.z >= z0 && worldPos.z < z1)
						{
							s_hoveredEntity = i;
							break;
						}
					}
				}
			}
			if (DXL2_Input::mousePressed(MBUTTON_LEFT))
			{
				s_selectedEntity = s_hoveredEntity;
				s_selectedEntitySector = s_hoveredEntitySector;
			}
		}

		s_offsetVis.x = floorf(s_offset.x * 100.0f) * 0.01f;
		s_offsetVis.z = floorf(s_offset.z * 100.0f) * 0.01f;
	}

	Vec4f transform(const Mat4& mtx, const Vec4f& vec)
	{
		return { DXL2_Math::dot(&mtx.m0, &vec), DXL2_Math::dot(&mtx.m1, &vec), DXL2_Math::dot(&mtx.m2, &vec), DXL2_Math::dot(&mtx.m3, &vec) };
	}

	// Get the world direction from the current screen pixel coordinates.
	Vec3f getWorldDir(s32 x, s32 y, s32 rtWidth, s32 rtHeight)
	{
		// Given a plane at Z = 1.0, figure out the view space position of the pixel.
		const Vec4f posScreen =
		{
            f32(x) / f32(rtWidth)  * 2.0f - 1.0f,
		    f32(y) / f32(rtHeight) * 2.0f - 1.0f,
		   -1.0f, 1.0f
		};
		const Vec4f posView = transform(s_camera.invProj, posScreen);
		const Mat3  viewT   = DXL2_Math::transpose(s_camera.viewMtx);

		const Vec3f posView3 = { posView.x, posView.y, posView.z };
		const Vec3f posRelWorld =	// world relative to the camera position.
		{
			DXL2_Math::dot(&posView3, &viewT.m0),
			DXL2_Math::dot(&posView3, &viewT.m1),
			DXL2_Math::dot(&posView3, &viewT.m2)
		};
		return DXL2_Math::normalize(&posRelWorld);
	}

	void editWinControls3d(s32 mx, s32 my, s32 rtWidth, s32 rtHeight)
	{
		s_hoveredSector = -1;
		s_hoveredWall = -1;
		s_hoveredVertex = -1;
		s_hoveredVertexSector = -1;
		s_hoveredEntity = -1;
		if (!DXL2_Input::relativeModeEnabled() && s_hideFrames == 0)
		{
			s_rayDir = getWorldDir(mx - (s32)s_editWinMapCorner.x, my - (s32)s_editWinMapCorner.z, rtWidth, rtHeight);

			const Ray ray = { s_camera.pos, s_rayDir, -1, 1000.0f, s_editMode == LEDIT_ENTITY, s_layerIndex + s_layerMin };
			RayHitInfoLE hitInfo;
			if (LevelEditorData::traceRay(&ray, &hitInfo))
			{
				s_cursor3d = hitInfo.hitPoint;

				if (s_editMode == LEDIT_SECTOR)
				{
					if (DXL2_Input::mousePressed(MBUTTON_LEFT))
					{
						s_selectedSector = hitInfo.hitSectorId;
					}
					else
					{
						s_hoveredSector = hitInfo.hitSectorId;
					}
				}
				else if (s_editMode == LEDIT_WALL && hitInfo.hitWallId >= 0)
				{
					if (DXL2_Input::mousePressed(MBUTTON_LEFT))
					{
						s_selectedWallSector = hitInfo.hitSectorId;
						s_selectedWall = hitInfo.hitWallId;
						s_selectWallPart = hitInfo.hitPart;
					}
					else
					{
						s_hoveredWallSector = hitInfo.hitSectorId;
						s_hoveredWall = hitInfo.hitWallId;
						s_hoveredWallPart = hitInfo.hitPart;
					}
				}
				else if (s_editMode == LEDIT_WALL)
				{
					// In the last case, the ray hit a surface but not a wall.
					// Since this is wall edit mode, see if there is a "hidden" wall close enough to select (i.e. an adjoin with no mask wall or upper/lower part).
					const EditorSector* sector = s_levelData->sectors.data() + hitInfo.hitSectorId;
					const EditorWall* wall = sector->walls.data();
					const Vec2f* vtx = sector->vertices.data();
					const u32 wallCount = (u32)sector->walls.size();

					const Vec2f hitPoint2d = { hitInfo.hitPoint.x, hitInfo.hitPoint.z };
					const f32 distFromCam = DXL2_Math::distance(&hitInfo.hitPoint, &s_camera.pos);
					const f32 maxDist = distFromCam * 64.0f / f32(rtHeight);
					const f32 maxDistSq = maxDist * maxDist;

					f32 closestDistSq = maxDistSq;
					s32 closestWall = -1;
					for (u32 w = 0; w < wallCount; w++, wall++)
					{
						if (wall->adjoin < 0) { continue; }
						// Test to see if we should select the wall parts themselves.
						EditorSector* next = s_levelData->sectors.data() + wall->adjoin;
						if ((next->floorAlt < sector->floorAlt - 0.1f && hitInfo.hitPart == HIT_PART_FLOOR) || (next->ceilAlt > sector->ceilAlt + 0.1f && hitInfo.hitPart == HIT_PART_CEIL)) { continue; }

						Vec2f closest;
						Geometry::closestPointOnLineSegment(vtx[wall->i0], vtx[wall->i1], hitPoint2d, &closest);
						const Vec2f diff = { closest.x - hitPoint2d.x, closest.z - hitPoint2d.z };
						const f32 distSq = DXL2_Math::dot(&diff, &diff);
						if (distSq < closestDistSq)
						{
							closestDistSq = distSq;
							closestWall = s32(w);
						}
					}
					if (closestWall >= 0)
					{
						if (DXL2_Input::mousePressed(MBUTTON_LEFT))
						{
							s_selectedWallSector = hitInfo.hitSectorId;
							s_selectedWall = closestWall;
							s_selectWallPart = HIT_PART_MID;
						}
						else
						{
							s_hoveredWallSector = hitInfo.hitSectorId;
							s_hoveredWall = closestWall;
							s_hoveredWallPart = HIT_PART_MID;
						}
					}
					else if (DXL2_Input::mousePressed(MBUTTON_LEFT))
					{
						s_selectedWall = -1;
					}
				}
				else if (s_editMode == LEDIT_VERTEX)
				{
					s_hoveredVertexSector = hitInfo.hitSectorId;

					// Check for the nearest vertex in the current sector.
					// In the last case, the ray hit a surface but not a wall.
					// Since this is wall edit mode, see if there is a "hidden" wall close enough to select (i.e. an adjoin with no mask wall or upper/lower part).
					const EditorSector* sector = s_levelData->sectors.data() + hitInfo.hitSectorId;
					const Vec2f* vtx = sector->vertices.data();
					const u32 vtxCount = (u32)sector->vertices.size();

					const f32 distFromCam = DXL2_Math::distance(&hitInfo.hitPoint, &s_camera.pos);
					const f32 maxDist = distFromCam * 64.0f / f32(rtHeight);
					f32 minDistSq = maxDist * maxDist;
					s32 closestVtx = -1;
					RayHitPart closestPart = HIT_PART_FLOOR;

					for (u32 v = 0; v < vtxCount; v++)
					{
						// Check against the floor and ceiling vertex of each vertex.
						const Vec3f floorVtx = { vtx[v].x, sector->floorAlt, vtx[v].z };
						const Vec3f ceilVtx  = { vtx[v].x, sector->ceilAlt,  vtx[v].z };

						const f32 floorDistSq = DXL2_Math::distanceSq(&floorVtx, &hitInfo.hitPoint);
						const f32 ceilDistSq  = DXL2_Math::distanceSq(&ceilVtx, &hitInfo.hitPoint);
						if (floorDistSq < minDistSq)
						{
							minDistSq = floorDistSq;
							closestVtx = v;
							closestPart = HIT_PART_FLOOR;
						}
						if (ceilDistSq < minDistSq)
						{
							minDistSq = ceilDistSq;
							closestVtx = v;
							closestPart = HIT_PART_CEIL;
						}
					}

					if (closestVtx >= 0)
					{
						if (DXL2_Input::mousePressed(MBUTTON_LEFT))
						{
							s_selectedVertexSector = hitInfo.hitSectorId;
							s_selectedVertex = closestVtx;
							s_selectVertexPart = closestPart;
						}
						s_hoveredVertex = closestVtx;
						s_hoveredVertexPart = closestPart;
					}
					else if (DXL2_Input::mousePressed(MBUTTON_LEFT))
					{
						s_selectedVertex = -1;
					}
				}
				else if (s_editMode == LEDIT_ENTITY)
				{
					if (DXL2_Input::mousePressed(MBUTTON_LEFT))
					{
						s_selectedEntitySector = hitInfo.hitSectorId;
						s_selectedEntity = hitInfo.hitObjectId;
					}
					s_hoveredEntity = hitInfo.hitObjectId;
					s_hoveredEntitySector = hitInfo.hitSectorId;
				}

				if (s_editMode != LEDIT_SECTOR) { s_selectedSector = -1; }
				if (s_editMode != LEDIT_WALL)   { s_selectedWall = -1;   }
				if (s_editMode != LEDIT_VERTEX) { s_selectedVertex = -1; }
				if (s_editMode != LEDIT_ENTITY) { s_selectedEntity = -1; }
			}
			else if (DXL2_Input::mousePressed(MBUTTON_LEFT))
			{
				s_selectedSector = -1;
				s_selectedWall = -1;
				s_selectedVertex = -1;
				s_cursor3d = { 0 };
			}
		}

		if (s_hideFrames > 0) { s_hideFrames--; }
	}

	// Controls for the main level edit window.
	// This will only be called if the mouse is inside the area and its not covered by popups (like combo boxes).
	void editWinControls(s32 rtWidth, s32 rtHeight)
	{
		// Edit controls.
		s32 mx, my;
		DXL2_Input::getMousePos(&mx, &my);
		if (!DXL2_Input::relativeModeEnabled() && (mx < s_editWinPos.x || mx >= s_editWinPos.x + s_editWinSize.x || my < s_editWinPos.z || my >= s_editWinPos.z + s_editWinSize.z))
		{
			return;
		}

		if (s_editView == EDIT_VIEW_2D)
		{
			cameraControl2d(mx, my);
			editWinControls2d(mx, my);
		}
		else
		{
			const bool relEnabled = cameraControl3d(mx, my);
			editWinControls3d(mx, my, rtWidth, rtHeight);

			DXL2_Input::enableRelativeMode(relEnabled);
		}
	}

	void drawVertex(const Vec2f* vtx, const f32 scale, const u32* color)
	{
		Vec2f quad[8];
		// Outer Quad
		quad[0].x = vtx->x - scale;
		quad[0].z = vtx->z - scale;

		quad[1].x = quad[0].x + 2.0f * scale;
		quad[1].z = quad[0].z;

		quad[2].x = quad[0].x + 2.0f * scale;
		quad[2].z = quad[0].z + 2.0f * scale;

		quad[3].x = quad[0].x;
		quad[3].z = quad[0].z + 2.0f * scale;

		// Inner Quad
		quad[4].x = vtx->x - scale * 0.5f;
		quad[4].z = vtx->z - scale * 0.5f;

		quad[5].x = quad[4].x + 2.0f * scale * 0.5f;
		quad[5].z = quad[4].z;

		quad[6].x = quad[4].x + 2.0f * scale * 0.5f;
		quad[6].z = quad[4].z + 2.0f * scale * 0.5f;

		quad[7].x = quad[4].x;
		quad[7].z = quad[4].z + 2.0f * scale * 0.5f;

		Vec2f tri[12] = { quad[0], quad[1], quad[2], quad[0], quad[2], quad[3],
						  quad[4], quad[5], quad[6], quad[4], quad[6], quad[7] };
		TriColoredDraw2d::addTriangles(4, tri, color);
	}
		
	void highlightWall(bool selected)
	{
		if ((selected && s_selectedWall < 0) || (!selected && s_hoveredWall < 0)) { return; }

		const s32 wallId = s_selectedWall >= 0 && selected ? s_selectedWall : s_hoveredWall;
		const s32 sectorId = s_selectedWall >= 0 && selected ? s_selectedWallSector : s_hoveredWallSector;

		EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const EditorWall* wall = sector->walls.data() + wallId;
		const Vec2f* vtx = sector->vertices.data();

		Vec2f line[] = { vtx[wall->i0], vtx[wall->i1] };
		u32 lineClr;
		if (s_selectedWall >= 0 && selected)
		{
			lineClr = wall->adjoin < 0 ? 0xffffA040 : 0xff805020;
		}
		else
		{
			lineClr = wall->adjoin < 0 ? 0xffffC080 : 0xff806040;
		}
		u32 color[] = { lineClr, lineClr };
		LineDraw2d::addLines(1, 2.0f, line, color);

		Vec2f nrm = { vtx[wall->i1].z - vtx[wall->i0].z, -(vtx[wall->i1].x - vtx[wall->i0].x) };
		nrm = DXL2_Math::normalize(&nrm);
		const f32 nScale = 8.0f * s_zoomVisual;

		line[0] = { (vtx[wall->i0].x + vtx[wall->i1].x) * 0.5f, (vtx[wall->i0].z + vtx[wall->i1].z) * 0.5f };
		line[1] = { line[0].x + nrm.x * nScale, line[0].z + nrm.z * nScale };
		LineDraw2d::addLines(1, 2.0f, line, color);
	}

	void beginDraw2d(u32 rtWidth, u32 rtHeight)
	{
		TriColoredDraw2d::begin(rtWidth, rtHeight, s_zoomVisual, s_offsetVis.x, s_offsetVis.z);
		TriTexturedDraw2d::begin(rtWidth, rtHeight, s_zoomVisual, s_offsetVis.x, s_offsetVis.z);
		LineDraw2d::begin(rtWidth, rtHeight, s_zoomVisual, s_offsetVis.x, s_offsetVis.z);
	}

	void flushTriangles()
	{
		TriTexturedDraw2d::drawTriangles();
		TriColoredDraw2d::drawTriangles();
	}

	struct InfWall
	{
		const EditorSector* sector;
		u32 wallId;
		u32 triggerType;
	};
	static std::vector<InfWall> s_infWalls;
		
	void drawSectorWalls2d(s32 drawLayer, s32 layer, u8 infType)
	{
		if (infType != INF_NONE && infType != INF_ALL && drawLayer != layer) { return; }

		// First draw the non-INF affected walls.
		const u32 sectorCount = (u32)s_levelData->sectors.size();
		EditorSector* sector = s_levelData->sectors.data();
		for (u32 i = 0; i < sectorCount; i++, sector++)
		{
			if (sector->layer != drawLayer || s_selectedSector == i || s_hoveredSector == i) { continue; }
			if ((infType != INF_NONE || drawLayer == layer) && sector->infType != infType && infType != INF_ALL) { continue; }
			
			const u32 wallCount = (u32)sector->walls.size();
			const EditorWall* wall = sector->walls.data();
			const Vec2f* vtx = sector->vertices.data();

			for (u32 w = 0; w < wallCount; w++, wall++)
			{
				Vec2f line[] = { vtx[wall->i0], vtx[wall->i1] };
				u32 lineClr;
				
				if (drawLayer != layer)
				{
					u32 alpha = 0x40 / (layer - drawLayer);
					lineClr = 0x00808000 | (alpha << 24);
				}
				else
				{
					if (infType == INF_NONE || infType == INF_ALL) { lineClr = wall->adjoin < 0 ? 0xffffffff : 0xff808080; }
					else if (infType == INF_SELF) { lineClr = wall->adjoin < 0 ? 0xffff80ff : 0xff804080; }
					else if (infType == INF_OTHER) { lineClr = wall->adjoin < 0 ? 0xffffff80 : 0xff808020; }
				}

				u32 color[] = { lineClr, lineClr };
				LineDraw2d::addLines(1, 1.5f, line, color);

				if (s_showSectorColors && sector->layer == layer && wall->infType != INF_NONE)
				{
					s_infWalls.push_back({ sector, w, (u32)wall->triggerType });
				}
			}
		}
	}

	void drawLineToTarget2d(const Vec2f* p0, s32 sectorId, s32 wallId, f32 width, u32 color)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const Vec2f* vtx = sector->vertices.data();
		Vec2f target = { 0 };

		if (wallId >= 0)
		{
			const EditorWall* wall = sector->walls.data() + wallId;
			target.x = (vtx[wall->i0].x + vtx[wall->i1].x) * 0.5f;
			target.z = (vtx[wall->i0].z + vtx[wall->i1].z) * 0.5f;
		}
		else
		{
			const u32 vtxCount = (u32)sector->vertices.size();
			for (u32 v = 0; v < vtxCount; v++)
			{
				target.x += vtx[v].x;
				target.z += vtx[v].z;
			}
			const f32 scale = 1.0f / f32(vtxCount);
			target.x *= scale;
			target.z *= scale;
		}
		Vec2f line[] = { *p0, target };
		u32 colors[] = { color, color };
		LineDraw2d::addLine(width, line, colors);
	}

	// If the selected wall or sector has an INF script, draw lines towards
	// 1. Any objects it effects (targets and clients).
	// 2. Any slaves (sector).
	void drawTargetsAndClientLines2d(s32 sectorId, f32 width)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const EditorInfItem* item = sector->infItem;
		if (!item) { return; }

		Vec2f lineStart = { 0 };
		const Vec2f* vtx = sector->vertices.data();
		const u32 vtxCount = (u32)sector->vertices.size();
		for (u32 v = 0; v < vtxCount; v++)
		{
			lineStart.x += vtx[v].x;
			lineStart.z += vtx[v].z;
		}
		const f32 scale = 1.0f / f32(vtxCount);
		lineStart.x *= scale;
		lineStart.z *= scale;

		for (u32 c = 0; c < (u32)item->classData.size(); c++)
		{
			const EditorInfClassData* cdata = &item->classData[c];
			u32 stopCount = 0;
			if (cdata->iclass == INF_CLASS_TRIGGER)
			{
				stopCount = 1;
			}
			else if (cdata->iclass == INF_CLASS_ELEVATOR)
			{
				stopCount = (u32)cdata->stop.size();
			}

			for (u32 s = 0; s < stopCount; s++)
			{
				const u32 funcCount = (u32)cdata->stop[s].func.size();
				for (u32 f = 0; f < funcCount; f++)
				{
					// func | clientCount << 8 | argCount << 16  (top bot available).
					const u32 funcId = cdata->stop[s].func[f].funcId;
					const u32 clientCount = (u32)cdata->stop[s].func[f].client.size();
					for (u32 cl = 0; cl < clientCount; cl++)
					{
						const EditorSector* clientSector = LevelEditorData::getSector(cdata->stop[s].func[f].client[cl].sectorName.c_str());
						const s32 clientWallId = cdata->stop[s].func[f].client[cl].wallId;
						if (!clientSector) { continue; }

						if (clientWallId >= 0)
						{
							drawLineToTarget2d(&lineStart, clientSector->id, clientWallId, width, 0xffff2020);
						}
						else
						{
							drawLineToTarget2d(&lineStart, clientSector->id, -1, width, 0xffff2020);
						}
					}

					// Draw to the adjoining lines.
					if (funcId == INF_MSG_ADJOIN)
					{
						drawLineToTarget2d(&lineStart, LevelEditorData::getSector(cdata->stop[s].func[f].arg[0].sValue.c_str())->id, cdata->stop[s].func[f].arg[1].iValue, width, 0xff2020ff);
						drawLineToTarget2d(&lineStart, LevelEditorData::getSector(cdata->stop[s].func[f].arg[2].sValue.c_str())->id, cdata->stop[s].func[f].arg[3].iValue, width, 0xff2020ff);
					}
				}
			}
		}
	}

	void drawTargetsAndClientLines2d(s32 sectorId, s32 wallId, f32 width)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const Vec2f* vtx = sector->vertices.data();
		const EditorWall* wall = sector->walls.data() + wallId;
		const EditorInfItem* item = wall->infItem;
				
		Vec2f lineStart;
		lineStart.x = (vtx[wall->i0].x + vtx[wall->i1].x) * 0.5f;
		lineStart.z = (vtx[wall->i0].z + vtx[wall->i1].z) * 0.5f;

		for (u32 c = 0; c < (u32)item->classData.size(); c++)
		{
			const EditorInfClassData* cdata = &item->classData[c];
			u32 stopCount = 0;
			if (cdata->iclass == INF_CLASS_TRIGGER)
			{
				stopCount = 1;
			}
			else if (cdata->iclass == INF_CLASS_ELEVATOR)
			{
				stopCount = (u32)cdata->stop.size();
			}

			for (u32 s = 0; s < stopCount; s++)
			{
				const u32 funcCount = (u32)cdata->stop[s].func.size();
				for (u32 f = 0; f < funcCount; f++)
				{
					const u32 clientCount = (u32)cdata->stop[s].func[f].client.size();
					for (u32 cl = 0; cl < clientCount; cl++)
					{
						const EditorSector* clientSector = LevelEditorData::getSector(cdata->stop[s].func[f].client[cl].sectorName.c_str());
						const s32 clientWallId = cdata->stop[s].func[f].client[cl].wallId;
						if (!clientSector) { continue; }

						if (clientWallId >= 0)
						{
							drawLineToTarget2d(&lineStart, clientSector->id, clientWallId, width, 0xffff2020);
						}
						else
						{
							drawLineToTarget2d(&lineStart, clientSector->id, -1, width, 0xffff2020);
						}
					}
				}
			}
		}
	}

	void drawTargetsAndClientSlaves2d(s32 sectorId, f32 width)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const EditorInfItem* item = sector->infItem;
		if (!item) { return; }

		Vec2f lineStart = { 0 };
		const Vec2f* vtx = sector->vertices.data();
		const u32 vtxCount = (u32)sector->vertices.size();
		for (u32 v = 0; v < vtxCount; v++)
		{
			lineStart.x += vtx[v].x;
			lineStart.z += vtx[v].z;
		}
		const f32 scale = 1.0f / f32(vtxCount);
		lineStart.x *= scale;
		lineStart.z *= scale;

		for (u32 c = 0; c < (u32)item->classData.size(); c++)
		{
			const EditorInfClassData* cdata = &item->classData[c];
			for (u32 s = 0; s < (u32)cdata->slaves.size(); s++)
			{
				const EditorSector* slaveSector = LevelEditorData::getSector(cdata->slaves[s].c_str());
				drawLineToTarget2d(&lineStart, slaveSector->id, -1, width, 0xff20ff20);
			}
		}
	}

	void drawLevel2d(u32 rtWidth, u32 rtHeight)
	{
		if (!s_levelData || s_sectorDrawMode == SDM_WIREFRAME || (s_layerIndex + s_layerMin < s_levelData->layerMin))
		{
			// Draw the grid.
			Grid2d::blitToScreen(rtWidth, rtHeight, s_gridSize, s_subGridSize, s_zoomVisual, s_offsetVis.x, s_offsetVis.z, s_gridOpacity);
		}

		// Draw the level.
		if (!s_levelData) { return; }

		const u32 sectorCount = (u32)s_levelData->sectors.size();
		const EditorSector* sector = s_levelData->sectors.data();
		s32 layer = s_layerIndex + s_layerMin;

		beginDraw2d(rtWidth, rtHeight);

		// Loop through all layers, bottom to current - only draw walls for all but the current layer.
		s_infWalls.clear();
		s32 startLayer = s_showLowerLayers ? s_levelData->layerMin : layer;
		for (s32 l = startLayer; l <= layer; l++)
		{
			// Draw the solid sectors before drawing walls.
			if (s_sectorDrawMode == SDM_LIGHTING && l == layer)
			{
				sector = s_levelData->sectors.data();
				for (u32 i = 0; i < sectorCount; i++, sector++)
				{
					if (sector->layer != layer) { continue; }

					u32 L = u32(sector->ambient);
					L = s_fullbright ? 255u : std::max(0u, std::min(CONV_5bitTo8bit(L), 255u));
					const u32 Lrg = L * 7 / 8;
					const u32 lighting = Lrg | (Lrg << 8u) | (L << 16u) | 0xff000000;
					drawSectorPolygon(sector, false, lighting);
				}
			}
			else if ((s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL) && l == layer)
			{
				sector = s_levelData->sectors.data();
				for (u32 i = 0; i < sectorCount; i++, sector++)
				{
					if (sector->layer != layer) { continue; }

					u32 L = u32(sector->ambient);
					L = s_fullbright ? 255u : std::max(0u, std::min(CONV_5bitTo8bit(L), 255u));
					const u32 lighting = L | (L << 8u) | (L << 16u) | 0xff000000;
					drawTexturedSectorPolygon(sector, lighting, s_sectorDrawMode == SDM_TEXTURED_FLOOR);
				}
			}

			if (l == layer && s_sectorDrawMode != SDM_WIREFRAME)
			{
				// Draw the grid.
				flushTriangles();
				Grid2d::blitToScreen(rtWidth, rtHeight, s_gridSize, s_subGridSize, s_zoomVisual, s_offsetVis.x, s_offsetVis.z, s_gridOpacity);
			}

			// Draw the selected sector before drawing the walls.
			if (l == layer && (s_selectedSector >= 0 || s_hoveredSector >= 0))
			{
				if (s_selectedSector >= 0) { drawSectorPolygon(s_levelData->sectors.data() + s_selectedSector, false); }
				if (s_hoveredSector >= 0) { drawSectorPolygon(s_levelData->sectors.data() + s_hoveredSector, true); }
				flushTriangles();
			}

			// Walls
			if (s_showSectorColors)
			{
				drawSectorWalls2d(l, layer, INF_NONE);
				
				// Draw INF walls.
				if (l == layer)
				{
					drawSectorWalls2d(l, layer, INF_SELF);
					drawSectorWalls2d(l, layer, INF_OTHER);

					const size_t iWallCount = s_infWalls.size();
					const InfWall* infWall = s_infWalls.data();
					for (size_t w = 0; w < iWallCount; w++, infWall++)
					{
						const EditorWall* wall = infWall->sector->walls.data() + infWall->wallId;
						const Vec2f* vtx = infWall->sector->vertices.data();

						Vec2f line[] = { vtx[wall->i0], vtx[wall->i1] };
						u32 lineClr = wall->adjoin < 0 ? 0xff2020ff : 0xff101080;
						u32 color[] = { lineClr, lineClr };
						LineDraw2d::addLines(1, 1.5f, line, color);

						Vec2f nrm = { vtx[wall->i1].z - vtx[wall->i0].z, -(vtx[wall->i1].x - vtx[wall->i0].x) };
						nrm = DXL2_Math::normalize(&nrm);
						const f32 nScale = 4.0f * s_zoomVisual;

						line[0] = { (vtx[wall->i0].x + vtx[wall->i1].x) * 0.5f, (vtx[wall->i0].z + vtx[wall->i1].z) * 0.5f };
						line[1] = { line[0].x + nrm.x * nScale, line[0].z + nrm.z * nScale };
						LineDraw2d::addLines(1, 2.0f, line, color);
					}
				}
			}
			else
			{
				drawSectorWalls2d(l, layer, INF_ALL);
			}

			if (l == layer && s_selectedSector >= 0)
			{
				sector = s_levelData->sectors.data() + s_selectedSector;
				const u32 wallCount = (u32)sector->walls.size();
				const EditorWall* wall = sector->walls.data();
				const Vec2f* vtx = sector->vertices.data();

				for (u32 w = 0; w < wallCount; w++, wall++)
				{
					Vec2f line[] = { vtx[wall->i0], vtx[wall->i1] };
					u32 lineClr = wall->adjoin < 0 ? 0xffffA040 : 0xff805020;
					u32 color[] = { lineClr, lineClr };
					LineDraw2d::addLines(1, 2.0f, line, color);
				}
			}
			if (l == layer && s_hoveredSector >= 0)
			{
				sector = s_levelData->sectors.data() + s_hoveredSector;
				const u32 wallCount = (u32)sector->walls.size();
				const EditorWall* wall = sector->walls.data();
				const Vec2f* vtx = sector->vertices.data();

				for (u32 w = 0; w < wallCount; w++, wall++)
				{
					Vec2f line[] = { vtx[wall->i0], vtx[wall->i1] };
					u32 lineClr = wall->adjoin < 0 ? 0xffffC080 : 0xff806040;
					u32 color[] = { lineClr, lineClr };
					LineDraw2d::addLines(1, 2.0f, line, color);
				}
			}
			// Draw the selected sector before drawing the walls.
			if (l == layer && (s_selectedWall >= 0 || s_hoveredWall >= 0))
			{
				highlightWall(true);
				highlightWall(false);
			}
			if (s_selectedSector >= 0 && s_levelData->sectors[s_selectedSector].infType != INF_NONE)
			{
				// If the selected wall or sector has an INF script, draw lines towards
				drawTargetsAndClientLines2d(s_selectedSector, 2.0f);
				drawTargetsAndClientSlaves2d(s_selectedSector, 2.0f);
			}
			else if (s_selectedWall >= 0 && s_levelData->sectors[s_selectedWallSector].walls[s_selectedWall].infType != INF_NONE)
			{
				drawTargetsAndClientLines2d(s_selectedWallSector, s_selectedWall, 2.0f);
			}
			LineDraw2d::drawLines();
		}

		// Objects
		const u32 alphaFg = s_editMode == LEDIT_ENTITY ? 0xff000000 : 0xA0000000;
		const u32 alphaBg = s_editMode == LEDIT_ENTITY ? 0xA0000000 : 0x60000000;
		sector = s_levelData->sectors.data();
		for (u32 i = 0; i < sectorCount; i++, sector++)
		{
			if (sector->layer != layer) { continue; }
			
			// For now just draw a quad + image.
			const u32 objCount = (u32)sector->objects.size();
			const EditorLevelObject* obj = sector->objects.data();
			for (u32 o = 0; o < objCount; o++, obj++)
			{
				const bool selected = s_selectedEntity == o && s_selectedEntitySector == i;
				const bool highlight = (s_hoveredEntity == o && s_hoveredEntitySector == i) || selected;
				const u32 clrBg = highlight ? (0xffae8653 | alphaBg) : (0x0051331a | alphaBg);

				if (obj->oclass == CLASS_3D)
				{
					const Vec2f pos = { obj->pos.x, obj->pos.z };
					DXL2_EditorRender::drawModel2d_Bounds(obj->displayModel, &obj->pos, &obj->rotMtx, clrBg, highlight);
					DXL2_EditorRender::drawModel2d(sector, obj->displayModel, &pos, &obj->rotMtx, s_palette->colors, alphaFg);
				}
				else
				{
					f32 width  = obj->display ? (f32)obj->display->width  : 1.0f;
					f32 height = obj->display ? (f32)obj->display->height : 1.0f;
					
					f32 x0 = -obj->worldExt.x, z0 = -obj->worldExt.z;
					f32 x1 =  obj->worldExt.x, z1 =  obj->worldExt.z;
					if (height > width)
					{
						const f32 dx = obj->worldExt.x * width / height;
						x0 = -dx;
						x1 = x0 + 2.0f * dx;
					}
					else if (width > height)
					{
						const f32 dz = obj->worldExt.z * height / width;
						z0 = -dz;
						z1 = z0 + 2.0f * dz;
					}

					const Vec2f vtxTex[6]=
					{
						{obj->worldCen.x + x0, obj->worldCen.z + z0},
						{obj->worldCen.x + x1, obj->worldCen.z + z0},
						{obj->worldCen.x + x1, obj->worldCen.z + z1},

						{obj->worldCen.x + x0, obj->worldCen.z + z0},
						{obj->worldCen.x + x1, obj->worldCen.z + z1},
						{obj->worldCen.x + x0, obj->worldCen.z + z1},
					};

					const f32 scale = highlight ? 1.25f : 1.0f;
					Vec2f vtxClr[6]=
					{
						{obj->worldCen.x - obj->worldExt.x*scale, obj->worldCen.z - obj->worldExt.z*scale},
						{obj->worldCen.x + obj->worldExt.x*scale, obj->worldCen.z - obj->worldExt.z*scale},
						{obj->worldCen.x + obj->worldExt.x*scale, obj->worldCen.z + obj->worldExt.z*scale},

						{obj->worldCen.x - obj->worldExt.x*scale, obj->worldCen.z - obj->worldExt.z*scale},
						{obj->worldCen.x + obj->worldExt.x*scale, obj->worldCen.z + obj->worldExt.z*scale},
						{obj->worldCen.x - obj->worldExt.x*scale, obj->worldCen.z + obj->worldExt.z*scale},
					};

					const Vec2f uv[6] =
					{
						{0.0f, 1.0f},
						{1.0f, 1.0f},
						{1.0f, 0.0f},

						{0.0f, 1.0f},
						{1.0f, 0.0f},
						{0.0f, 0.0f},
					};
					const u32 baseRGB = selected ? 0x00ffa0a0 : 0x00ffffff;
					const u32 colorFg[2] = { baseRGB | alphaFg, baseRGB | alphaFg };
					const u32 colorBg[2] = { clrBg, clrBg };

					TriColoredDraw2d::addTriangles(2, vtxClr, colorBg);
					if (obj->display) { TriTexturedDraw2d::addTriangles(2, vtxTex, uv, colorFg, obj->display->texture); }

					// Draw a direction.
					const f32 angle = obj->orientation.y * PI / 180.0f;
					Vec2f dir = { sinf(angle), cosf(angle) };

					// Make an arrow.
					f32 dx = dir.x * obj->worldExt.x;
					f32 dz = dir.z * obj->worldExt.z;
					f32 cx = obj->pos.x + dx;
					f32 cz = obj->pos.z + dz;
					f32 tx = -dir.z * obj->worldExt.x * 0.5f;
					f32 tz =  dir.x * obj->worldExt.z * 0.5f;

					Vec2f vtx[]=
					{
						{cx, cz}, {cx + tx - dx * 0.25f, cz + tz - dz * 0.25f},
						{cx, cz}, {cx - tx - dx * 0.25f, cz - tz - dz * 0.25f},
					};

					const u32 alphaArrow = s_editMode == LEDIT_ENTITY ? (highlight ? 0xA0000000 : 0x60000000) : 0x30000000;
					u32 baseRGBArrow = 0x00ffff00;
					u32 baseClrArrow = baseRGBArrow | alphaArrow;
					u32 color[] = { baseClrArrow, baseClrArrow, baseClrArrow, baseClrArrow };
					LineDraw2d::addLines(2, 2.0f, vtx, color);
				}
			}
		}
		TriColoredDraw2d::drawTriangles();
		TriTexturedDraw2d::drawTriangles();
		LineDraw2d::drawLines();

		// Vertices
		const u32 color[4] = { 0xffae8653, 0xffae8653, 0xff51331a, 0xff51331a };
		const u32 colorSelected[4] = { 0xffffc379, 0xffffc379, 0xff764a26, 0xff764a26 };

		const f32 scaleMax = s_zoomVisual * 4.0f;
		const f32 scaleMin = s_zoomVisual * 1.0f;
		const f32 scale = std::max(scaleMin, std::min(scaleMax, 0.5f));

		sector = s_levelData->sectors.data();
		for (u32 i = 0; i < sectorCount; i++, sector++)
		{
			if (sector->layer != layer) { continue; }

			const u32 wallCount = (u32)sector->walls.size();
			const EditorWall* wall = sector->walls.data();
			const Vec2f* vtx = sector->vertices.data();

			for (u32 w = 0; w < wallCount; w++, wall++)
			{
				drawVertex(&vtx[wall->i0], scale, color);
			}
		}
		if (s_hoveredVertex >= 0 && s_hoveredVertex < (s32)s_levelData->sectors[s_hoveredVertexSector].vertices.size())
		{
			const Vec2f* vtx = &s_levelData->sectors[s_hoveredVertexSector].vertices[s_hoveredVertex];
			drawVertex(vtx, scale * 1.5f, colorSelected);
		}
		flushTriangles();
	}

	void drawSectorFloorAndCeilColor(const EditorSector* sector, const SectorTriangles* poly, const u32* color, bool blend)
	{
		const u32 triCount = poly->count;
		const Vec2f* vtx = poly->vtx.data();
		for (u32 t = 0; t < triCount; t++, vtx += 3)
		{
			const Vec3f floorCeilVtx[6] =
			{
				{vtx[0].x, sector->floorAlt, vtx[0].z},
				{vtx[2].x, sector->floorAlt, vtx[2].z},
				{vtx[1].x, sector->floorAlt, vtx[1].z},

				{vtx[0].x, sector->ceilAlt, vtx[0].z},
				{vtx[1].x, sector->ceilAlt, vtx[1].z},
				{vtx[2].x, sector->ceilAlt, vtx[2].z},
			};
			const Vec2f floorUv[6] =
			{
				{vtx[0].x, vtx[0].z},
				{vtx[2].x, vtx[2].z},
				{vtx[1].x, vtx[1].z},

				{vtx[0].x, vtx[0].z},
				{vtx[1].x, vtx[1].z},
				{vtx[2].x, vtx[2].z},
			};
			TrianglesColor3d::addTriangles(2, floorCeilVtx, floorUv, color, blend);
		}
	}

	void drawSectorFloorAndCeilTextured(const EditorSector* sector, const SectorTriangles* poly, u32 color)
	{
		const f32* floorOff = &sector->floorTexture.offsetX;
		const f32* ceilOff = &sector->ceilTexture.offsetX;
		const TextureGpu* floorTex = sector->floorTexture.tex->texture;
		const TextureGpu* ceilTex = sector->ceilTexture.tex->texture;
		const f32 scaleFloorX = -8.0f / f32(sector->floorTexture.tex->width);
		const f32 scaleFloorZ = -8.0f / f32(sector->floorTexture.tex->height);
		const f32 scaleCeilX  = -8.0f / f32(sector->ceilTexture.tex->width);
		const f32 scaleCeilZ  = -8.0f / f32(sector->ceilTexture.tex->height);

		const u32 triCount = poly->count;
		
		// Two passes, one for floor and ceiling to avoid changing textures too often...
		const Vec2f* vtx = poly->vtx.data();
		for (u32 t = 0; t < triCount; t++, vtx += 3)
		{
			const Vec3f floorCeilVtx[6] =
			{
				{vtx[0].x, sector->floorAlt, vtx[0].z},
				{vtx[2].x, sector->floorAlt, vtx[2].z},
				{vtx[1].x, sector->floorAlt, vtx[1].z},
			};
			const Vec2f floorUv[6] =
			{
				{vtx[0].x, vtx[0].z},
				{vtx[2].x, vtx[2].z},
				{vtx[1].x, vtx[1].z},
			};
			const Vec2f floorUv1[6] =
			{
				{(vtx[0].x - floorOff[0]) * scaleFloorX, (vtx[0].z - floorOff[1]) * scaleFloorZ},
				{(vtx[2].x - floorOff[0]) * scaleFloorX, (vtx[2].z - floorOff[1]) * scaleFloorZ},
				{(vtx[1].x - floorOff[0]) * scaleFloorX, (vtx[1].z - floorOff[1]) * scaleFloorZ},
			};

			TrianglesColor3d::addTexturedTriangle(floorCeilVtx, floorUv, floorUv1, color, floorTex);
		}

		vtx = poly->vtx.data();
		for (u32 t = 0; t < triCount; t++, vtx += 3)
		{
			const Vec3f floorCeilVtx[6] =
			{
				{vtx[0].x, sector->ceilAlt, vtx[0].z},
				{vtx[1].x, sector->ceilAlt, vtx[1].z},
				{vtx[2].x, sector->ceilAlt, vtx[2].z},
			};
			const Vec2f floorUv[6] =
			{
				{vtx[0].x, vtx[0].z},
				{vtx[1].x, vtx[1].z},
				{vtx[2].x, vtx[2].z},
			};
			const Vec2f floorUv1[6] =
			{
				{(vtx[0].x - ceilOff[0]) * scaleCeilX, (vtx[0].z - ceilOff[1]) * scaleCeilZ},
				{(vtx[1].x - ceilOff[0]) * scaleCeilX, (vtx[1].z - ceilOff[1]) * scaleCeilZ},
				{(vtx[2].x - ceilOff[0]) * scaleCeilX, (vtx[2].z - ceilOff[1]) * scaleCeilZ},
			};

			TrianglesColor3d::addTexturedTriangle(floorCeilVtx, floorUv, floorUv1, color, ceilTex);
		}
	}

	f32 wallLength(const EditorWall* wall, const EditorSector* sector)
	{
		const Vec2f* vtx = sector->vertices.data();
		const Vec2f offset = { vtx[wall->i1].x - vtx[wall->i0].x, vtx[wall->i1].z - vtx[wall->i0].z };
		return sqrtf(offset.x * offset.x + offset.z * offset.z);
	}

	void buildWallVertices(f32 floorAlt, f32 ceilAlt, const Vec2f* v0, const Vec2f* v1, Vec3f* wallVertex, Vec2f* wallUv)
	{
		const f32 dx = fabsf(v1->x - v0->x);
		const f32 dz = fabsf(v1->z - v0->z);

		wallVertex[0] = { v0->x, ceilAlt, v0->z };
		wallVertex[1] = { v1->x, ceilAlt, v1->z };
		wallVertex[2] = { v1->x, floorAlt, v1->z };

		wallVertex[3] = { v0->x, ceilAlt, v0->z };
		wallVertex[4] = { v1->x, floorAlt, v1->z };
		wallVertex[5] = { v0->x, floorAlt, v0->z };

		wallUv[0] = { (dx >= dz) ? v0->x : v0->z, ceilAlt };
		wallUv[1] = { (dx >= dz) ? v1->x : v1->z, ceilAlt };
		wallUv[2] = { (dx >= dz) ? v1->x : v1->z, floorAlt };

		wallUv[3] = { (dx >= dz) ? v0->x : v0->z, ceilAlt };
		wallUv[4] = { (dx >= dz) ? v1->x : v1->z, floorAlt };
		wallUv[5] = { (dx >= dz) ? v0->x : v0->z, floorAlt };
	}

	void buildWallTextureVertices(const EditorSectorTexture* stex, f32 wallLen, f32 h, bool flipHorz, u32 texWidth, u32 texHeight, Vec2f* wallUv1)
	{
		f32 u0 = (stex->offsetX) * c_worldToTexelScale;
		f32 u1 = u0 + wallLen;
		f32 v0 = (-stex->offsetY - h) * c_worldToTexelScale;
		f32 v1 = (-stex->offsetY) * c_worldToTexelScale;

		const f32 textureScaleX = 1.0f / f32(texWidth);
		const f32 textureScaleY = 1.0f / f32(texHeight);
		u0 = u0 * textureScaleX;
		u1 = u1 * textureScaleX;
		v0 = v0 * textureScaleY;
		v1 = v1 * textureScaleY;

		if (flipHorz) { u0 = 1.0f - u0; u1 = 1.0f - u1; }

		wallUv1[0] = { u0, v0 };
		wallUv1[1] = { u1, v0 };
		wallUv1[2] = { u1, v1 };

		wallUv1[3] = { u0, v0 };
		wallUv1[4] = { u1, v1 };
		wallUv1[5] = { u0, v1 };
	}

	void buildSignTextureVertices(const EditorSectorTexture* sbaseTex, const EditorSectorTexture* sTex, f32 wallLen, f32 h, u32 texWidth, u32 texHeight, Vec2f* wallUv1)
	{
		const f32 offsetX = sbaseTex->offsetX - sTex->offsetX;
		// The way the base offset Y is factored in is quite strange but has been arrived at by looking at the data...
		const f32 offsetY = -DXL2_Math::fract(std::max(sbaseTex->offsetY, 0.0f)) + sTex->offsetY;

		f32 u0 = offsetX * c_worldToTexelScale;
		f32 u1 = u0 + wallLen;
		f32 v0 = (-offsetY - h) * c_worldToTexelScale;
		f32 v1 = (-offsetY) * c_worldToTexelScale;

		const f32 textureScaleX = 1.0f / f32(texWidth);
		const f32 textureScaleY = 1.0f / f32(texHeight);
		u0 = u0 * textureScaleX;
		u1 = u1 * textureScaleX;
		v0 = 1.0f + v0 * textureScaleY;
		v1 = 1.0f + v1 * textureScaleY;

		wallUv1[0] = { u0, v0 };
		wallUv1[1] = { u1, v0 };
		wallUv1[2] = { u1, v1 };

		wallUv1[3] = { u0, v0 };
		wallUv1[4] = { u1, v1 };
		wallUv1[5] = { u0, v1 };
	}

	void drawWallColor(const EditorSector* sector, const Vec2f* vtx, const EditorWall* wall, const u32* color, bool blend, RayHitPart part)
	{
		const Vec2f* v0 = &vtx[wall->i0];
		const Vec2f* v1 = &vtx[wall->i1];

		Vec3f wallVertex[6];
		Vec2f wallUv[6];
		if (wall->adjoin < 0 && (part == HIT_PART_UNKNOWN || part == HIT_PART_MID))
		{
			// No adjoin - a single quad.
			buildWallVertices(sector->floorAlt, sector->ceilAlt, v0, v1, wallVertex, wallUv);
			TrianglesColor3d::addTriangles(2, wallVertex, wallUv, color, blend);
		}
		else
		{
			// lower
			f32 nextFloorAlt = s_levelData->sectors[wall->adjoin].floorAlt;
			if (nextFloorAlt < sector->floorAlt && (part == HIT_PART_UNKNOWN || part == HIT_PART_BOT))
			{
				buildWallVertices(sector->floorAlt, nextFloorAlt, v0, v1, wallVertex, wallUv);
				TrianglesColor3d::addTriangles(2, wallVertex, wallUv, color, blend);
			}

			// upper
			f32 nextCeilAlt = s_levelData->sectors[wall->adjoin].ceilAlt;
			if (nextCeilAlt > sector->ceilAlt && (part == HIT_PART_UNKNOWN || part == HIT_PART_TOP))
			{
				buildWallVertices(nextCeilAlt, sector->ceilAlt, v0, v1, wallVertex, wallUv);
				TrianglesColor3d::addTriangles(2, wallVertex, wallUv, color, blend);
			}

			// mask wall or highlight.
			if (((wall->flags[0] & WF1_ADJ_MID_TEX) || blend) && (part == HIT_PART_UNKNOWN || part == HIT_PART_MID))
			{
				const f32 ceilAlt = std::max(nextCeilAlt, sector->ceilAlt);
				const f32 floorAlt = std::min(nextFloorAlt, sector->floorAlt);

				buildWallVertices(floorAlt, ceilAlt, v0, v1, wallVertex, wallUv);
				TrianglesColor3d::addTriangles(2, wallVertex, wallUv, color, blend);
			}
		}
	}
				
	void drawWallTexture(const EditorSector* sector, const Vec2f* vtx, const EditorWall* wall, u32 wallIndex, const u32* color)
	{
		const Vec2f* v0 = &vtx[wall->i0];
		const Vec2f* v1 = &vtx[wall->i1];
		const f32 wallLen = wallLength(wall, sector) * c_worldToTexelScale;
		const bool flipHorz = (wall->flags[0] & WF1_FLIP_HORIZ) != 0u;

		const u32 fullbright[] = { 0xffffffff, 0xffffffff };

		Vec3f wallVertex[6];
		Vec2f wallUv[6];
		Vec2f wallUv1[6];
		if (wall->adjoin < 0)
		{
			const f32 h = sector->floorAlt - sector->ceilAlt;

			buildWallVertices(sector->floorAlt, sector->ceilAlt, v0, v1, wallVertex, wallUv);
			buildWallTextureVertices(&wall->mid, wallLen, h, flipHorz, wall->mid.tex->width, wall->mid.tex->height, wallUv1);
			TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, color, wall->mid.tex->texture);

			if (wall->sign.tex)
			{
				const u32* sgnColor = (wall->flags[0] & WF1_ILLUM_SIGN) ? fullbright : color;
				buildSignTextureVertices(&wall->mid, &wall->sign, wallLen, h, wall->sign.tex->width, wall->sign.tex->height, wallUv1);
				TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, sgnColor, wall->sign.tex->texture, Tri3dTrans::TRANS_CLAMP);
			}
		}
		else
		{
			const EditorSector* next = s_levelData->sectors.data() + wall->adjoin;

			// lower
			if (next->floorAlt < sector->floorAlt && wall->bot.tex)
			{
				const f32 h = sector->floorAlt - next->floorAlt;

				buildWallVertices(sector->floorAlt, next->floorAlt, v0, v1, wallVertex, wallUv);
				buildWallTextureVertices(&wall->bot, wallLen, h, flipHorz, wall->bot.tex->width, wall->bot.tex->height, wallUv1);
				TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, color, wall->bot.tex->texture);

				if (wall->sign.tex)
				{
					const u32* sgnColor = (wall->flags[0] & WF1_ILLUM_SIGN) ? fullbright : color;
					buildSignTextureVertices(&wall->bot, &wall->sign, wallLen, h, wall->sign.tex->width, wall->sign.tex->height, wallUv1);
					TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, sgnColor, wall->sign.tex->texture, Tri3dTrans::TRANS_CLAMP);
				}
			}

			// upper
			if (next->ceilAlt > sector->ceilAlt && wall->top.tex)
			{
				const f32 h = next->ceilAlt - sector->ceilAlt;

				buildWallVertices(next->ceilAlt, sector->ceilAlt, v0, v1, wallVertex, wallUv);
				buildWallTextureVertices(&wall->top, wallLen, h, flipHorz, wall->top.tex->width, wall->top.tex->height, wallUv1);
				TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, color, wall->top.tex->texture);

				// no upper sign if a lower sign was rendered.
				if (wall->sign.tex && next->floorAlt >= sector->floorAlt)
				{
					const u32* sgnColor = (wall->flags[0] & WF1_ILLUM_SIGN) ? fullbright : color;
					buildSignTextureVertices(&wall->top, &wall->sign, wallLen, h, wall->sign.tex->width, wall->sign.tex->height, wallUv1);
					TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, sgnColor, wall->sign.tex->texture, Tri3dTrans::TRANS_CLAMP);
				}
			}

			// mask wall
			if (wall->flags[0] & WF1_ADJ_MID_TEX)
			{
				const f32 ceilAlt  = std::max(next->ceilAlt,  sector->ceilAlt);
				const f32 floorAlt = std::min(next->floorAlt, sector->floorAlt);
				const f32 h = floorAlt - ceilAlt;

				buildWallVertices(floorAlt, ceilAlt, v0, v1, wallVertex, wallUv);
				buildWallTextureVertices(&wall->mid, wallLen, h, flipHorz, wall->mid.tex->width, wall->mid.tex->height, wallUv1);
				TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, color, wall->mid.tex->texture, Tri3dTrans::TRANS_CUTOUT);
			}
		}
	}

	static f32 c_editorCameraFov = 1.57079632679489661923f;

	void drawScreenQuad(const Vec3f* center, f32 side, u32 color)
	{
		const f32 w = side * 0.5f;
		const Vec3f r = { s_camera.viewMtx.m0.x * w, s_camera.viewMtx.m0.y * w, s_camera.viewMtx.m0.z * w };
		const Vec3f u = { s_camera.viewMtx.m1.x * w, s_camera.viewMtx.m1.y * w, s_camera.viewMtx.m1.z * w };

		const Vec3f vtx[]=
		{
			{center->x - r.x - u.x, center->y - r.y - u.y, center->z - r.z - u.z},
			{center->x + r.x - u.x, center->y + r.y - u.y, center->z + r.z - u.z},
			{center->x + r.x + u.x, center->y + r.y + u.y, center->z + r.z + u.z},

			{center->x - r.x - u.x, center->y - r.y - u.y, center->z - r.z - u.z},
			{center->x + r.x + u.x, center->y + r.y + u.y, center->z + r.z + u.z},
			{center->x - r.x + u.x, center->y - r.y + u.y, center->z - r.z + u.z},
		};

		u32 colors[2];
		for (u32 i = 0; i < 2; i++)
		{
			colors[i] = color;
		}

		TrianglesColor3d::addTriangles(2, vtx, nullptr, colors, true);
	}

	void drawScreenQuadOutline(const Vec3f* center, f32 side, f32 lineWidth, u32 color)
	{
		const f32 w = side * 0.5f;
		const Vec3f r = { s_camera.viewMtx.m0.x * w, s_camera.viewMtx.m0.y * w, s_camera.viewMtx.m0.z * w };
		const Vec3f u = { s_camera.viewMtx.m1.x * w, s_camera.viewMtx.m1.y * w, s_camera.viewMtx.m1.z * w };

		const Vec3f lines[] =
		{
			{center->x - r.x - u.x, center->y - r.y - u.y, center->z - r.z - u.z}, {center->x + r.x - u.x, center->y + r.y - u.y, center->z + r.z - u.z},
			{center->x + r.x - u.x, center->y + r.y - u.y, center->z + r.z - u.z}, {center->x + r.x + u.x, center->y + r.y + u.y, center->z + r.z + u.z},
			{center->x + r.x + u.x, center->y + r.y + u.y, center->z + r.z + u.z}, {center->x - r.x + u.x, center->y - r.y + u.y, center->z - r.z + u.z},
			{center->x - r.x + u.x, center->y - r.y + u.y, center->z - r.z + u.z}, {center->x - r.x - u.x, center->y - r.y - u.y, center->z - r.z - u.z},
		};

		u32 colors[4];
		for (u32 i = 0; i < 4; i++)
		{
			colors[i] = color;
		}

		LineDraw3d::addLines(4, lineWidth, lines, colors);
	}

	void drawBox(const Vec3f* center, f32 side, f32 lineWidth, u32 color)
	{
		const f32 w = side * 0.5f;
		const Vec3f lines[]=
		{
			{center->x - w, center->y - w, center->z - w}, {center->x + w, center->y - w, center->z - w},
			{center->x + w, center->y - w, center->z - w}, {center->x + w, center->y - w, center->z + w},
			{center->x + w, center->y - w, center->z + w}, {center->x - w, center->y - w, center->z + w},
			{center->x - w, center->y - w, center->z + w}, {center->x - w, center->y - w, center->z - w},

			{center->x - w, center->y + w, center->z - w}, {center->x + w, center->y + w, center->z - w},
			{center->x + w, center->y + w, center->z - w}, {center->x + w, center->y + w, center->z + w},
			{center->x + w, center->y + w, center->z + w}, {center->x - w, center->y + w, center->z + w},
			{center->x - w, center->y + w, center->z + w}, {center->x - w, center->y + w, center->z - w},

			{center->x - w, center->y - w, center->z - w}, {center->x - w, center->y + w, center->z - w},
			{center->x + w, center->y - w, center->z - w}, {center->x + w, center->y + w, center->z - w},
			{center->x + w, center->y - w, center->z + w}, {center->x + w, center->y + w, center->z + w},
			{center->x - w, center->y - w, center->z + w}, {center->x - w, center->y + w, center->z + w},
		};
		u32 colors[12];
		for (u32 i = 0; i < 12; i++)
		{
			colors[i] = color;
		}

		LineDraw3d::addLines(12, lineWidth, lines, colors);
	}

	void drawBounds(const Vec3f* center, const Vec3f* ext, f32 lineWidth, u32 color)
	{
		const Vec3f lines[] =
		{
			{center->x - ext->x, center->y + ext->y, center->z - ext->z}, {center->x + ext->x, center->y + ext->y, center->z - ext->z},
			{center->x + ext->x, center->y + ext->y, center->z - ext->z}, {center->x + ext->x, center->y + ext->y, center->z + ext->z},
			{center->x + ext->x, center->y + ext->y, center->z + ext->z}, {center->x - ext->x, center->y + ext->y, center->z + ext->z},
			{center->x - ext->x, center->y + ext->y, center->z + ext->z}, {center->x - ext->x, center->y + ext->y, center->z - ext->z},

			{center->x - ext->x, center->y - ext->y, center->z - ext->z}, {center->x + ext->x, center->y - ext->y, center->z - ext->z},
			{center->x + ext->x, center->y - ext->y, center->z - ext->z}, {center->x + ext->x, center->y - ext->y, center->z + ext->z},
			{center->x + ext->x, center->y - ext->y, center->z + ext->z}, {center->x - ext->x, center->y - ext->y, center->z + ext->z},
			{center->x - ext->x, center->y - ext->y, center->z + ext->z}, {center->x - ext->x, center->y - ext->y, center->z - ext->z},

			{center->x - ext->x, center->y + ext->y, center->z - ext->z}, {center->x - ext->x, center->y - ext->y, center->z - ext->z},
			{center->x + ext->x, center->y + ext->y, center->z - ext->z}, {center->x + ext->x, center->y - ext->y, center->z - ext->z},
			{center->x + ext->x, center->y + ext->y, center->z + ext->z}, {center->x + ext->x, center->y - ext->y, center->z + ext->z},
			{center->x - ext->x, center->y + ext->y, center->z + ext->z}, {center->x - ext->x, center->y - ext->y, center->z + ext->z},
		};
		u32 colors[12];
		for (u32 i = 0; i < 12; i++)
		{
			colors[i] = color;
		}

		LineDraw3d::addLines(12, lineWidth, lines, colors);
	}
		
	void drawSector3d(const EditorSector* sector, const SectorTriangles* poly, bool overlay, bool hover)
	{
		// Draw the floor and ceiling polygons.
		const u32 triCount = poly->count;
		const Vec2f* vtx = poly->vtx.data();

		u32 color[4];

		if (overlay)
		{
			for (u32 t = 0; t < 4; t++) { color[t] = hover ? 0x40ff8020 : 0x40ff4020; }
		}
		else if (s_sectorDrawMode != SDM_WIREFRAME)
		{
			u32 L = u32(sector->ambient);
			L = s_fullbright ? 255u : std::max(0u, std::min(CONV_5bitTo8bit(L), 255u));
			const u32 Lrg = s_sectorDrawMode == SDM_LIGHTING ? L * 7 / 8 : L;
			const u32 lighting = Lrg | (Lrg << 8u) | (L << 16u) | 0xff000000;

			for (u32 t = 0; t < 4; t++) { color[t] = lighting; }
		}
		else
		{
			for (u32 t = 0; t < 4; t++) { color[t] = 0xff1a0f0d; }
		}

		if (s_sectorDrawMode == SDM_WIREFRAME || s_sectorDrawMode == SDM_LIGHTING || overlay)
		{
			drawSectorFloorAndCeilColor(sector, poly, color, overlay);
		}
		else
		{
			drawSectorFloorAndCeilTextured(sector, poly, color[0]);
		}

		// Draw quads for each wall.
		const u32 wallCount = (u32)sector->walls.size();
		const EditorWall* wall = sector->walls.data();
		vtx = sector->vertices.data();
		for (u32 w = 0; w < wallCount; w++, wall++)
		{
			if (overlay)
			{
				for (u32 t = 0; t < 4; t++) { color[t] = hover ? 0x40ff8020 : 0x40ff4020;; }
			}
			else if (s_sectorDrawMode != SDM_WIREFRAME)
			{
				const s32 wallLight = sector->ambient < 31 ? wall->light : 0;

				u32 L = u32(std::max(sector->ambient + wallLight, 0));
				L = s_fullbright ? 255u : std::max(0u, std::min(CONV_5bitTo8bit(L), 255u));
				const u32 Lrg = s_sectorDrawMode == SDM_LIGHTING ? L * 7 / 8 : L;
				const u32 lighting = Lrg | (Lrg << 8u) | (L << 16u) | 0xff000000;

				for (u32 t = 0; t < 4; t++) { color[t] = lighting; }
			}
			else
			{
				for (u32 t = 0; t < 4; t++) { color[t] = 0xff1a0f0d; }
			}

			if (s_sectorDrawMode == SDM_WIREFRAME || s_sectorDrawMode == SDM_LIGHTING || overlay)
			{
				drawWallColor(sector, vtx, wall, color, overlay);
			}
			else
			{
				drawWallTexture(sector, vtx, wall, w, color);
			}
		}
	}

	void highlightWall3d(bool selected, f32 width)
	{
		if ((selected && s_selectedWall < 0) || (!selected && s_hoveredWall < 0)) { return; }

		s32 sectorId = selected ? s_selectedWallSector : s_hoveredWallSector;
		s32 wallId = selected ? s_selectedWall : s_hoveredWall;
		RayHitPart part = selected ? s_selectWallPart : s_hoveredWallPart;
		const u32 color = selected ? 0xffffA040 : 0xffffC080;
		const u32 colors[] = { color, color, color, color };

		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const EditorWall* wall = sector->walls.data() + wallId;
		const Vec2f* vtx = sector->vertices.data();

		const EditorSector* next = wall->adjoin >= 0 ? s_levelData->sectors.data() + wall->adjoin : nullptr;
		f32 floorHeight, ceilHeight;
		if (part == HIT_PART_MID || part == HIT_PART_UNKNOWN || !next)
		{
			floorHeight = next ? std::min(sector->floorAlt, next->floorAlt) : sector->floorAlt;
			ceilHeight = next ? std::max(sector->ceilAlt, next->ceilAlt) : sector->ceilAlt;
		}
		else if (part == HIT_PART_BOT)
		{
			floorHeight = sector->floorAlt;
			ceilHeight = next->floorAlt;
		}
		else if (part == HIT_PART_TOP)
		{
			floorHeight = next->ceilAlt;
			ceilHeight = sector->ceilAlt;
		}

		const Vec2f* v0 = &vtx[wall->i0];
		const Vec2f* v1 = &vtx[wall->i1];

		Vec3f lines[8]=
		{
			{v0->x, ceilHeight, v0->z}, {v1->x, ceilHeight, v1->z},
			{v1->x, ceilHeight, v1->z}, {v1->x, floorHeight, v1->z},
			{v1->x, floorHeight, v1->z}, {v0->x, floorHeight, v0->z},
			{v0->x, floorHeight, v0->z}, {v0->x, ceilHeight, v0->z}
		};
		LineDraw3d::addLines(4, width, lines, colors);
	}

	void drawInfWalls3d(f32 width, u32 color)
	{
		const u32 colors[] = { color, color, color, color };

		const u32 count = (u32)s_infWalls.size();
		const InfWall* infWall = s_infWalls.data();
		for (u32 w = 0; w < count; w++, infWall++)
		{
			const EditorSector* sector = infWall->sector;
			const EditorWall* wall = sector->walls.data() + infWall->wallId;
			const Vec2f* vtx = sector->vertices.data();

			// TODO: Draw based on trigger type?
			f32 y0, y1;
			if (infWall->triggerType == TRIGGER_LINE && wall->adjoin >= 0)
			{
				const EditorSector* next = s_levelData->sectors.data() + wall->adjoin;
				y0 = std::min(next->floorAlt, sector->floorAlt);
				y1 = std::max(next->ceilAlt, sector->ceilAlt);
			}
			else
			{
				// Switch of some type.
				if (wall->adjoin < 0)
				{
					const f32 h = sector->floorAlt - sector->ceilAlt;
					y0 = sector->floorAlt;
					y1 = sector->ceilAlt;
				}
				else
				{
					const EditorSector* next = s_levelData->sectors.data() + wall->adjoin;

					if (next->floorAlt < sector->floorAlt)  // lower
					{
						y0 = next->floorAlt;
						y1 = sector->floorAlt;
					}
					else if (next->ceilAlt > sector->ceilAlt)	// upper
					{
						y0 = next->ceilAlt;
						y1 = sector->ceilAlt;
					}
					else // mid
					{
						y0 = sector->floorAlt;
						y1 = sector->ceilAlt;
					}
				}
			}
			Vec3f lines[8];
			lines[0] = { vtx[wall->i0].x, y0, vtx[wall->i0].z };
			lines[1] = { vtx[wall->i1].x, y0, vtx[wall->i1].z };

			lines[2] = { vtx[wall->i1].x, y0, vtx[wall->i1].z };
			lines[3] = { vtx[wall->i1].x, y1, vtx[wall->i1].z };

			lines[4] = { vtx[wall->i1].x, y1, vtx[wall->i1].z };
			lines[5] = { vtx[wall->i0].x, y1, vtx[wall->i0].z };

			lines[6] = { vtx[wall->i0].x, y1, vtx[wall->i0].z };
			lines[7] = { vtx[wall->i0].x, y0, vtx[wall->i0].z };

			LineDraw3d::addLines(4, width, lines, colors);
		}
	}

	void drawSector3d_Lines(const EditorSector* sector, f32 width, u32 color, bool overlay, bool hover)
	{
		const u32 colors[] = { color, color, color, color, color };

		// Draw the walls
		const u32 wallCount = (u32)sector->walls.size();
		const EditorWall* wall = sector->walls.data();
		const Vec2f* vtx = sector->vertices.data();
		for (u32 w = 0; w < wallCount; w++, wall++)
		{
			if (s_showSectorColors && wall->infType != INF_NONE)
			{
				// Only add the wall if we are on the "correct" side.
				Vec2f dir = { vtx[wall->i1].x - vtx[wall->i0].x, vtx[wall->i1].z - vtx[wall->i0].z };
				Vec2f camDir = { s_camera.pos.x - vtx[wall->i0].x, s_camera.pos.z - vtx[wall->i0].z };
				Vec2f nrm = { -dir.z, dir.x };
				if (nrm.x*camDir.x + nrm.z*camDir.z < 0.0f)
				{
					s_infWalls.push_back({ sector, w, (u32)wall->triggerType });
				}
			}

			// Draw the floor piece.
			Vec3f lines[10];
			// floor
			lines[0] = { vtx[wall->i0].x, sector->floorAlt, vtx[wall->i0].z };
			lines[1] = { vtx[wall->i1].x, sector->floorAlt, vtx[wall->i1].z };
			// ceiling
			u32 count = 1;
			if (wall->adjoin < 0 || s_levelData->sectors[wall->adjoin].ceilAlt != sector->ceilAlt || s_camera.pos.y > sector->ceilAlt)
			{
				lines[2] = { vtx[wall->i0].x, sector->ceilAlt, vtx[wall->i0].z };
				lines[3] = { vtx[wall->i1].x, sector->ceilAlt, vtx[wall->i1].z };
				count++;
			}

			if (wall->adjoin < 0)
			{
				lines[count * 2 + 0] = { vtx[wall->i0].x, sector->floorAlt, vtx[wall->i0].z };
				lines[count * 2 + 1] = { vtx[wall->i0].x, sector->ceilAlt, vtx[wall->i0].z };
				count++;

				if (overlay || hover || color != 0xffffffff)
				{
					lines[count * 2 + 0] = { vtx[wall->i1].x, sector->floorAlt, vtx[wall->i1].z };
					lines[count * 2 + 1] = { vtx[wall->i1].x, sector->ceilAlt,  vtx[wall->i1].z };
					count++;
				}
			}
			else
			{
				// lower
				f32 nextFloorAlt = s_levelData->sectors[wall->adjoin].floorAlt;
				if (nextFloorAlt < sector->floorAlt)
				{
					lines[count * 2 + 0] = { vtx[wall->i0].x, sector->floorAlt, vtx[wall->i0].z };
					lines[count * 2 + 1] = { vtx[wall->i0].x, nextFloorAlt, vtx[wall->i0].z };
					count++;
				}

				// upper
				f32 nextCeilAlt = s_levelData->sectors[wall->adjoin].ceilAlt;
				if (nextCeilAlt > sector->ceilAlt)
				{
					lines[count * 2 + 0] = { vtx[wall->i0].x, sector->ceilAlt, vtx[wall->i0].z };
					lines[count * 2 + 1] = { vtx[wall->i0].x, nextCeilAlt, vtx[wall->i0].z };
					count++;
				}
			}
			LineDraw3d::addLines(count, width, lines, colors);
		}
	}

	void drawLineToTarget3d(const Vec3f* p0, s32 sectorId, s32 wallId, f32 width, u32 color)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const Vec2f* vtx = sector->vertices.data();
		Vec3f target = { 0 };
		target.y = (sector->floorAlt + sector->ceilAlt) * 0.5f;

		if (wallId >= 0)
		{
			const EditorWall* wall = sector->walls.data() + wallId;
			target.x = (vtx[wall->i0].x + vtx[wall->i1].x) * 0.5f;
			target.z = (vtx[wall->i0].z + vtx[wall->i1].z) * 0.5f;
		}
		else
		{
			const u32 vtxCount = (u32)sector->vertices.size();
			for (u32 v = 0; v < vtxCount; v++)
			{
				target.x += vtx[v].x;
				target.z += vtx[v].z;
			}
			const f32 scale = 1.0f / f32(vtxCount);
			target.x *= scale;
			target.z *= scale;
		}
		Vec3f line[] = { *p0, target };
		LineDraw3d::addLine(width, line, &color);
	}

	// If the selected wall or sector has an INF script, draw lines towards
	// 1. Any objects it effects (targets and clients).
	// 2. Any slaves (sector).
	void drawTargetsAndClientLines3d(s32 sectorId, f32 width)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const EditorInfItem* item = sector->infItem;
		if (!item) { return; }

		Vec3f lineStart = { 0 };
		lineStart.y = (sector->floorAlt + sector->ceilAlt) * 0.5f;
		const Vec2f* vtx = sector->vertices.data();
		const u32 vtxCount = (u32)sector->vertices.size();
		for (u32 v = 0; v < vtxCount; v++)
		{
			lineStart.x += vtx[v].x;
			lineStart.z += vtx[v].z;
		}
		const f32 scale = 1.0f / f32(vtxCount);
		lineStart.x *= scale;
		lineStart.z *= scale;

		for (u32 c = 0; c < (u32)item->classData.size(); c++)
		{
			const EditorInfClassData* cdata = &item->classData[c];
			u32 stopCount = 0;
			if (cdata->iclass == INF_CLASS_TRIGGER)
			{
				stopCount = 1;
			}
			else if (cdata->iclass == INF_CLASS_ELEVATOR)
			{
				stopCount = (u32)cdata->stop.size();
			}

			for (u32 s = 0; s < stopCount; s++)
			{
				const u32 funcCount = (u32)cdata->stop[s].func.size();
				for (u32 f = 0; f < funcCount; f++)
				{
					const u32 funcId = cdata->stop[s].func[f].funcId;
					const u32 clientCount = (u32)cdata->stop[s].func[f].client.size();
					for (u32 cl = 0; cl < clientCount; cl++)
					{
						const EditorSector* clientSector = LevelEditorData::getSector(cdata->stop[s].func[f].client[cl].sectorName.c_str());
						const s32 clientWallId = cdata->stop[s].func[f].client[cl].wallId;
						if (!clientSector) { continue; }

						if (clientWallId >= 0)
						{
							drawLineToTarget3d(&lineStart, clientSector->id, clientWallId, 1.5f * width, 0xffff2020);
						}
						else
						{
							drawLineToTarget3d(&lineStart, clientSector->id, -1, 1.5f * width, 0xffff2020);
						}
					}

					// Draw to the adjoining lines.
					if (funcId == INF_MSG_ADJOIN)
					{
						drawLineToTarget3d(&lineStart, LevelEditorData::getSector(cdata->stop[s].func[f].arg[0].sValue.c_str())->id, cdata->stop[s].func[f].arg[1].iValue, 1.5f * width, 0xff2020ff);
						drawLineToTarget3d(&lineStart, LevelEditorData::getSector(cdata->stop[s].func[f].arg[2].sValue.c_str())->id, cdata->stop[s].func[f].arg[3].iValue, 1.5f * width, 0xff2020ff);
					}
				}
			}
		}
	}

	void drawTargetsAndClientLines3d(s32 sectorId, s32 wallId, f32 width)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const Vec2f* vtx = sector->vertices.data();
		const EditorWall* wall = sector->walls.data() + wallId;
		const EditorInfItem* item = wall->infItem;
				
		Vec3f lineStart;
		lineStart.x = (vtx[wall->i0].x + vtx[wall->i1].x) * 0.5f;
		lineStart.y = (sector->floorAlt + sector->ceilAlt) * 0.5f;
		lineStart.z = (vtx[wall->i0].z + vtx[wall->i1].z) * 0.5f;

		for (u32 c = 0; c < (u32)item->classData.size(); c++)
		{
			const EditorInfClassData* cdata = &item->classData[c];
			u32 stopCount = 0;
			if (cdata->iclass == INF_CLASS_TRIGGER)
			{
				stopCount = 1;
			}
			else if (cdata->iclass == INF_CLASS_ELEVATOR)
			{
				stopCount = (u32)cdata->stop.size();
			}

			for (u32 s = 0; s < stopCount; s++)
			{
				const u32 funcCount = (u32)cdata->stop[s].func.size();
				for (u32 f = 0; f < funcCount; f++)
				{
					const u32 clientCount = (u32)cdata->stop[s].func[f].client.size();
					for (u32 cl = 0; cl < clientCount; cl++)
					{
						const EditorSector* clientSector = LevelEditorData::getSector(cdata->stop[s].func[f].client[cl].sectorName.c_str());
						const s32 clientWallId = cdata->stop[s].func[f].client[cl].wallId;
						if (!clientSector) { continue; }

						if (clientWallId >= 0)
						{
							drawLineToTarget3d(&lineStart, clientSector->id, clientWallId, 1.5f * width, 0xffff2020);
						}
						else
						{
							drawLineToTarget3d(&lineStart, clientSector->id, -1, 1.5f * width, 0xffff2020);
						}
					}
				}
			}
		}
	}

	void drawTargetsAndClientSlaves3d(s32 sectorId, f32 width)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const EditorInfItem* item = sector->infItem;
		if (!item) { return; }

		Vec3f lineStart = { 0 };
		lineStart.y = (sector->floorAlt + sector->ceilAlt) * 0.5f;
		const Vec2f* vtx = sector->vertices.data();
		const u32 vtxCount = (u32)sector->vertices.size();
		for (u32 v = 0; v < vtxCount; v++)
		{
			lineStart.x += vtx[v].x;
			lineStart.z += vtx[v].z;
		}
		const f32 scale = 1.0f / f32(vtxCount);
		lineStart.x *= scale;
		lineStart.z *= scale;

		for (u32 c = 0; c < (u32)item->classData.size(); c++)
		{
			const EditorInfClassData* cdata = &item->classData[c];
			for (u32 s = 0; s < (u32)cdata->slaves.size(); s++)
			{
				const EditorSector* slaveSector = LevelEditorData::getSector(cdata->slaves[s].c_str());
				if (!slaveSector) { continue; }

				drawLineToTarget3d(&lineStart, slaveSector->id, -1, 1.25f * width, 0xff20ff20);
			}
		}
	}

	void drawLevel3d(u32 rtWidth, u32 rtHeight)
	{
		Vec3f upDir = { 0.0f, 1.0f, 0.0f };
		Vec3f lookDir = { sinf(s_camera.yaw) * cosf(s_camera.pitch), sinf(s_camera.pitch), cosf(s_camera.yaw) * cosf(s_camera.pitch) };

		s_camera.viewMtx = DXL2_Math::computeViewMatrix(&lookDir, &upDir);
		s_camera.projMtx = DXL2_Math::computeProjMatrix(c_editorCameraFov, f32(rtWidth) / f32(rtHeight), 0.01f, 1000.0f);
		s_camera.invProj = DXL2_Math::computeInvProjMatrix(s_camera.projMtx);

		// Get the current sector (top-down only)
		const s32 layer = s_layerIndex + s_layerMin;
		const Vec2f topDownPos = { s_camera.pos.x, s_camera.pos.z };
		s32 overSector = LevelEditorData::findSector(layer, &topDownPos);

		f32 pixelSize = 1.0f / (f32)rtHeight;
		if (!s_levelData)
		{
			Grid3d::draw(s_gridSize, s_subGridSize, 1.0f, pixelSize, &s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx);
		}

		if (!s_levelData) { return; }
		u32 sectorCount = (u32)s_levelData->sectors.size();
		EditorSector* sector = s_levelData->sectors.data();
		// Draw the sector faces.
		u32 color[] = { 0xff1a0f0d, 0xff1a0f0d, 0xff1a0f0d, 0xff1a0f0d };
		for (u32 i = 0; i < sectorCount; i++, sector++)
		{
			if (sector->layer != layer) { continue; }
			drawSector3d(sector, &sector->triangles);
		}
		TrianglesColor3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx);

		const f32 width = 3.0f / f32(rtHeight);
		// Walls
		if (s_showSectorColors)
		{
			const u32 sectorClr[] = { 0xffffffff, 0xffff80ff, 0xffffff80 };
			s_infWalls.clear();
			for (u32 i = 0; i < INF_COUNT; i++)
			{
				sector = s_levelData->sectors.data();
				for (u32 s = 0; s < sectorCount; s++, sector++)
				{
					if (sector->layer != layer || sector->infType != i) { continue; }
					drawSector3d_Lines(sector, width, sectorClr[i]);
				}
			}
			drawInfWalls3d(width, 0xff2020ff);
		}
		else
		{
			sector = s_levelData->sectors.data();
			for (u32 i = 0; i < sectorCount; i++, sector++)
			{
				if (sector->layer != layer) { continue; }
				drawSector3d_Lines(sector, width, 0xffffffff);
			}
		}

		// Debug to show the 3d cursor (should be hidden).
		//drawBox(&s_cursor3d, 1.0f, 3.0f / f32(rtHeight));
		LineDraw3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx);

		// Draw selected or hovered elements.
		if (s_editMode == LEDIT_SECTOR && (s_hoveredSector >= 0 || s_selectedSector >= 0))
		{
			// Draw solid sector.
			if (s_selectedSector >= 0)
			{
				sector = s_levelData->sectors.data() + s_selectedSector;
				drawSector3d(sector, &sector->triangles, true, false);
			}
			if (s_hoveredSector >= 0)
			{
				sector = s_levelData->sectors.data() + s_hoveredSector;
				drawSector3d(sector, &sector->triangles, true, true);
			}
			TrianglesColor3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx);

			// Draw outline (no depth buffer).
			if (s_selectedSector >= 0)
			{
				sector = s_levelData->sectors.data() + s_selectedSector;
				drawSector3d_Lines(sector, width * 1.25f, 0xffffA040);
			}
			if (s_hoveredSector >= 0)
			{
				sector = s_levelData->sectors.data() + s_hoveredSector;
				drawSector3d_Lines(sector, width * 1.25f, 0xffffC080);
			}
			LineDraw3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, false);
		}
		if (s_editMode == LEDIT_WALL && (s_hoveredWall >= 0 || s_selectedWall >= 0))
		{
			// Draw solid wall.
			if (s_selectedWall >= 0)
			{
				u32 color[] = { 0x40ff4020, 0x40ff4020 };
				sector = s_levelData->sectors.data() + s_selectedWallSector;
				drawWallColor(sector, sector->vertices.data(), sector->walls.data() + s_selectedWall, color, true, s_selectWallPart);
			}
			if (s_hoveredWall >= 0)
			{
				u32 color[] = { 0x40ff8020, 0x40ff8020 };
				sector = s_levelData->sectors.data() + s_hoveredWallSector;
				drawWallColor(sector, sector->vertices.data(), sector->walls.data() + s_hoveredWall, color, true, s_hoveredWallPart);
			}
			TrianglesColor3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx);

			// Draw the selected sector before drawing the walls.
			if (s_selectedWall >= 0 || s_hoveredWall >= 0)
			{
				highlightWall3d(true, width*1.25f);
				highlightWall3d(false, width*1.25f);
			}
			LineDraw3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, false);
		}
		if (s_editMode == LEDIT_VERTEX && (s_hoveredVertex >= 0 || s_selectedVertex >= 0))
		{
			// Draw the selected sector before drawing the walls.
			if (s_selectedVertex >= 0 || s_hoveredVertex >= 0)
			{
				if (s_selectedVertex >= 0)
				{					
					sector = s_levelData->sectors.data() + s_selectedVertexSector;
					const Vec2f* vtx = sector->vertices.data() + s_selectedVertex;
					const Vec3f pos = { vtx->x, s_selectVertexPart == HIT_PART_FLOOR ? sector->floorAlt : sector->ceilAlt , vtx->z };

					const f32 distFromCam = DXL2_Math::distance(&pos, &s_camera.pos);
					const f32 size = distFromCam * 16.0f / f32(rtHeight);

					// Add a screen aligned quad.
					drawScreenQuad(&pos, size, 0xA0ff4020);
					drawScreenQuadOutline(&pos, size, width, 0xffffA040);
				}
				if (s_hoveredVertex >= 0)
				{
					sector = s_levelData->sectors.data() + s_hoveredVertexSector;
					const Vec2f* vtx = sector->vertices.data() + s_hoveredVertex;
					const Vec3f pos = { vtx->x, s_hoveredVertexPart == HIT_PART_FLOOR ? sector->floorAlt : sector->ceilAlt , vtx->z };

					const f32 distFromCam = DXL2_Math::distance(&pos, &s_camera.pos);
					const f32 size = distFromCam * 16.0f / f32(rtHeight);

					drawScreenQuad(&pos, size, 0xA0ff8020);
					drawScreenQuadOutline(&pos, size, width, 0xffffC080);
				}
			}
			TrianglesColor3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, false);
			LineDraw3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, false);
		}

		if (s_selectedSector >= 0 && s_levelData->sectors[s_selectedSector].infType != INF_NONE)
		{
			// If the selected wall or sector has an INF script, draw lines towards
			drawTargetsAndClientLines3d(s_selectedSector, width);
			drawTargetsAndClientSlaves3d(s_selectedSector, width);
		}
		else if (s_selectedWall >= 0 && s_levelData->sectors[s_selectedWallSector].walls[s_selectedWall].infType != INF_NONE)
		{
			drawTargetsAndClientLines3d(s_selectedWallSector, s_selectedWall, width);
		}
		LineDraw3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, false);

		// Objects
		u32 alphaFg = s_editMode == LEDIT_ENTITY ? 0xff000000 : 0xA0000000;
		u32 alphaBg = s_editMode == LEDIT_ENTITY ? 0xA0000000 : 0x60000000;
		sector = s_levelData->sectors.data();
		for (u32 i = 0; i < sectorCount; i++, sector++)
		{
			if (sector->layer != layer) { continue; }

			// For now just draw a quad + image.
			const u32 objCount = (u32)sector->objects.size();
			const EditorLevelObject* obj = sector->objects.data();
			for (u32 o = 0; o < objCount; o++, obj++)
			{
				f32 bwidth = 3.0f / f32(rtHeight);
				u32 bcolor = 0x00ffd7a4 | alphaBg;
				const bool selected = s_selectedEntity == o && s_selectedEntitySector == i;
				if ((s_hoveredEntity == o && s_hoveredEntitySector == i) || selected)
				{
					bwidth *= 1.25f;
					bcolor = 0x00ffe7b4 | alphaFg;
				}

				if (obj->oclass == CLASS_3D)
				{
					DXL2_EditorRender::drawModel3d(sector, obj->displayModel, &obj->pos, &obj->rotMtx, s_palette->colors, alphaFg);
					DXL2_EditorRender::drawModel3d_Bounds(obj->displayModel, &obj->pos, &obj->rotMtx, bwidth, bcolor);
				}
				else
				{
					// Draw the bounding box.
					drawBounds(&obj->worldCen, &obj->worldExt, bwidth, bcolor);

					if (obj->display)
					{
						const Vec3f r = s_camera.viewMtx.m0;
						const Vec3f u = { 0.0f, 1.0f, 0.0f };
						const Vec3f vtx[] =
						{
							{obj->worldCen.x - obj->worldExt.x * r.x, obj->worldCen.y - obj->worldExt.x * r.y - obj->worldExt.y * u.y, obj->worldCen.z - obj->worldExt.z * r.z },
							{obj->worldCen.x + obj->worldExt.x * r.x, obj->worldCen.y + obj->worldExt.x * r.y - obj->worldExt.y * u.y, obj->worldCen.z + obj->worldExt.z * r.z },
							{obj->worldCen.x + obj->worldExt.x * r.x, obj->worldCen.y + obj->worldExt.x * r.y + obj->worldExt.y * u.y, obj->worldCen.z + obj->worldExt.z * r.z },

							{obj->worldCen.x - obj->worldExt.x * r.x, obj->worldCen.y - obj->worldExt.x * r.y - obj->worldExt.y * u.y, obj->worldCen.z - obj->worldExt.z * r.z },
							{obj->worldCen.x + obj->worldExt.x * r.x, obj->worldCen.y + obj->worldExt.x * r.y + obj->worldExt.y * u.y, obj->worldCen.z + obj->worldExt.z * r.z },
							{obj->worldCen.x - obj->worldExt.x * r.x, obj->worldCen.y - obj->worldExt.x * r.y + obj->worldExt.y * u.y, obj->worldCen.z - obj->worldExt.z * r.z },
						};

						const Vec2f uv[6] =
						{
							{0.0f, 0.0f},
							{1.0f, 0.0f},
							{1.0f, 1.0f},

							{0.0f, 0.0f},
							{1.0f, 1.0f},
							{0.0f, 1.0f},
						};
						u32 baseRGB = selected ? 0x00ffa0a0 : 0x00ffffff;
						u32 colorFg[2] = { baseRGB | alphaFg, baseRGB | alphaFg };
						TrianglesColor3d::addTexturedTriangles(2, vtx, uv, uv, colorFg, obj->display->texture, TrianglesColor3d::TRANS_BLEND_CLAMP);

						// Draw a direction.
						const f32 angle = obj->orientation.y * PI / 180.0f;
						const Vec3f dir = { sinf(angle), 0.0f, cosf(angle) };

						// Make an arrow.
						Vec3f dP  = { dir.x * obj->worldExt.x, obj->worldExt.y, dir.z * obj->worldExt.z };
						Vec3f cen = { obj->worldCen.x + dP.x, obj->worldCen.y + dP.y, obj->worldCen.z + dP.z };
						Vec3f tan = { -dir.z * obj->worldExt.x * 0.5f, 0.0f, dir.x * obj->worldExt.z * 0.5f };
						// Make sure the direction arrow is visible.
						cen.y = std::min(cen.y, sector->floorAlt - 0.1f);

						Vec3f arrowVtx[] =
						{
							cen, {cen.x + tan.x - dP.x * 0.25f, cen.y, cen.z + tan.z - dP.z * 0.25f},
							cen, {cen.x - tan.x - dP.x * 0.25f, cen.y, cen.z - tan.z - dP.z * 0.25f},
						};

						const bool highlight = (s_hoveredEntity == o && s_hoveredEntitySector == i) || selected;
						const u32 alphaArrow = s_editMode == LEDIT_ENTITY ? (highlight ? 0xA0000000 : 0x60000000) : 0x30000000;
						u32 baseRGBArrow = 0x00ffff00;
						u32 baseClrArrow = baseRGBArrow | alphaArrow;
						u32 color[] = { baseClrArrow, baseClrArrow, baseClrArrow, baseClrArrow };
						LineDraw3d::addLines(2, bwidth, arrowVtx, color);
					}
				}
			}
		}
		TrianglesColor3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, true);
		LineDraw3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, true, false);
		
		if (overSector < 0)
		{
			Grid3d::draw(s_gridSize, s_subGridSize, 1.0f, pixelSize, &s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx);
		}
	}

	void draw(bool* isActive)
	{
		if (s_runLevel)
		{
			return;
		}

		// Draw the tool bar.
		toolbarBegin();
		{
			if (drawToolbarButton(s_editCtrlToolbarData, 0, false))
			{
				bool res = LevelEditorData::generateLevelData();
				res &= LevelEditorData::generateInfAsset();
				res &= LevelEditorData::generateObjects();
				if (res)
				{
					Vec2f pos2d = { s_camera.pos.x, s_camera.pos.z };
					s32 sectorId = LevelEditorData::findSector(s_layerIndex + s_layerMin, &pos2d);
					LevelObjectData* levelObj = DXL2_LevelObjects::getLevelObjectData();

					u32 w = 320, h = 200;
					if (DXL2_GameLoop::startLevelFromExisting(&s_camera.pos, -s_camera.yaw + PI, sectorId, s_palette, levelObj, s_renderer, w, h))
					{
						s_runLevel = true;
						DXL2_Input::enableRelativeMode(true);
						s_renderer->enableScreenClear(false);
					}
				}
			}
			ImGui::SameLine(0.0f, 32.0f);

			for (u32 i = 1; i < 6; i++)
			{
				if (drawToolbarButton(s_editCtrlToolbarData, i, i == s_editMode))
				{
					s_editMode = i;
					s_enableInfEditor = false;
				}
				ImGui::SameLine();
			}
			ImGui::SameLine(0.0f, 32.0f);

			for (u32 i = 0; i < 6; i++)
			{
				if (drawToolbarButton(s_booleanToolbarData, i, i == s_boolMode))
				{
					s_boolMode = i;
				}
				ImGui::SameLine();
			}

			// TODO: Toolbar for rendering mode:
			// Wireframe, Shaded, Textured, Textured & Shaded

			ImGui::SameLine(0.0f, 32.0f);
			ImGui::PushItemWidth(64.0f);
			if (ImGui::Combo("Grid Size", &s_gridIndex, c_gridSizes, IM_ARRAYSIZE(c_gridSizes)))
			{
				s_gridSize = c_gridSizeMap[s_gridIndex];
			}
			ImGui::PopItemWidth();
			// Get the "Grid Size" combo position to align the message panel later.
			const ImVec2 itemPos = ImGui::GetItemRectMin();
						
			ImGui::SameLine(0.0f, 32.0f);
			ImGui::PushItemWidth(64.0f);
			ImGui::Combo("Layer", &s_layerIndex, s_layerStr, IM_ARRAYSIZE(s_layerStr));
			ImGui::PopItemWidth();

			// Message Panel
			messagePanel(itemPos);
		}
		toolbarEnd();
		
		// Draw the info bars.
		s_infoHeight = 486;

		infoToolBegin(s_infoHeight);
		{
			if (s_hoveredVertex >= 0 || s_selectedVertex >= 0)
			{
				infoPanelVertex();
			}
			else if (s_hoveredSector >= 0 || s_selectedSector >= 0)
			{
				infoPanelSector();
			}
			else if (s_hoveredWall >= 0 || s_selectedWall >= 0)
			{
				infoPanelWall();
			}
			else if (s_hoveredEntity >= 0 || s_selectedEntity >= 0)
			{
				infoPanelEntity();
			}
			else
			{
				infoPanelMap();
			}
		}
		infoToolEnd();
		// Browser
		browserBegin(s_infoHeight);
		{
			if (s_editMode == LEDIT_ENTITY)
			{
				browseEntities();
			}
			else
			{
				browseTextures();
			}
		}
		browserEnd();

		// Draw the grid.
		bool recreateRendertarget = !s_view3d;
		DisplayInfo displayInfo;
		DXL2_RenderBackend::getDisplayInfo(&displayInfo);
		const u32 curWidth  = displayInfo.width - 480 - 16;
		const u32 curHeight = displayInfo.height - 68 - 18;
		if (s_view3d)
		{
			u32 width, height;
			DXL2_RenderBackend::getRenderTargetDim(s_view3d, &width, &height);
												
			if (curWidth != width || curHeight != height)
			{
				DXL2_RenderBackend::freeRenderTarget(s_view3d);
				s_view3d = nullptr;
				recreateRendertarget = true;
			}
		}
		if (recreateRendertarget)
		{
			s_view3d = DXL2_RenderBackend::createRenderTarget(curWidth, curHeight, true);
		}
		DXL2_RenderBackend::bindRenderTarget(s_view3d);
		{
			const f32 clearColor[] = { 0.05f, 0.06f, 0.1f, 1.0f };
			DXL2_RenderBackend::clearRenderTarget(s_view3d, clearColor, 1.0f);
			DXL2_RenderState::setColorMask(CMASK_RGB);

			// Calculate the sub-grid size.
			if (s_gridSize > 1.0f)
			{
				s_subGridSize = floorf(log2f(1.0f / s_zoomVisual));
				s_subGridSize = powf(2.0f, s_subGridSize);

				f32 scaledGrid = s_gridSize / s_subGridSize;
				if (floorf(scaledGrid * 100.0f)*0.01f != scaledGrid)
				{
					s_subGridSize = s_gridSize * 4.0f;
				}
			}
			else
			{
				s_subGridSize = std::min(floorf(log10f(0.1f / s_zoomVisual)), 2.0f);
				s_subGridSize = powf(10.0f, s_subGridSize);
			}
			s_subGridSize = std::max(0.0f, s_subGridSize);

			u32 rtWidth, rtHeight;
			DXL2_RenderBackend::getRenderTargetDim(s_view3d, &rtWidth, &rtHeight);
			if (s_editView == EDIT_VIEW_2D)      { drawLevel2d(rtWidth, rtHeight); }
			else if (s_editView == EDIT_VIEW_3D) { drawLevel3d(rtWidth, rtHeight); }
		}
		DXL2_RenderBackend::unbindRenderTarget();
		DXL2_RenderState::setColorMask(CMASK_ALL);

		// Draw the edit window
		levelEditWinBegin();
		{
			const TextureGpu* texture = DXL2_RenderBackend::getRenderTargetTexture(s_view3d);
			ImGui::ImageButton(DXL2_RenderBackend::getGpuPtr(texture), { (f32)curWidth, (f32)curHeight }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0, { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
			const ImVec2 itemPos = ImGui::GetItemRectMin();
			s_editWinMapCorner = { itemPos.x, itemPos.y };

			s_hoveredSector = -1;
			s_hoveredWall = -1;
			if ((ImGui::IsWindowHovered() || ImGui::IsWindowFocused()) && (!HelpWindow::isOpen() || HelpWindow::isMinimized()))
			{
				editWinControls((s32)curWidth, (s32)curHeight);
			}

			// Draw the position of the currently hovered vertex.
			if (s_hoveredVertex >= 0 && s_hoveredVertex < s_levelData->sectors[s_hoveredVertexSector].vertices.size() && s_editView == EDIT_VIEW_2D)
			{
				const EditorSector* sector = s_levelData->sectors.data() + s_hoveredVertexSector;
				const Vec2f* vtx = sector->vertices.data() + s_hoveredVertex;
				const f32 rcpZoom = 1.0f / s_zoomVisual;
				s32 x = s32(( vtx->x - s_offset.x) * rcpZoom + s_editWinMapCorner.x ) - 56;
				s32 y = s32((-vtx->z - s_offset.z) * rcpZoom + s_editWinMapCorner.z ) - 40;
				x = std::min(s32(s_editWinMapCorner.x + s_editWinSize.x) - 128, std::max((s32)s_editWinMapCorner.x, x));
				y = std::min(s32(s_editWinMapCorner.z + s_editWinSize.z) - 32, std::max((s32)s_editWinMapCorner.z, y));

				const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
					| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs;

				ImVec4 windowBg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
				windowBg.w *= 0.75f;
				ImGui::PushStyleColor(ImGuiCol_ChildBg, windowBg);
				ImGui::SetNextWindowPos({ f32(x), f32(y) });
				ImGui::BeginChild("Coordinates", ImVec2(128, 32), true, window_flags);
				{
					ImGui::TextColored({ 1.0f, 1.0f, 1.0f, 0.75f }, "%0.2f, %0.2f", vtx->x, vtx->z);
				}
				ImGui::EndChild();
				ImGui::PopStyleColor();
			}
		}
		levelEditWinEnd();

		// Show the error popup if needed.
		showErrorPopup();

		// Load and other actions.
		if (s_loadLevel)
		{
			loadLevel();
			s_loadLevel = false;
		}
	}
		
	void menu()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New", "Ctrl+N", (bool*)NULL))
			{
				// GOB (creates a new Gob if it doesn't exist)
				// Name
				// Slot (SECBASE, TALAY, etc.)
				// Resources (TEXTURE.GOB, SPRITES.GOB, SOUNDS.GOB, DARK.GOB, ...)
			}
			if (ImGui::MenuItem("Copy Level", NULL, (bool*)NULL))
			{
				// Source GOB
				// Select Level
				// Destination GOB
				// New Map from copy (same as New Level)
			}
			if (ImGui::MenuItem("Open", "Ctrl+O", (bool*)NULL))
			{
				// Open GOB
				// Open Level
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Close", NULL, (bool*)NULL))
			{
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Save", "Ctrl+S", (bool*)NULL))
			{
			}
			ImGui::Separator();
			// Path to Gob : filename
			if (ImGui::BeginMenu("Recent"))
			{
				for (u32 i = 0; i < s_recentCount; i++)
				{
					char recent[DXL2_MAX_PATH];
					sprintf(recent, "%s: \"%s\" (%s)", s_recentLevels[i].gobName, s_recentLevels[i].levelName, s_recentLevels[i].levelFilename);
					if (ImGui::MenuItem(recent, NULL, (bool*)NULL))
					{
						strcpy(s_gobFile, s_recentLevels[i].gobPath);
						strcpy(s_levelFile, s_recentLevels[i].levelFilename);
						strcpy(s_levelName, s_recentLevels[i].levelName);
						s_loadLevel = true;
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", (bool*)NULL))
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
			ImGui::Separator();
			if (ImGui::MenuItem("Clear Selection", "Backspace", (bool*)NULL))
			{
				s_selectedSector = -1;
				s_selectedVertex = -1;
				s_selectedWall = -1;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("2D", "Ctrl+1", s_editView == EDIT_VIEW_2D))
			{
				if (s_editView == EDIT_VIEW_3D)
				{
					const Vec2f scale = { s_editWinSize.x*0.5f, s_editWinSize.z*0.5f };
					s_offset.x =  s_camera.pos.x - scale.x*s_zoomVisual;
					s_offset.z = -s_camera.pos.z - scale.z*s_zoomVisual;
				}
				s_editView = EDIT_VIEW_2D;
			}
			if (ImGui::MenuItem("3D (Editor)", "Ctrl+2", s_editView == EDIT_VIEW_3D))
			{
				if (s_editView == EDIT_VIEW_2D)
				{
					const Vec2f scale = { s_editWinSize.x*0.5f, s_editWinSize.z*0.5f };
					s_camera.pos.x =  s_offset.x + scale.x*s_zoomVisual;
					s_camera.pos.z = -s_offset.z - scale.z*s_zoomVisual;
				}
				s_editView = EDIT_VIEW_3D;
			}
			if (ImGui::MenuItem("3D (Game)", "Ctrl+3", s_editView == EDIT_VIEW_3D_GAME))
			{
				s_editView = EDIT_VIEW_3D_GAME;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Show Lower Layers", "Ctrl+L", s_showLowerLayers))
			{
				s_showLowerLayers = !s_showLowerLayers;
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
			if (ImGui::MenuItem("Fullbright", "Ctrl+F5", s_fullbright))
			{
				s_fullbright = !s_fullbright;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Display Sector Colors", "", s_showSectorColors))
			{
				s_showSectorColors = !s_showSectorColors;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Run"))
		{
			if (ImGui::MenuItem("Run [DarkXL 2]", "Ctrl+R", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Run [Dark Forces]", "Ctrl+T", (bool*)NULL))
			{
			}
			ImGui::EndMenu();
		}
	}

	void toolbarBegin()
	{
		bool toolbarActive = true;

		DisplayInfo displayInfo;
		DXL2_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("MainToolbar", { 0.0f, 19.0f });
		ImGui::SetWindowSize("MainToolbar", { (f32)displayInfo.width - 480.0f, 48.0f });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("MainToolbar", &toolbarActive, window_flags);
	}

	void toolbarEnd()
	{
		ImGui::End();
	}
				
	void levelEditWinBegin()
	{
		bool gridActive = true;

		DisplayInfo displayInfo;
		DXL2_RenderBackend::getDisplayInfo(&displayInfo);

		s_editWinSize = { (f32)displayInfo.width - 480.0f, (f32)displayInfo.height - 68.0f };

		ImGui::SetWindowPos("LevelEditWin", { s_editWinPos.x, s_editWinPos.z });
		ImGui::SetWindowSize("LevelEditWin", { s_editWinSize.x, s_editWinSize.z });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("LevelEditWin", &gridActive, window_flags);
	}

	void levelEditWinEnd()
	{
		ImGui::End();
	}

	void messagePanel(ImVec2 pos)
	{
		bool msgPanel = true;
		DisplayInfo displayInfo;
		DXL2_RenderBackend::getDisplayInfo(&displayInfo);

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground;

		ImGui::SetNextWindowPos({ pos.x, pos.y + 24.0f });
		ImGui::BeginChild("MsgPanel", { 256.0f, 20.0f }, false, window_flags);

		s32 mx, my;
		DXL2_Input::getMousePos(&mx, &my);
		if (mx >= s_editWinPos.x && mx < s_editWinPos.x + s_editWinSize.x && my >= s_editWinPos.z && my < s_editWinPos.z + s_editWinSize.z)
		{
			// We want to zoom into the mouse position.
			s32 relX = s32(mx - s_editWinMapCorner.x);
			s32 relY = s32(my - s_editWinMapCorner.z);
			// Old position in world units.
			Vec2f worldPos;
			worldPos.x = s_offset.x + f32(relX) * s_zoomVisual;
			worldPos.z = s_offset.z + f32(relY) * s_zoomVisual;
			if (s_editView == EDIT_VIEW_2D)
			{
				ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 0.75f }, "Pos %0.2f, %0.2f   Sub-grid %0.2f", worldPos.x, -worldPos.z, s_gridSize / s_subGridSize);
			}
			else
			{
				ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 0.75f }, "Dir %0.3f, %0.3f, %0.3f   Sub-grid %0.2f", s_rayDir.x, s_rayDir.y, s_rayDir.z, s_gridSize / s_subGridSize);
			}
		}

		ImGui::EndChild();
	}
	   
	void infoToolBegin(s32 height)
	{
		bool infoTool = true;

		DisplayInfo displayInfo;
		DXL2_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("Info Panel", { (f32)displayInfo.width - 480.0f, 19.0f });
		ImGui::SetWindowSize("Info Panel", { 480.0f, f32(height) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("Info Panel", &infoTool, window_flags);
	}
		
	void infoPanelMap()
	{
		if (!s_levelData) { return; }

		ImGui::Text("Level Name:"); ImGui::SameLine(); ImGui::InputText("", s_levelName, 64);
		ImGui::Text("Gob File: %s", s_gobFile);
		ImGui::Text("Level File: %s", s_levelFile);
		ImGui::Text("Sector Count: %u", s_levelData->sectors.size());
		ImGui::Text("Layer Range: %d, %d", s_levelData->layerMin, s_levelData->layerMax);
	}

	void infoPanelVertex()
	{
		if (!s_levelData || (s_selectedVertex < 0 && s_hoveredVertex < 0)) { return; }
		EditorSector* sector = s_levelData->sectors.data() + ( (s_selectedVertex >= 0) ? s_selectedVertexSector : s_hoveredVertexSector );
		s32 index = s_selectedVertex >= 0 ? s_selectedVertex : s_hoveredVertex;
		if (index < 0 || !sector) { return; }

		Vec2f* vtx = sector->vertices.data() + index;

		ImGui::Text("Vertex %d of Sector %d", index, sector->id);
		char s_vtxPosX_str[64];
		char s_vtxPosZ_str[64];
		sprintf(s_vtxPosX_str, "%0.2f", vtx->x);
		sprintf(s_vtxPosZ_str, "%0.2f", vtx->z);

		ImGui::NewLine();
		ImGui::PushItemWidth(64.0f);
		ImGui::LabelText("##PositionLabel", "Position");
		ImGui::PopItemWidth();

		ImGui::SameLine();
		ImGui::PushItemWidth(196.0f);
		ImGui::InputFloat2("##VertexPosition", &vtx->x, "%0.2f", ImGuiInputTextFlags_CharsDecimal);
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
		EditorSector* sector;
		EditorWall* wall;
		
		s32 wallId;
		if (s_selectedWall >= 0) { sector = s_levelData->sectors.data() + s_selectedWallSector; wallId = s_selectedWall; }
		else if (s_hoveredWall >= 0) { sector = s_levelData->sectors.data() + s_hoveredWallSector; wallId = s_hoveredWall; }
		else { return; }
		wall = sector->walls.data() + wallId;

		if (s_enableInfEditor)
		{
			infoPanelInfWall(sector, wallId);
			return;
		}

		Vec2f* vertices = sector->vertices.data();
		f32 len = DXL2_Math::distance(&vertices[wall->i0], &vertices[wall->i1]);

		ImGui::Text("Wall ID: %d  Sector ID: %d  Length: %2.2f", wallId, sector->id, len);
		ImGui::Text("Vertices: [%d](%2.2f, %2.2f), [%d](%2.2f, %2.2f)", wall->i0, vertices[wall->i0].x, vertices[wall->i0].z, wall->i1, vertices[wall->i1].x, vertices[wall->i1].z);
		ImGui::Separator();

		// Adjoin data (should be carefully and rarely edited directly).
		ImGui::LabelText("##SectorAdjoin", "Adjoin"); ImGui::SameLine(64.0f);
		infoIntInput("##SectorAdjoinInput", 96, &wall->adjoin); ImGui::SameLine(180.0f);
		ImGui::LabelText("##SectorWalk", "Walk"); ImGui::SameLine(224.0f);
		infoIntInput("##SectorWalkInput", 96, &wall->walk); ImGui::SameLine(335.0f);
		ImGui::LabelText("##SectorMirror", "Mirror"); ImGui::SameLine(384);
		infoIntInput("##SectorMirrorInput", 96, &wall->mirror);

		s32 light = wall->light;
		ImGui::LabelText("##SectorLight", "Light Adjustment"); ImGui::SameLine(138.0f);
		infoIntInput("##SectorLightInput", 96, &light);
		wall->light = (s16)light;

		ImGui::SameLine();
		if (ImGui::Button("Edit INF Script##Wall"))
		{
			if (isValidName(sector->name))
			{
				// Enable the editor.
				s_enableInfEditor = true;
			}
			else
			{
				// Popup a message box.
				popupErrorMessage("INF script functionality relies on sector names for identification. Please give the sector a unique name (in this level) before proceeding.");
			}
		}

		ImGui::Separator();

		// Flags
		infoLabel("##Flags1Label", "Flags1", 48);
		infoUIntInput("##Flags1", 128, &wall->flags[0]);
		ImGui::SameLine();

		infoLabel("##Flags3Label", "Flags3", 48);
		infoUIntInput("##Flags3", 128, &wall->flags[2]);

		const f32 column[] = { 0.0f, 160.0f, 320.0f };

		ImGui::CheckboxFlags("Mask Wall##WallFlag", &wall->flags[0], WF1_ADJ_MID_TEX); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Illum Sign##WallFlag", &wall->flags[0], WF1_ILLUM_SIGN); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Horz Flip Texture##SectorFlag", &wall->flags[0], WF1_FLIP_HORIZ);

		ImGui::CheckboxFlags("Change WallLight##WallFlag", &wall->flags[0], WF1_CHANGE_WALL_LIGHT); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Tex Anchored##WallFlag", &wall->flags[0], WF1_TEX_ANCHORED); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Wall Morphs##SectorFlag", &wall->flags[0], WF1_WALL_MORPHS);

		ImGui::CheckboxFlags("Scroll Top Tex##WallFlag", &wall->flags[0], WF1_SCROLL_TOP_TEX); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Scroll Mid Tex##WallFlag", &wall->flags[0], WF1_SCROLL_MID_TEX); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Scroll Bottom Tex##SectorFlag", &wall->flags[0], WF1_SCROLL_BOT_TEX);

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

		EditorTexture* midTex = wall->mid.tex;
		EditorTexture* topTex = wall->top.tex;
		EditorTexture* botTex = wall->bot.tex;
		EditorTexture* sgnTex = wall->sign.tex;

		const f32 texCol = 150.0f;
		// Labels
		ImGui::Text("Mid Texture"); ImGui::SameLine(texCol);
		ImGui::Text("Sign Texture");

		// Textures - Mid / Sign
		const f32 midScale = midTex ? 1.0f / (f32)std::max(midTex->width, midTex->height) : 0.0f;
		const f32 sgnScale = sgnTex ? 1.0f / (f32)std::max(sgnTex->width, sgnTex->height) : 0.0f;
		const f32 aspectMid[] = { midTex ? f32(midTex->width) * midScale : 1.0f, midTex ? f32(midTex->height) * midScale : 1.0f };
		const f32 aspectSgn[] = { sgnTex ? f32(sgnTex->width) * sgnScale : 1.0f, sgnTex ? f32(sgnTex->height) * sgnScale : 1.0f };

		ImGui::ImageButton(midTex ? DXL2_RenderBackend::getGpuPtr(midTex->texture) : nullptr, { 128.0f * aspectMid[0], 128.0f * aspectMid[1] });
		ImGui::SameLine(texCol);
		ImGui::ImageButton(sgnTex ? DXL2_RenderBackend::getGpuPtr(sgnTex->texture) : nullptr, { 128.0f * aspectSgn[0], 128.0f * aspectSgn[1] });
		const ImVec2 imageLeft0 = ImGui::GetItemRectMin();
		const ImVec2 imageRight0 = ImGui::GetItemRectMax();

		// Names
		ImGui::Text("%s %dx%d", midTex ? midTex->name : "NONE", midTex ? midTex->width : 0, midTex ? midTex->height : 0); ImGui::SameLine(texCol);
		ImGui::Text("%s %dx%d", sgnTex ? sgnTex->name : "NONE", sgnTex ? sgnTex->width : 0, sgnTex ? sgnTex->height : 0);

		ImVec2 imageLeft1, imageRight1;
		if (wall->adjoin >= 0)
		{
			ImGui::NewLine();

			// Textures - Top / Bottom
			// Labels
			ImGui::Text("Top Texture"); ImGui::SameLine(texCol); ImGui::Text("Bottom Texture");

			const f32 topScale = topTex ? 1.0f / (f32)std::max(topTex->width, topTex->height) : 0.0f;
			const f32 botScale = botTex ? 1.0f / (f32)std::max(botTex->width, botTex->height) : 0.0f;
			const f32 aspectTop[] = { topTex ? f32(topTex->width) * topScale : 1.0f, topTex ? f32(topTex->height) * topScale : 1.0f };
			const f32 aspectBot[] = { botTex ? f32(botTex->width) * botScale : 1.0f, botTex ? f32(botTex->height) * botScale : 1.0f };

			ImGui::ImageButton(topTex ? DXL2_RenderBackend::getGpuPtr(topTex->texture) : nullptr, { 128.0f * aspectTop[0], 128.0f * aspectTop[1] });
			ImGui::SameLine(texCol);
			ImGui::ImageButton(botTex ? DXL2_RenderBackend::getGpuPtr(botTex->texture) : nullptr, { 128.0f * aspectBot[0], 128.0f * aspectBot[1] });
			imageLeft1 = ImGui::GetItemRectMin();
			imageRight1 = ImGui::GetItemRectMax();

			// Names
			ImGui::Text("%s %dx%d", topTex ? topTex->name : "NONE", topTex ? topTex->width : 0, topTex ? topTex->height : 0); ImGui::SameLine(texCol);
			ImGui::Text("%s %dx%d", botTex ? botTex->name : "NONE", botTex ? botTex->width : 0, botTex ? botTex->height : 0);
		}

		// Texture Offsets
		DisplayInfo displayInfo;
		DXL2_RenderBackend::getDisplayInfo(&displayInfo);

		// Set 0
		ImVec2 offsetLeft = { imageLeft0.x + texCol + 8.0f, imageLeft0.y + 8.0f };
		ImVec2 offsetSize = { displayInfo.width - (imageLeft0.x + texCol + 16.0f), 128.0f };
		ImGui::SetNextWindowPos(offsetLeft);
		// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
		ImGui::BeginChild("##TextureOffsets0Wall", offsetSize);
		{
			ImGui::Text("Mid Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##MidOffsetInput", &wall->mid.offsetX, "%.2f");
			ImGui::PopItemWidth();

			ImGui::NewLine();

			ImGui::Text("Sign Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##SignOffsetInput", &wall->sign.offsetX, "%.2f");
			ImGui::PopItemWidth();
		}
		ImGui::EndChild();

		// Set 1
		if (wall->adjoin >= 0)
		{
			offsetLeft = { imageLeft1.x + texCol + 8.0f, imageLeft1.y + 8.0f };
			offsetSize = { displayInfo.width - (imageLeft1.x + texCol + 16.0f), 128.0f };
			ImGui::SetNextWindowPos(offsetLeft);
			// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
			ImGui::BeginChild("##TextureOffsets1Wall", offsetSize);
			{
				ImGui::Text("Top Offset");
				ImGui::PushItemWidth(128.0f);
				ImGui::InputFloat2("##TopOffsetInput", &wall->top.offsetX, "%.2f");
				ImGui::PopItemWidth();

				ImGui::NewLine();

				ImGui::Text("Bottom Offset");
				ImGui::PushItemWidth(128.0f);
				ImGui::InputFloat2("##BotOffsetInput", &wall->bot.offsetX, "%.2f");
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();
		}
	}

	bool isValidName(const char* name)
	{
		if (strlen(name) > 0 && name[0] > 32 && name[0] < 127) { return true; }
		return false;
	}

	void addFuncText(EditorInfClassData* cdata, u32 stopIndex, u32 funcIndex, char** itemList, s32& itemCount, bool inclTarget)
	{
		const EditorInfFunction* func = &cdata->stop[stopIndex].func[funcIndex];
		const u32 funcId = func->funcId;
		const u32 clientCount = (u32)func->client.size();
		const u32 argCount = (u32)func->arg.size();

		const char* funcName = DXL2_InfAsset::getFuncName(funcId);
		if (funcId < INF_MSG_LIGHTS)
		{
			char client[256];
			EditorSector* sector = LevelEditorData::getSector(func->client[0].sectorName.c_str());
			if (!sector) { return; }

			u32 sectorId = sector->id;
			s32 wallId = func->client[0].wallId;
			if (inclTarget)
			{
				if (wallId < 0) { sprintf(client, "%s", sector->name); }
				else { sprintf(client, "%s(%d)", sector->name, wallId); }
			}

			char funcText[256];
			if (argCount)
			{
				sprintf(funcText, "%s(", funcName);
				for (u32 a = 0; a < argCount; a++)
				{
					char argText[64];
					if (a < argCount - 1)
					{
						sprintf(argText, "%d, ", func->arg[a].iValue);
					}
					else
					{
						sprintf(argText, "%d", func->arg[a].iValue);
					}
					strcat(funcText, argText);
				}
				strcat(funcText, ")");
			}
			else
			{
				sprintf(funcText, "%s", funcName);
			}

			if (inclTarget) { sprintf(itemList[itemCount], "      Func: %s  Target: %s", funcText, client); }
			else { sprintf(itemList[itemCount], "      Func: %s", funcText); }

			itemCount++;
		}
		else
		{
			if (funcId == INF_MSG_LIGHTS)
			{
				sprintf(itemList[itemCount], "      Func: lights");
				itemCount++;
			}
			else if (funcId == INF_MSG_ADJOIN)
			{
				sprintf(itemList[itemCount], "      Func: adjoin(%s, %d, %s, %d)",
					func->arg[0].sValue.c_str(), func->arg[1].iValue,
					func->arg[2].sValue.c_str(), func->arg[3].iValue);
				itemCount++;
			}
			else if (funcId == INF_MSG_PAGE)
			{
				// This is supposed to be a string - Sounds are TODO.
				sprintf(itemList[itemCount], "      Func: page(TODO)");
				itemCount++;
			}
			else if (funcId == INF_MSG_TEXT)
			{
				sprintf(itemList[itemCount], "      Func: text(%d)", func->arg[0].iValue);
				itemCount++;
			}
			else if (funcId == INF_MSG_TEXTURE)
			{
				sprintf(itemList[itemCount], "      Func: texture(%d, %s)", func->arg[0].iValue, func->arg[1].sValue.c_str());
				itemCount++;
			}
		}
	}

	char* getEventMaskList(u32 mask, char* buffer, bool wall)
	{
		buffer[0] = 0;
		if (!mask) { return buffer; }

		if (mask & (INF_EVENT_ANY))
		{
			strcat(buffer, "(");
			if (mask & INF_EVENT_CROSS_LINE_FRONT) { strcat(buffer, "cross front, "); }
			if (mask & INF_EVENT_CROSS_LINE_BACK)  { strcat(buffer, "cross back, "); }
			if (mask & INF_EVENT_ENTER_SECTOR)     { strcat(buffer, "enter, "); }
			if (mask & INF_EVENT_LEAVE_SECTOR)     { strcat(buffer, "leave, "); }
			if (mask & INF_EVENT_NUDGE_FRONT)      { strcat(buffer, wall ? "nudge front, " : "nudge inside, "); }
			if (mask & INF_EVENT_NUDGE_BACK)       { strcat(buffer, wall ? "nudge back, " : "nudge outside, "); }
			if (mask & INF_EVENT_EXPLOSION)        { strcat(buffer, "explosion, "); }
			if (mask & INF_EVENT_SHOOT_LINE)       { strcat(buffer, "shoot, "); }
			if (mask & INF_EVENT_LAND)             { strcat(buffer, "land, "); }
			buffer[strlen(buffer) - 2] = 0;
			strcat(buffer, ")");
		}

		return buffer;
	}

	char* getEntityMaskList(u32 mask, char* buffer)
	{
		buffer[0] = 0;
		if (!mask) { return buffer; }

		strcat(buffer, "(");
		if (mask & INF_ENTITY_ENEMY)  { strcat(buffer, "enemy, "); }
		if (mask & INF_ENTITY_WEAPON) { strcat(buffer, "weapon, "); }
		if (mask & INF_ENTITY_PLAYER) { strcat(buffer, "player, "); }
		size_t len = strlen(buffer);
		if (len > 2) { buffer[len - 2] = 0; }
		strcat(buffer, ")");

		return buffer;
	}

	char* getKeyList(u32 keys, char* buffer)
	{
		buffer[0] = 0;
		if (!keys) { return buffer; }

		strcat(buffer, "(");
		if (keys & KEY_RED) { strcat(buffer, "red, "); }
		if (keys & KEY_YELLOW) { strcat(buffer, "yellow, "); }
		if (keys & KEY_BLUE) { strcat(buffer, "blue, "); }
		buffer[strlen(buffer) - 2] = 0;
		strcat(buffer, ")");

		return buffer;
	}

	bool isElevatorFloorCeil(u32 sclass)
	{
		return sclass == ELEVATOR_BASIC || sclass == ELEVATOR_INV || sclass == ELEVATOR_MOVE_FLOOR || sclass == ELEVATOR_MOVE_CEILING || sclass == ELEVATOR_MOVE_FC || sclass == ELEVATOR_MOVE_OFFSET || sclass == ELEVATOR_BASIC_AUTO;
	}

	bool isDoor(u32 sclass)
	{
		return sclass == ELEVATOR_DOOR || sclass == ELEVATOR_DOOR_MID || sclass == ELEVATOR_DOOR_INV;
	}
		
	void infEditor(EditorInfItem* item, bool wall)
	{
		InfEditState* infEditState = LevelEditorData::getInfEditState();

		// There will be context sensitive controls here, such as adding classes.

		// Get the current list of items.
		s32 itemCount = 0;
		strcpy(infEditState->itemList[itemCount], "Seq"); itemCount++;
		// Go through the data here...
		if (item)
		{
			const u32 classCount = (u32)item->classData.size();
			for (u32 c = 0; c < classCount; c++)
			{
				EditorInfClassData* cdata = &item->classData[c];
				const u32 iclass = cdata->iclass;
				const u32 sclass = cdata->isubclass;

				sprintf(infEditState->itemList[itemCount], "  Class: %s %s",
					DXL2_InfAsset::getClassName(iclass), DXL2_InfAsset::getSubclassName(iclass, sclass)); itemCount++;

				if (iclass == INF_CLASS_ELEVATOR)
				{
					if (!isDoor(sclass))
					{
						const f32 sgn = isElevatorFloorCeil(sclass) ? -1.0f : 1.0f;
						for (u32 s = 0; s < (u32)cdata->stop.size(); s++)
						{
							const u32 stop0Type = cdata->stop[s].stopValue0Type;
							const u32 stop1Type = cdata->stop[s].stopValue1Type;
							const u32 funcCount = (u32)cdata->stop[s].func.size();

							char value0[256];
							char value1[256];
							if (stop0Type == INF_STOP0_SECTORNAME)
							{
								sprintf(value0, "\"%s\"", cdata->stop[s].value0.sValue.c_str());
							}
							else if (stop0Type == INF_STOP0_ABSOLUTE)
							{
								// Show the values in the same coordinate space as the level.
								sprintf(value0, "%2.2f", cdata->stop[s].value0.fValue * sgn);
							}
							else if (stop0Type == INF_STOP0_RELATIVE)
							{
								sprintf(value0, "@%2.2f", cdata->stop[s].value0.fValue * sgn);
							}

							if (stop1Type == INF_STOP1_TIME)
							{
								sprintf(value1, "%2.2f", cdata->stop[s].time);
							}
							else if (stop1Type == INF_STOP1_HOLD)
							{
								sprintf(value1, "hold");
							}
							else if (stop1Type == INF_STOP1_TERMINATE)
							{
								sprintf(value1, "terminate");
							}
							else if (stop1Type == INF_STOP1_COMPLETE)
							{
								sprintf(value1, "complete");
							}

							sprintf(infEditState->itemList[itemCount], "    Stop %d: %s %s", s, value0, value1); itemCount++;

							for (u32 f = 0; f < funcCount; f++)
							{
								addFuncText(cdata, s, f, infEditState->itemList.data(), itemCount, true);
							}
						}
					}
				}
				else if (iclass == INF_CLASS_TRIGGER)
				{
					// Functions
					const u32 funcCount = (u32)cdata->stop[0].func.size();
					for (u32 f = 0; f < funcCount; f++)
					{
						addFuncText(cdata, 0, f, infEditState->itemList.data(), itemCount, false);
					}
					// Clients.
					const EditorInfFunction* func = &cdata->stop[0].func[0];
					const u32 clientCount = (u32)func->client.size();
					if (clientCount)
					{
						sprintf(infEditState->itemList[itemCount], "    Clients %u", clientCount); itemCount++;
						for (u32 c = 0; c < clientCount; c++)
						{
							const s32 wallId = func->client[c].wallId;
							if (wallId < 0)
							{
								sprintf(infEditState->itemList[itemCount], "      %s", func->client[c].sectorName.c_str()); itemCount++;
							}
							else
							{
								sprintf(infEditState->itemList[itemCount], "      %s(%d)", func->client[c].sectorName.c_str(), wallId); itemCount++;
							}
						}
					}
				}
				else
				{
					sprintf(infEditState->itemList[itemCount], "    Target: %s", cdata->var.target.c_str()); itemCount++;
				}

				// Slaves
				if (iclass == INF_CLASS_ELEVATOR)
				{
					const u32 slaveCount = (u32)cdata->slaves.size();
					sprintf(infEditState->itemList[itemCount], "    Slaves %u", slaveCount); itemCount++;
					for (u32 s = 0; s < slaveCount; s++)
					{
						sprintf(infEditState->itemList[itemCount], "      %s", cdata->slaves[s].c_str()); itemCount++;
					}
				}

				// Variables.
				u32 defaultEventMask = 0;
				if (iclass == INF_CLASS_ELEVATOR && (sclass == ELEVATOR_DOOR || sclass == ELEVATOR_DOOR_INV || sclass == ELEVATOR_DOOR_MID))
				{
					defaultEventMask = INF_EVENT_NUDGE_BACK;
				}
				else if (iclass == INF_CLASS_ELEVATOR && (sclass == ELEVATOR_BASIC || sclass == ELEVATOR_INV || sclass == ELEVATOR_BASIC_AUTO))
				{
					defaultEventMask = 52;
				}
				else if (iclass == INF_CLASS_ELEVATOR && (sclass == ELEVATOR_MORPH_MOVE1 || sclass == ELEVATOR_MORPH_MOVE2 || sclass == ELEVATOR_MORPH_SPIN1 || sclass == ELEVATOR_MORPH_SPIN2))
				{
					defaultEventMask = 60;
				}
				else if (iclass == INF_CLASS_TRIGGER)
				{
					defaultEventMask = INF_EVENT_CROSS_LINE_FRONT | INF_EVENT_CROSS_LINE_BACK | INF_EVENT_ENTER_SECTOR | INF_EVENT_LEAVE_SECTOR | INF_EVENT_NUDGE_FRONT | INF_EVENT_NUDGE_BACK | INF_EVENT_LAND;
				}

				char buffer[128];
				sprintf(infEditState->itemList[itemCount], "    Variables"); itemCount++;
				if (!cdata->var.master)
				{
					sprintf(infEditState->itemList[itemCount], "      master: off");
					itemCount++;
				}
				if (cdata->var.event_mask != defaultEventMask)
				{
					sprintf(infEditState->itemList[itemCount], "      event_mask: %u %s", cdata->var.event_mask, getEventMaskList(cdata->var.event_mask, buffer, wall));
					itemCount++;
				}
				if (cdata->var.event)
				{
					sprintf(infEditState->itemList[itemCount], "      event: %u", cdata->var.event);
					itemCount++;
				}
				if (cdata->var.entity_mask != INF_ENTITY_PLAYER)
				{
					sprintf(infEditState->itemList[itemCount], "      entity_mask: %u %s", cdata->var.entity_mask, getEntityMaskList(cdata->var.entity_mask, buffer));
					itemCount++;
				}
				if (cdata->var.speed != 30.0f && iclass == INF_CLASS_ELEVATOR)
				{
					sprintf(infEditState->itemList[itemCount], "      speed: %2.2f", cdata->var.speed);
					itemCount++;
				}
				if (cdata->var.start != 0 && iclass == INF_CLASS_ELEVATOR)
				{
					sprintf(infEditState->itemList[itemCount], "      start: %d", cdata->var.start);
					itemCount++;
				}
				if (sclass == ELEVATOR_MORPH_SPIN1 || sclass == ELEVATOR_MORPH_SPIN2 || sclass == ELEVATOR_ROTATE_WALL)
				{
					sprintf(infEditState->itemList[itemCount], "      center: %2.2f, %2.2f", cdata->var.center.x, cdata->var.center.z);
					itemCount++;
				}
				if (sclass == ELEVATOR_MORPH_MOVE1 || sclass == ELEVATOR_MORPH_MOVE2 || sclass == ELEVATOR_MOVE_WALL || sclass == ELEVATOR_SCROLL_WALL)
				{
					sprintf(infEditState->itemList[itemCount], "      angle: %2.2f", cdata->var.angle);
					itemCount++;
				}
				if (cdata->var.key != 0)
				{
					sprintf(infEditState->itemList[itemCount], "      key: %u %s", cdata->var.key, getKeyList(cdata->var.key, buffer));
					itemCount++;
				}
				u32 flagsDefault = 0;
				if (iclass == INF_CLASS_ELEVATOR && (sclass == ELEVATOR_SCROLL_FLOOR || sclass == ELEVATOR_MORPH_MOVE2 || sclass == ELEVATOR_MORPH_SPIN2))
				{
					flagsDefault = INF_MOVE_FLOOR | INF_MOVE_SECALT;
				}
				if (cdata->var.flags != flagsDefault)
				{
					sprintf(infEditState->itemList[itemCount], "      flags: %u", cdata->var.flags);
					itemCount++;
				}
				if (cdata->var.target != "")
				{
					sprintf(infEditState->itemList[itemCount], "      target: %s", cdata->var.target.c_str());
					itemCount++;
				}
				// Skip sound for now.
				// Skip "object_mask" for now.
			}
		}
		strcpy(infEditState->itemList[itemCount], "Seqend"); itemCount++;

		ImGui::SetNextItemWidth(440.0f);
		ImGui::ListBox("##InfDescrSector", &infEditState->editIndex, infEditState->itemList.data(), itemCount, 20);
	}

	void infoPanelInfSector(EditorSector* sector)
	{
		if (!isValidName(sector->name))
		{
			s_enableInfEditor = false;
			return;
		}
		InfEditState* infEditState = LevelEditorData::getInfEditState();
		if (infEditState->sector != sector || infEditState->wallId >= 0)
		{
			infEditState->sector = sector;
			infEditState->wallId = -1;
			infEditState->item = sector->infItem;
			infEditState->editIndex = 0;

			infEditState->itemMem.resize(1024 * 64);
			infEditState->itemList.resize(1024);
			char* itemMem = infEditState->itemMem.data();
			for (u32 i = 0; i < 1024; i++, itemMem += 64)
			{
				infEditState->itemList[i] = itemMem;
			}
		}

		// Sector Name (optional, used by INF)
		infoLabel("##NameLabel", "Name", 32);
		ImGui::PushItemWidth(240.0f);
		ImGui::InputText("##NameSector", sector->name, 32);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		if (ImGui::Button("Return to Properties##Sector"))
		{
			s_enableInfEditor = false;
		}
		ImGui::Separator();

		infEditor(infEditState->item, false);
	}

	void infoPanelInfWall(EditorSector* sector, u32 wallId)
	{
		if (!isValidName(sector->name))
		{
			s_enableInfEditor = false;
			return;
		}
		InfEditState* infEditState = LevelEditorData::getInfEditState();
		if (infEditState->sector != sector || infEditState->wallId != wallId)
		{
			infEditState->sector = sector;
			infEditState->wallId = wallId;
			infEditState->item = sector->walls[wallId].infItem;
			infEditState->editIndex = 0;

			infEditState->itemMem.resize(1024 * 64);
			infEditState->itemList.resize(1024);
			char* itemMem = infEditState->itemMem.data();
			for (u32 i = 0; i < 1024; i++, itemMem += 64)
			{
				infEditState->itemList[i] = itemMem;
			}
		}

		ImGui::Text("Wall ID: %d  Sector: %s", wallId, sector->name);
		if (ImGui::Button("Return to Properties##Wall"))
		{
			s_enableInfEditor = false;
		}
		ImGui::Separator();

		infEditor(infEditState->item, true);
	}
		
	void infoPanelSector()
	{
		EditorSector* sector;
		if (s_selectedSector >= 0) { sector = s_levelData->sectors.data() + s_selectedSector; }
		else if (s_hoveredSector >= 0) { sector = s_levelData->sectors.data() + s_hoveredSector; }
		else { return; }

		if (s_enableInfEditor)
		{
			infoPanelInfSector(sector);
			return;
		}

		ImGui::Text("Sector ID: %d      Wall Count: %u", sector->id, (u32)sector->walls.size());
		ImGui::Separator();

		// Sector Name (optional, used by INF)
		infoLabel("##NameLabel", "Name", 32);
		ImGui::PushItemWidth(240.0f);
		ImGui::InputText("##NameSector", sector->name, 32);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Edit INF Script##Sector"))
		{
			if (isValidName(sector->name))
			{
				// Enable the editor.
				s_enableInfEditor = true;
			}
			else
			{
				// Popup a message box.
				popupErrorMessage("INF script functionality relies on sector names for identification. Please give the sector a unique name (in this level) before proceeding.");
			}
		}

		// Layer and Ambient
		s32 layer = sector->layer;
		infoLabel("##LayerLabel", "Layer", 32);
		infoIntInput("##LayerSector", 96, &layer);
		if (layer != sector->layer)
		{
			sector->layer = layer;
			// Adjust layer range.
			s_levelData->layerMin = std::min(s_levelData->layerMin, (s8)layer);
			s_levelData->layerMax = std::max(s_levelData->layerMax, (s8)layer);
			// Change the current layer so we can still see the sector.
			s_layerIndex = layer - s_layerMin;
		}
		
		ImGui::SameLine(0.0f, 16.0f);

		s32 ambient = (s32)sector->ambient;
		infoLabel("##AmbientLabel", "Ambient", 48);
		infoIntInput("##AmbientSector", 96, &ambient);
		sector->ambient = std::max(0, std::min(31, ambient));

		// Heights
		infoLabel("##HeightLabel", "Heights", 64);

		infoLabel("##FloorHeightLabel", "Floor", 32);
		infoFloatInput("##FloorHeight", 64, &sector->floorAlt);
		ImGui::SameLine();

		infoLabel("##SecondHeightLabel", "Second", 44);
		infoFloatInput("##SecondHeight", 64, &sector->secAlt);
		ImGui::SameLine();

		infoLabel("##CeilHeightLabel", "Ceiling", 48);
		infoFloatInput("##CeilHeight", 64, &sector->ceilAlt);

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

		ImGui::CheckboxFlags("Exterior Ceil Adj##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXT_ADJ); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Exterior Floor Adj##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXT_FLOOR_ADJ);  ImGui::SameLine(column[2]);
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
		EditorTexture* floorTex = sector->floorTexture.tex;
		EditorTexture* ceilTex = sector->ceilTexture.tex;

		void* floorPtr = DXL2_RenderBackend::getGpuPtr(floorTex->texture);
		void* ceilPtr  = DXL2_RenderBackend::getGpuPtr(ceilTex->texture);

		const f32 texCol = 150.0f;
		// Labels
		ImGui::Text("Floor Texture"); ImGui::SameLine(texCol);
		ImGui::Text("Ceiling Texture");
		
		// Textures
		const f32 floorScale = 1.0f / (f32)std::max(floorTex->width, floorTex->height);
		const f32 ceilScale  = 1.0f / (f32)std::max(ceilTex->width, ceilTex->height);
		const f32 aspectFloor[] = { f32(floorTex->width) * floorScale, f32(floorTex->height) * floorScale };
		const f32 aspectCeil[] = { f32(ceilTex->width) * ceilScale, f32(ceilTex->height) * ceilScale };

		ImGui::ImageButton(floorPtr, { 128.0f * aspectFloor[0], 128.0f * aspectFloor[1] });
		ImGui::SameLine(texCol);
		ImGui::ImageButton(ceilPtr, { 128.0f * aspectCeil[0], 128.0f * aspectCeil[1] });
		const ImVec2 imageLeft  = ImGui::GetItemRectMin();
		const ImVec2 imageRight = ImGui::GetItemRectMax();

		// Names
		ImGui::Text("%s %dx%d", floorTex->name, floorTex->width, floorTex->height); ImGui::SameLine(texCol);
		ImGui::Text("%s %dx%d", ceilTex->name, ceilTex->width, ceilTex->height); ImGui::SameLine();

		// Texture Offsets
		DisplayInfo displayInfo;
		DXL2_RenderBackend::getDisplayInfo(&displayInfo);

		ImVec2 offsetLeft = { imageLeft.x + texCol + 8.0f, imageLeft.y + 8.0f };
		ImVec2 offsetSize = { displayInfo.width - (imageLeft.x + texCol + 16.0f), 128.0f };
		ImGui::SetNextWindowPos(offsetLeft);
		// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
		ImGui::BeginChild("##TextureOffsetsSector", offsetSize);
		{
			ImGui::Text("Floor Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##FloorOffsetInput", &sector->floorTexture.offsetX, "%.2f");
			ImGui::PopItemWidth();

			ImGui::NewLine();

			ImGui::Text("Ceil Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##CeilOffsetInput", &sector->ceilTexture.offsetX, "%.2f");
			ImGui::PopItemWidth();
		}
		ImGui::EndChild();
	}

	void infoPanelEntity()
	{
		s32 entityId = s_selectedEntity >= 0 ? s_selectedEntity : s_hoveredEntity;
		if (entityId < 0 || !s_levelData) { return; }

		EditorSector* sector = s_levelData->sectors.data() + (s_selectedEntity >= 0 ? s_selectedEntitySector : s_hoveredEntitySector);
		EditorLevelObject* obj = sector->objects.data() + entityId;

		// difficulty: -3, -2, -1, 0, 1, 2, 3 <+ 3> = 0, 1, 2, 3, 4, 5, 6
		const s32 diffToFlagMap[] = { DIFF_EASY | DIFF_MEDIUM | DIFF_HARD, DIFF_EASY | DIFF_MEDIUM, DIFF_EASY, DIFF_EASY | DIFF_MEDIUM | DIFF_HARD, DIFF_EASY | DIFF_MEDIUM | DIFF_HARD, DIFF_MEDIUM | DIFF_HARD, DIFF_HARD };
		u32 diffFlags = diffToFlagMap[obj->difficulty + 3];

		// Entity name = resource.
		ImGui::LabelText("##Entity", "Entity: %s", obj->dataFile.c_str());
		// Class.
		ImGui::LabelText("##EntityClass", "Class: %s", c_objectClassName[obj->oclass]);
		// Difficulty.
		ImGui::LabelText("##EntityDiff", "Difficulty:  Easy");  ImGui::SameLine(136.0f);
		ImGui::CheckboxFlags("##EntityDiffEasy", &diffFlags, DIFF_EASY); ImGui::SameLine(168.0f);
		ImGui::LabelText("##EntityDiffM", "Medium");  ImGui::SameLine(220.0f);
		ImGui::CheckboxFlags("##EntityDiffMed", &diffFlags, DIFF_MEDIUM); ImGui::SameLine(252.0f);
		ImGui::LabelText("##EntityDiffH", "Hard");  ImGui::SameLine(290.0f);
		ImGui::CheckboxFlags("##EntityDiffHard", &diffFlags, DIFF_HARD);
		// Position.
		ImGui::LabelText("##EntityClass", "Position:"); ImGui::SameLine(96.0f);
		ImGui::InputFloat3("##EntityPos", obj->pos.m, "%0.2f");
		// Orientation.
		ImGui::PushItemWidth(64.0f);
		ImGui::LabelText("##EntityAngleLabel", "Angle"); ImGui::SameLine(64.0f);
		ImGui::InputFloat("##EntityAngle", &obj->orientation.y, 0.0f, 0.0f, "%0.2f"); ImGui::SameLine(150.0f);
		ImGui::LabelText("##EntityPitchLabel", "Pitch"); ImGui::SameLine(204.0f);
		ImGui::InputFloat("##EntityPitch", &obj->orientation.x, 0.0f, 0.0f, "%0.2f"); ImGui::SameLine(290.0f);
		ImGui::LabelText("##EntityRollLabel", "Roll"); ImGui::SameLine(344.0f);
		ImGui::InputFloat("##EntityRoll", &obj->orientation.z, 0.0f, 0.0f, "%0.2f");
		ImGui::PopItemWidth();
		ImGui::Separator();
		ImGui::Text("Logics");
		// TODO: Visual controls for orientation (on the map).
		// TODO: Logic and Variable editing.
		const u32 logicCount = (u32)obj->logics.size();
		Logic* logic = obj->logics.data();
		for (u32 i = 0; i < logicCount; i++, logic++)
		{
			ImGui::Text("  %s", c_logicName[logic->type]);
			if (logic->type == LOGIC_UPDATE)
			{
				char updateFlags[256]="";
				if (logic->flags & 8)
				{
					strcat(updateFlags, "X-Axis");
					if (logic->flags > 8) { strcat(updateFlags, "|"); }
				}
				if (logic->flags & 16)
				{
					strcat(updateFlags, "Y-Axis");
					if (logic->flags > 16) { strcat(updateFlags, "|"); }
				}
				if (logic->flags & 32)
				{
					strcat(updateFlags, "Z-Axis");
				}

				ImGui::Text("    FLAGS: %u (%s)", logic->flags, updateFlags);
				if (logic->flags & 8)  { ImGui::Text("    D_PITCH: %2.2f", logic->rotation.x); }
				if (logic->flags & 16) { ImGui::Text("    D_YAW: %2.2f",   logic->rotation.y); }
				if (logic->flags & 32) { ImGui::Text("    D_ROLL: %2.2f",  logic->rotation.z); }
			}
			else if (logic->type == LOGIC_KEY)
			{
				ImGui::Text("    VUE: %s %s", logic->Vue.c_str(), logic->VueId.c_str());
				ImGui::Text("    VUE_APPEND: %s %s", logic->VueAppend.c_str(), logic->VueAppendId.c_str());
				ImGui::Text("    PAUSE: %s", obj->comFlags & LCF_PAUSE ? "true" : "false");
				ImGui::Text("    FRAME_RATE: %2.2f", logic->frameRate);
			}
		}
		const u32 genCount = (u32)obj->generators.size();
		EnemyGenerator* gen = obj->generators.data();
		for (u32 i = 0; i < genCount; i++, gen++)
		{
			ImGui::Text("  Generator %s", c_logicName[gen->type]);
		}
		ImGui::Separator();
		ImGui::Text("Common Variables");
		if (obj->comFlags & LCF_EYE)
		{
			ImGui::Text("  EYE: TRUE");
		}
		if (obj->comFlags & LCF_BOSS)
		{
			ImGui::Text("  BOSS: TRUE");
		}
		ImGui::LabelText("##EntityRadius", "  RADIUS:"); ImGui::SameLine(96.0f);
		ImGui::SetNextItemWidth(96.0f);
		ImGui::InputFloat("##EntityRadiusInput", &obj->radius, 0.0f, 0.0f, "%.2f");

		ImGui::LabelText("##EntityHeight", "  HEIGHT:"); ImGui::SameLine(96.0f);
		ImGui::SetNextItemWidth(96.0f);
		ImGui::InputFloat("##EntityHeightInput", &obj->height, 0.0f, 0.0f, "%.2f");
		
		// Correct the bounding box center if needed.
		obj->worldCen = obj->pos;
		obj->worldCen.y = obj->pos.y - obj->worldExt.y;
		if (obj->oclass != CLASS_SPIRIT && obj->oclass != CLASS_SAFE && obj->oclass != CLASS_SOUND && obj->display)
		{
			obj->worldCen.y += obj->display->scale.z * fabsf(obj->display->rect[1]) * c_spriteTexelToWorldScale;
		}

		// Fixup difficulty
		if (diffFlags == (DIFF_EASY | DIFF_MEDIUM | DIFF_HARD))
		{
			obj->difficulty = 0;
		}
		else if (diffFlags == (DIFF_EASY | DIFF_MEDIUM))
		{
			obj->difficulty = -2;
		}
		else if (diffFlags == (DIFF_MEDIUM | DIFF_HARD))
		{
			obj->difficulty = 2;
		}
		else if (diffFlags == DIFF_EASY)
		{
			obj->difficulty = -1;
		}
		else if (diffFlags == DIFF_MEDIUM)
		{
			// This is not actually valid, so pick medium + easy.
			obj->difficulty = -2;
		}
		else if (diffFlags == DIFF_HARD)
		{
			obj->difficulty = 3;
		}
	}

	void infoToolEnd()
	{
		ImGui::End();
	}

	void browserBegin(s32 offset)
	{
		bool browser = true;

		DisplayInfo displayInfo;
		DXL2_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("Browser", { (f32)displayInfo.width - 480.0f, 19.0f + f32(offset)});
		ImGui::SetWindowSize("Browser", { 480.0f, (f32)displayInfo.height - f32(offset+20) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("Browser", &browser, window_flags);
	}
		
	void browseTextures()
	{
		if (!s_levelData) { return; }

		u32 count = (u32)s_levelData->textures.size();
		u32 rows = count / 6;
		u32 leftOver = count - rows * 6;
		f32 y = 0.0f;
		for (u32 i = 0; i < rows; i++)
		{
			for (u32 t = 0; t < 6; t++)
			{
				EditorTexture* texture = &s_levelData->textures[i * 6 + t];
				void* ptr = DXL2_RenderBackend::getGpuPtr(texture->texture);
				u32 w = 64, h = 64;
				if (texture->width > texture->height)
				{
					w = 64;
					h = 64 * texture->height / texture->width;
				}
				else if (texture->width < texture->height)
				{
					h = 64;
					w = 64 * texture->width / texture->height;
				}

				if (ImGui::ImageButton(ptr, ImVec2(f32(w), f32(h)), ImVec2(0.0f,0.0f), ImVec2(1.0f, 1.0f), (64 - s32(w))/2 + 2))
				{
					// TODO: select textures.
					// TODO: add hover functionality (tool tip - but show full texture + file name + size)
				}
				ImGui::SameLine();
			}
			ImGui::NewLine();
		}
	}

	void browseEntities()
	{
	}

	void browserEnd()
	{
		ImGui::End();
	}
		
	void loadLevel()
	{
		DXL2_LevelList::unload();
		s_selectedSector = -1;

		// First setup the GOB - for now assume DARK.GOB
		if (strcasecmp("DARK.GOB", s_gobFile) == 0)
		{
			DXL2_AssetSystem::clearCustomArchive();
		}
		else
		{
			char gobPath[DXL2_MAX_PATH];
			DXL2_Paths::appendPath(DXL2_PathType::PATH_SOURCE_DATA, s_gobFile, gobPath);

			char extension[DXL2_MAX_PATH];
			FileUtil::getFileExtension(s_gobFile, extension);

			ArchiveType type = ARCHIVE_COUNT;
			if (strcasecmp(extension, "gob") == 0) { type = ARCHIVE_GOB; }
			else if (strcasecmp(extension, "lfd") == 0) { type = ARCHIVE_LFD; }
			else { assert(0); }

			Archive* archive = Archive::getArchive(type, s_gobFile, gobPath);
			DXL2_AssetSystem::setCustomArchive(archive);
		}
		
		DXL2_LevelList::load();
		if (DXL2_LevelAsset::load(s_levelFile))
		{
			const LevelData* levelData = DXL2_LevelAsset::getLevelData();
			
			char infFile[64];
			strcpy(infFile, s_levelFile);
			const size_t len = strlen(s_levelFile);
			infFile[len - 3] = 'I';
			infFile[len - 2] = 'N';
			infFile[len - 1] = 'F';
			DXL2_InfAsset::load(infFile);
			const InfData* levelInf = DXL2_InfAsset::getInfData();

			// Load Objects.
			char objFile[64];
			strcpy(objFile, s_levelFile);
			objFile[len - 3] = 'O';
			objFile[len - 2] = 0;
			objFile[len - 1] = 0;
			DXL2_LevelObjects::load(objFile);
			const LevelObjectData* levelObj = DXL2_LevelObjects::getLevelObjectData();

			// Get the palette.
			char palFile[64];
			strcpy(palFile, s_levelFile);
			palFile[len - 3] = 'P';
			palFile[len - 2] = 'A';
			palFile[len - 1] = 'L';
			s_palette = DXL2_Palette::get256(palFile);
			s_renderer->setPalette(s_palette);

			s_levelData = nullptr;
			if (LevelEditorData::convertLevelDataToEditor(levelData, s_palette, levelInf, levelObj))
			{
				s_levelData = LevelEditorData::getEditorLevelData();
			}
		}
	}

	void drawSectorPolygon(const EditorSector* sector, bool hover, u32 colorOverride)
	{
		if (!sector || sector->triangles.count == 0) { return; }
		
		const SectorTriangles* poly = &sector->triangles;
		const u32 triCount = poly->count;
		const Vec2f* vtx = poly->vtx.data();
		
		// Draw the sector polygon which has already been triangulated.
		u32 color = hover ? 0x40ff8020 : 0x40ff4020;
		if (colorOverride > 0) { color = colorOverride; }

		for (u32 p = 0; p < triCount; p++, vtx += 3)
		{
			TriColoredDraw2d::addTriangles(1, vtx, &color);
		}
	}

	void drawTexturedSectorPolygon(const EditorSector* sector, u32 color, bool floorTex)
	{
		if (!sector || sector->triangles.count == 0) { return; }

		const SectorTriangles* poly = &sector->triangles;
		const EditorTexture* texture = floorTex ? sector->floorTexture.tex : sector->ceilTexture.tex;
		const u32 triCount = poly->count;
		const Vec2f* vtx = poly->vtx.data();

		const f32* texOffsets = floorTex ? &sector->floorTexture.offsetX : &sector->ceilTexture.offsetX;
		const f32 scaleX = -8.0f / f32(texture->width);
		const f32 scaleZ = -8.0f / f32(texture->height);

		// Draw the sector polygon which has already been triangulated.
		for (u32 p = 0; p < triCount; p++, vtx += 3)
		{
			Vec2f uv[3] = { { (vtx[0].x - texOffsets[0]) * scaleX, (vtx[0].z - texOffsets[1]) * scaleZ },
							{ (vtx[1].x - texOffsets[0]) * scaleX, (vtx[1].z - texOffsets[1]) * scaleZ },
							{ (vtx[2].x - texOffsets[0]) * scaleX, (vtx[2].z - texOffsets[1]) * scaleZ } };
			TriTexturedDraw2d::addTriangles(1, vtx, uv, &color, texture->texture);
		}
	}

	void popupErrorMessage(const char* errorMessage)
	{
		strcpy(s_errorMessage, errorMessage);
		s_showError = true;
	}

	void showErrorPopup()
	{
		if (!s_showError) { return; }

		bool keepOpen = true;
		ImGui::OpenPopup("Error##Popup");

		ImGui::SetNextWindowSize({ 384, 128 });
		if (ImGui::BeginPopupModal("Error##Popup", &keepOpen))
		{
			DXL2_Markdown::draw(s_errorMessage);
			ImGui::EndPopup();
		}

		if (!keepOpen) { s_showError = false; }
	}
}
