#include "levelEditor.h"
#include "levelEditorData.h"
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
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderShared/lineDraw2d.h>
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/parser.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
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
		LEDIT_SMART,	// vertex + wall + height "smart" edit.
		LEDIT_VERTEX,	// vertex only
		LEDIT_WALL,		// wall only in 2D, wall + floor/ceiling in 3D
		LEDIT_SECTOR,
		LEDIT_ENTITY
	};

	enum LevelEditFlags
	{
		LEF_NONE = 0,
		LEF_SHOW_GRID         = FLAG_BIT(0),
		LEF_SHOW_LOWER_LAYERS = FLAG_BIT(1),
		LEF_SHOW_INF_COLORS   = FLAG_BIT(2),

		LEF_DEFAULT = LEF_SHOW_GRID | LEF_SHOW_LOWER_LAYERS | LEF_SHOW_INF_COLORS
	};

	// The TFE Level Editor format is different than the base format and contains extra 
	// metadata, etc.
	// So existing levels need to be loaded into that format.
	// If the correct format already exists, though, then it is loaded directly.
	static EditorLevel s_level = {};

	static EditorView s_view = EDIT_VIEW_2D;
	static u32 s_editFlags = LEF_DEFAULT;
	static Vec2i s_viewportSize = { 0 };
	static Vec3f s_viewportPos = { 0 };
	static Vec4f s_viewportTrans2d = { 0 };
	static s32 s_curLayer = 0;
	static Vec2i s_editWinPos = { 0, 67 };
	static Vec2i s_editWinSize = { 0 };
	static Vec2f s_editWinMapCorner = { 0 };
	static f32 s_gridSize = 32.0f;
	static f32 s_gridSize2d = 32.0f;	// current finest grid visible in 2D.
	static f32 s_zoom2d = 0.25f;			// current zoom level in 2D.
	static RenderTargetHandle s_viewportRt = 0;

	////////////////////////////////////////////////////////
	// Forward Declarations
	////////////////////////////////////////////////////////
	void updateViewport();
	void renderViewport();
	void levelEditWinBegin();
	void levelEditWinEnd();
	void cameraControl2d(s32 mx, s32 my);

	////////////////////////////////////////////////////////
	// Public API
	////////////////////////////////////////////////////////
	bool init(Asset* asset)
	{
		// Cleanup any existing level data.
		destroy();
		// Load the new level.
		if (!loadLevelFromAsset(asset, &s_level))
		{
			return false;
		}
		updateViewport();
		s_gridSize = 32.0f;

		//s_viewportPos.x = (s_level.bounds[0].x + s_level.bounds[1].x) * 0.5f;
		//s_viewportPos.z = (s_level.bounds[0].z + s_level.bounds[1].z) * 0.5f;
		s_viewportPos = s_level.bounds[0];
		s_curLayer = s_level.layerRange[1];

		TFE_RenderShared::init(false);
		return true;
	}

	void destroy()
	{
		s_level.sectors.clear();
		TFE_RenderBackend::freeRenderTarget(s_viewportRt);
		s_viewportRt = 0;
		TFE_RenderShared::destroy();
	}

	void updateWindowControls()
	{
		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		if (!TFE_Input::relativeModeEnabled() && (mx < s_editWinPos.x || mx >= s_editWinPos.x + s_editWinSize.x || my < s_editWinPos.z || my >= s_editWinPos.z + s_editWinSize.z))
		{
			return;
		}

		if (s_view == EDIT_VIEW_2D)
		{
			cameraControl2d(mx, my);
		}
	}

	void update()
	{
		updateWindowControls();

		updateViewport();
		renderViewport();

		levelEditWinBegin();
		{
			const TextureGpu* texture = TFE_RenderBackend::getRenderTargetTexture(s_viewportRt);
			ImGui::ImageButton(TFE_RenderBackend::getGpuPtr(texture), { (f32)s_viewportSize.x, (f32)s_viewportSize.z },
				{ 0.0f, 0.0f }, { 1.0f, 1.0f }, 0, { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
			const ImVec2 itemPos = ImGui::GetItemRectMin();
			s_editWinMapCorner = { itemPos.x, itemPos.y };
		}
		levelEditWinEnd();
	}

	////////////////////////////////////////////////////////
	// Internal
	////////////////////////////////////////////////////////
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

			s_zoom2d = std::max(s_zoom2d - f32(dy) * s_zoom2d * 0.1f, 0.001f);
			s_zoom2d = floorf(s_zoom2d * 1000.0f) * 0.001f;

			// We want worldPos to stay put as we zoom
			Vec2f newWorldPos;
			newWorldPos.x = s_viewportPos.x + f32(relX) * s_zoom2d;
			newWorldPos.z = s_viewportPos.z + f32(relY) * s_zoom2d;
			s_viewportPos.x += (worldPos.x - newWorldPos.x);
			s_viewportPos.z += (worldPos.z - newWorldPos.z);
		}
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
		
	void computeViewportTransform2d()
	{
		// Compute the scale and offset.
		f32 pixelsToWorldUnits = s_zoom2d;
		s_viewportTrans2d.x = 1.0f / pixelsToWorldUnits;
		s_viewportTrans2d.z = 1.0f / pixelsToWorldUnits;

		s_viewportTrans2d.y = -s_viewportPos.x * s_viewportTrans2d.x;
		s_viewportTrans2d.w = -s_viewportPos.z * s_viewportTrans2d.z;
	}

	void renderSectorWalls2d(s32 layerStart, s32 layerEnd)
	{
		if (layerEnd < layerStart) { return; }

		const size_t count = s_level.sectors.size();
		const EditorSector* sector = s_level.sectors.data();
		for (size_t s = 0; s < count; s++, sector++)
		{
			if (sector->layer < layerStart || sector->layer > layerEnd) { continue; }

			const size_t wallCount = sector->walls.size();
			const EditorWall* wall = sector->walls.data();
			const Vec2f* vtx = sector->vtx.data();
			for (size_t w = 0; w < wallCount; w++, wall++)
			{
				Vec2f w0 = vtx[wall->idx[0]];
				Vec2f w1 = vtx[wall->idx[1]];

				// Transformed positions.
				Vec2f line[2];
				line[0] = { w0.x * s_viewportTrans2d.x + s_viewportTrans2d.y, w0.z * s_viewportTrans2d.z + s_viewportTrans2d.w };
				line[1] = { w1.x * s_viewportTrans2d.x + s_viewportTrans2d.y, w1.z * s_viewportTrans2d.z + s_viewportTrans2d.w };

				u32 baseColor;
				if (s_curLayer != sector->layer)
				{
					u32 alpha = 0x40 / (s_curLayer - sector->layer);
					baseColor = 0x00808000 | (alpha << 24);
				}
				else
				{
					baseColor = wall->adjoinId < 0 ? 0xffffffff : 0xff808080;
				}
				u32 color[2] = { baseColor, baseColor };

				TFE_RenderShared::lineDraw2d_addLine(1.25f, line, color);
			}
		}
	}

	void renderLevel2D()
	{
		// Compute the best base level of detail for the 2D grid based on the zoom.
		// Note the grid will never be finer than the current grid size.
		s_gridSize2d = floorf(log2f(1.0f / s_zoom2d));
		s_gridSize2d = max(s_gridSize, powf(2.0f, s_gridSize2d));

		// Prepare for drawing.
		computeViewportTransform2d();
		TFE_RenderShared::lineDraw2d_begin(s_viewportSize.x, s_viewportSize.z);

		// Draw lower layers, if enabled.
		if (s_editFlags & LEF_SHOW_LOWER_LAYERS)
		{
			renderSectorWalls2d(s_level.layerRange[0], s_curLayer - 1);
		}

		// Draw the grid layer.
		if (s_editFlags & LEF_SHOW_GRID)
		{
		}

		// Draw the current layer.
		renderSectorWalls2d(s_curLayer, s_curLayer);

		// Submit.
		TFE_RenderShared::lineDraw2d_drawLines();
	}

	void renderLevel3D()
	{
	}

	void renderViewport()
	{
		if (!s_viewportRt) { return; }

		const f32 clearColor[] = { 0.05f, 0.06f, 0.1f, 1.0f };
		TFE_RenderBackend::bindRenderTarget(s_viewportRt);
		TFE_RenderBackend::clearRenderTarget(s_viewportRt, clearColor);

		if (s_view == EDIT_VIEW_2D) { renderLevel2D(); }
		else { renderLevel3D(); }
		
		TFE_RenderBackend::unbindRenderTarget();
	}

	void updateViewport()
	{
		DisplayInfo info;
		TFE_RenderBackend::getDisplayInfo(&info);
		Vec2i newSize = { (s32)info.width - (s32)UI_SCALE(480) - 16, (s32)info.height - (s32)UI_SCALE(68) - 18 };
		if (newSize.x != s_viewportSize.x || newSize.z != s_viewportSize.z || !s_viewportRt)
		{
			s_viewportSize = newSize;
			TFE_RenderBackend::freeRenderTarget(s_viewportRt);
			s_viewportRt = TFE_RenderBackend::createRenderTarget(s_viewportSize.x, s_viewportSize.z, true);
		}
	}
}
