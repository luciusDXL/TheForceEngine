#include "viewport.h"
#include "grid2d.h"
#include <TFE_Editor/LevelEditor/levelEditor.h>
#include <TFE_Editor/LevelEditor/levelEditorData.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderShared/lineDraw2d.h>
#include <TFE_RenderShared/triDraw2d.h>
#include <TFE_System/system.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <algorithm>
#include <vector>

using namespace TFE_RenderShared;

namespace LevelEditor
{
	const f32 c_vertexSize = 2.0f;

	static RenderTargetHandle s_viewportRt = 0;
	static std::vector<Vec2f> s_transformedVtx;

	Vec2i s_viewportSize = { 0 };
	Vec3f s_viewportPos = { 0 };
	Vec4f s_viewportTrans2d = { 0 };
	f32 s_gridOpacity = 0.5f;
	f32 s_gridSize2d;
	f32 s_zoom2d = 0.25f;			// current zoom level in 2D.

	extern EditorLevel s_level;
	extern u32 s_editFlags;
	extern s32 s_curLayer;

	void renderLevel2D();
	void renderLevel3D();
	void renderSectorWalls2d(s32 layerStart, s32 layerEnd);
	void renderSectorVertices2d();

	void viewport_init()
	{
		grid2d_init();
		tri2d_init();
	}

	void viewport_destroy()
	{
		TFE_RenderBackend::freeRenderTarget(s_viewportRt);
		grid2d_destroy();
		tri2d_destroy();
		s_viewportRt = 0;
	}

	void viewport_render(EditorView view)
	{
		if (!s_viewportRt) { return; }

		const f32 clearColor[] = { 0.05f, 0.06f, 0.1f, 1.0f };
		TFE_RenderBackend::bindRenderTarget(s_viewportRt);
		TFE_RenderBackend::clearRenderTarget(s_viewportRt, clearColor);

		if (view == EDIT_VIEW_2D) { renderLevel2D(); }
		else { renderLevel3D(); }

		TFE_RenderBackend::unbindRenderTarget();
	}

	void viewport_update(s32 resvWidth, s32 resvHeight)
	{
		DisplayInfo info;
		TFE_RenderBackend::getDisplayInfo(&info);
		Vec2i newSize = { (s32)info.width - resvWidth, (s32)info.height - resvHeight };
		if (newSize.x != s_viewportSize.x || newSize.z != s_viewportSize.z || !s_viewportRt)
		{
			s_viewportSize = newSize;
			TFE_RenderBackend::freeRenderTarget(s_viewportRt);
			s_viewportRt = TFE_RenderBackend::createRenderTarget(s_viewportSize.x, s_viewportSize.z, true);
		}
	}

	const TextureGpu* viewport_getTexture()
	{
		return TFE_RenderBackend::getRenderTargetTexture(s_viewportRt);
	}

	//////////////////////////////////////////
	// Internal
	//////////////////////////////////////////
	void computeViewportTransform2d()
	{
		// Compute the scale and offset.
		f32 pixelsToWorldUnits = s_zoom2d;
		s_viewportTrans2d.x = 1.0f / pixelsToWorldUnits;
		s_viewportTrans2d.z = -1.0f / pixelsToWorldUnits;

		s_viewportTrans2d.y = -s_viewportPos.x * s_viewportTrans2d.x;
		s_viewportTrans2d.w = s_viewportPos.z * s_viewportTrans2d.z;
	}

	void renderLevel2D()
	{
		// Prepare for drawing.
		computeViewportTransform2d();
		TFE_RenderShared::lineDraw2d_begin(s_viewportSize.x, s_viewportSize.z);
		TFE_RenderShared::triDraw2d_begin(s_viewportSize.x, s_viewportSize.z);

		// Draw lower layers, if enabled.
		if (s_editFlags & LEF_SHOW_LOWER_LAYERS)
		{
			renderSectorWalls2d(s_level.layerRange[0], s_curLayer - 1);
		}

		// Draw the grid layer.
		if (s_editFlags & LEF_SHOW_GRID)
		{
			grid2d_blitToScreen(s_viewportSize, s_gridSize2d, s_zoom2d, s_viewportPos, s_gridOpacity);
		}

		// Draw the current layer.
		renderSectorWalls2d(s_curLayer, s_curLayer);

		// Draw vertices.
		renderSectorVertices2d();

		// Submit.
		TFE_RenderShared::triDraw2d_draw();
		TFE_RenderShared::lineDraw2d_drawLines();
	}

	void renderLevel3D()
	{
	}

	void renderSectorPolygon2d(const Polygon* poly, u32 color)
	{
		const size_t idxCount = poly->indices.size();
		if (idxCount)
		{
			const s32*    idxData = poly->indices.data();
			const Vec2f*  vtxData = poly->triVtx.data();
			const size_t vtxCount = poly->triVtx.size();

			s_transformedVtx.resize(vtxCount);
			Vec2f* transVtx = s_transformedVtx.data();
			for (size_t v = 0; v < vtxCount; v++, vtxData++)
			{
				transVtx[v] = { vtxData->x * s_viewportTrans2d.x + s_viewportTrans2d.y, vtxData->z * s_viewportTrans2d.z + s_viewportTrans2d.w };
			}
			triDraw2d_addColored(idxCount, (u32)vtxCount, transVtx, idxData, color);
		}
	}

	void renderSectorWalls2d(s32 layerStart, s32 layerEnd)
	{
		if (layerEnd < layerStart) { return; }

		const size_t count = s_level.sectors.size();
		const EditorSector* sector = s_level.sectors.data();
		for (size_t s = 0; s < count; s++, sector++)
		{
			if (sector->layer < layerStart || sector->layer > layerEnd) { continue; }

			// Draw a background polygon to help sectors stand out a bit.
			if (sector->layer == s_curLayer)
			{
				renderSectorPolygon2d(&sector->poly, 0x60402020);
			}

			// Draw lines.
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

	void drawVertex(const Vec2f* pos, f32 scale, const u32* color)
	{
		u32 colors[] = { color[0], color[1] };

		const Vec2f p0 = { pos->x * s_viewportTrans2d.x + s_viewportTrans2d.y, pos->z * s_viewportTrans2d.z + s_viewportTrans2d.w };
		const Vec2f vtx[]=
		{
			{ p0.x - scale, p0.z - scale },
			{ p0.x + scale, p0.z - scale },
			{ p0.x + scale, p0.z + scale },
			{ p0.x - scale, p0.z + scale },
			{ p0.x - scale, p0.z - scale },
		};
		TFE_RenderShared::lineDraw2d_addLine(1.5f, &vtx[0], colors);
		TFE_RenderShared::lineDraw2d_addLine(1.5f, &vtx[1], colors);
		TFE_RenderShared::lineDraw2d_addLine(1.5f, &vtx[2], colors);
		TFE_RenderShared::lineDraw2d_addLine(1.5f, &vtx[3], colors);
	}

	void renderSectorVertices2d()
	{
		const u32 color[4] = { 0xffae8653, 0xffae8653, 0xff51331a, 0xff51331a };
		const u32 colorSelected[4] = { 0xffffc379, 0xffffc379, 0xff764a26, 0xff764a26 };
		const f32 scale = std::min(1.0f, 1.0f/s_zoom2d) * c_vertexSize;

		const size_t sectorCount = s_level.sectors.size();
		const EditorSector* sector = s_level.sectors.data();
		for (size_t s = 0; s < sectorCount; s++, sector++)
		{
			if (sector->layer != s_curLayer) { continue; }

			const size_t vtxCount = sector->vtx.size();
			const Vec2f* vtx = sector->vtx.data();
			for (size_t v = 0; v < vtxCount; v++, vtx++)
			{
				drawVertex(vtx, scale, color);
			}
		}
	}
}
