#include "viewport.h"
#include "grid2d.h"
#include <TFE_System/math.h>
#include <TFE_Editor/LevelEditor/levelEditor.h>
#include <TFE_Editor/LevelEditor/levelEditorData.h>
#include <TFE_Editor/LevelEditor/sharedState.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
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
using namespace TFE_Editor;

namespace LevelEditor
{
	const f32 c_vertexSize = 2.0f;
	enum Highlight
	{
		HL_NONE = 0,
		HL_HOVERED,
		HL_SELECTED,
		HL_COUNT
	};

	enum SectorColor
	{
		SCOLOR_POLY_NORM     = 0x60402020,
		SCOLOR_POLY_HOVERED  = 0x60404020,
		SCOLOR_POLY_SELECTED = 0x60053060,

		SCOLOR_LINE_NORM     = 0xffffffff,
		SCOLOR_LINE_HOVERED  = 0xffffff80,
		SCOLOR_LINE_SELECTED = 0xff1080ff,

		SCOLOR_LINE_NORM_ADJ     = 0xff808080,
		SCOLOR_LINE_HOVERED_ADJ  = 0xffa0a060,
		SCOLOR_LINE_SELECTED_ADJ = 0xff0060a0,
	};
	enum VertexColor
	{
		VCOLOR_NORM     = 0xffae8653,
		VCOLOR_HOVERED  = 0xffffff20,
		VCOLOR_SELECTED = 0xff4a76ff
	};

	static const SectorColor c_sectorPolyClr[] = { SCOLOR_POLY_NORM, SCOLOR_POLY_HOVERED, SCOLOR_POLY_SELECTED };
	static const SectorColor c_sectorLineClr[] = { SCOLOR_LINE_NORM, SCOLOR_LINE_HOVERED, SCOLOR_LINE_SELECTED };
	static const SectorColor c_sectorLineClrAdjoin[] = { SCOLOR_LINE_NORM_ADJ, SCOLOR_LINE_HOVERED_ADJ, SCOLOR_LINE_SELECTED_ADJ };
	static const VertexColor c_vertexClr[] = { VCOLOR_NORM, VCOLOR_HOVERED, VCOLOR_SELECTED };

	#define AMBIENT(x) (x * 255/31) | ((x * 255/31)<<8) | ((x * 255/31)<<16) | (0xff << 24)
	static const u32 c_sectorTexClr[] =
	{
		AMBIENT(0), AMBIENT(1), AMBIENT(2), AMBIENT(3),
		AMBIENT(4), AMBIENT(5), AMBIENT(6), AMBIENT(7),
		AMBIENT(8), AMBIENT(9), AMBIENT(10), AMBIENT(11),
		AMBIENT(12), AMBIENT(13), AMBIENT(14), AMBIENT(15),
		AMBIENT(16), AMBIENT(17), AMBIENT(18), AMBIENT(19),
		AMBIENT(20), AMBIENT(21), AMBIENT(22), AMBIENT(23),
		AMBIENT(24), AMBIENT(25), AMBIENT(26), AMBIENT(27),
		AMBIENT(28), AMBIENT(29), AMBIENT(30), AMBIENT(31)
	};

	static RenderTargetHandle s_viewportRt = 0;
	static std::vector<Vec2f> s_transformedVtx;
	static std::vector<Vec2f> s_uvBuffer;

	SectorDrawMode s_sectorDrawMode = SDM_WIREFRAME;
	Vec2i s_viewportSize = { 0 };
	Vec3f s_viewportPos = { 0 };
	Vec4f s_viewportTrans2d = { 0 };
	f32 s_gridOpacity = 0.5f;
	f32 s_gridSize2d;
	f32 s_zoom2d = 0.25f;			// current zoom level in 2D.

	void renderLevel2D();
	void renderLevel3D();
	void renderSectorWalls2d(s32 layerStart, s32 layerEnd);
	void renderSectorPreGrid();
	void drawSector2d(const EditorSector* sector, Highlight highlight);
	void drawVertex2d(const Vec2f* pos, f32 scale, Highlight highlight);
	void drawVertex2d(const EditorSector* sector, s32 id, f32 extraScale, Highlight highlight);
	void drawWall2d(const EditorSector* sector, const EditorWall* wall, f32 extraScale, Highlight highlight, bool drawNormal = false);
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

		// Draw pre-grid polygons
		renderSectorPreGrid();

		// Draw the grid layer.
		if (s_editFlags & LEF_SHOW_GRID)
		{
			grid2d_computeScale(s_viewportSize, s_gridSize2d, s_zoom2d, s_viewportPos);
			grid2d_blitToScreen(s_gridOpacity);
		}

		// Draw the current layer.
		renderSectorWalls2d(s_curLayer, s_curLayer);

		// Draw the hovered and selected sectors.
		// Note they intentionally overlap if s_hoveredSector == s_selectedSector.
		if (s_hoveredSector && s_editMode == LEDIT_SECTOR)
		{
			drawSector2d(s_hoveredSector, HL_HOVERED);
		}
		if (s_selectedSector && s_editMode == LEDIT_SECTOR)
		{
			drawSector2d(s_selectedSector, HL_SELECTED);
		}

		// Draw the hovered and selected walls.
		if (s_hoveredWallSector && s_hoveredWallId >= 0)
		{
			drawWall2d(s_hoveredWallSector, &s_hoveredWallSector->walls[s_hoveredWallId], 1.5f, HL_HOVERED, /*draw normal*/true);
		}
		if (s_selectedWallSector && s_selectedWallId >= 0)
		{
			drawWall2d(s_selectedWallSector, &s_selectedWallSector->walls[s_selectedWallId], 1.5f, HL_SELECTED, /*draw normal*/true);
		}

		// Draw vertices.
		renderSectorVertices2d();

		// Draw the hovered and selected vertices.
		if (s_hoveredVtxSector && s_hoveredVtxId >= 0)
		{
			drawVertex2d(s_hoveredVtxSector, s_hoveredVtxId, 1.5f, HL_HOVERED);
		}
		if (s_selectedVtxSector && s_selectedVtxId >= 0)
		{
			drawVertex2d(s_selectedVtxSector, s_selectedVtxId, 1.5f, HL_SELECTED);
		}

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

	void renderTexturedSectorPolygon2d(const Polygon* poly, u32 color, EditorTexture* tex, const Vec2f& offset)
	{
		const size_t idxCount = poly->indices.size();
		if (idxCount)
		{
			const s32*    idxData = poly->indices.data();
			const Vec2f*  vtxData = poly->triVtx.data();
			const size_t vtxCount = poly->triVtx.size();

			s_uvBuffer.resize(vtxCount);
			s_transformedVtx.resize(vtxCount);
			Vec2f* transVtx = s_transformedVtx.data();
			Vec2f* uv = s_uvBuffer.data();
			for (size_t v = 0; v < vtxCount; v++, vtxData++)
			{
				transVtx[v] = { vtxData->x * s_viewportTrans2d.x + s_viewportTrans2d.y, vtxData->z * s_viewportTrans2d.z + s_viewportTrans2d.w };
				uv[v] = { (vtxData->x - offset.x) / 8.0f, (vtxData->z - offset.z) / 8.0f };
			}
			triDraw2D_addTextured(idxCount, (u32)vtxCount, transVtx, uv, idxData, color, tex->frames[0]);
		}
	}

	void drawWall2d(const EditorSector* sector, const EditorWall* wall, f32 extraScale, Highlight highlight, bool drawNormal)
	{
		Vec2f w0 = sector->vtx[wall->idx[0]];
		Vec2f w1 = sector->vtx[wall->idx[1]];

		// Transformed positions.
		s32 lineCount = 1;
		Vec2f line[4];
		line[0] = { w0.x * s_viewportTrans2d.x + s_viewportTrans2d.y, w0.z * s_viewportTrans2d.z + s_viewportTrans2d.w };
		line[1] = { w1.x * s_viewportTrans2d.x + s_viewportTrans2d.y, w1.z * s_viewportTrans2d.z + s_viewportTrans2d.w };

		if (drawNormal)
		{
			lineCount++;

			Vec2f midPoint = { (line[0].x + line[1].x)*0.5f, (line[0].z + line[1].z)*0.5f };
			Vec2f dir = { -(line[1].z - line[0].z), line[1].x - line[0].x };
			dir = TFE_Math::normalize(&dir);

			line[2] = midPoint;
			line[3] = { midPoint.x + dir.x * 8.0f, midPoint.z + dir.z * 8.0f };
		}

		u32 baseColor;
		if (s_curLayer != sector->layer)
		{
			u32 alpha = 0x40 / (s_curLayer - sector->layer);
			baseColor = 0x00808000 | (alpha << 24);
		}
		else
		{
			baseColor = wall->adjoinId < 0 ? c_sectorLineClr[highlight] : c_sectorLineClrAdjoin[highlight];
		}
		u32 color[4] = { baseColor, baseColor, baseColor, baseColor };

		TFE_RenderShared::lineDraw2d_addLines(lineCount, 1.25f * extraScale, line, color);
	}

	void drawSector2d(const EditorSector* sector, Highlight highlight)
	{
		// Draw a background polygon to help sectors stand out a bit.
		if (sector->layer == s_curLayer)
		{
			if (s_sectorDrawMode == SDM_WIREFRAME || highlight != HL_NONE)
			{
				renderSectorPolygon2d(&sector->poly, c_sectorPolyClr[highlight]);
			}
		}

		// Draw lines.
		const size_t wallCount = sector->walls.size();
		const EditorWall* wall = sector->walls.data();
		const Vec2f* vtx = sector->vtx.data();
		for (size_t w = 0; w < wallCount; w++, wall++)
		{
			// Skip hovered or selected walls.
			if ((s_hoveredWallSector == sector && s_hoveredWallId == w) ||
				(s_selectedWallSector == sector && s_selectedWallId == w))
			{
				continue;
			}

			drawWall2d(sector, wall, 1.0f, highlight);
		}
	}

	void renderSectorPreGrid()
	{
		if (s_sectorDrawMode != SDM_TEXTURED_FLOOR && s_sectorDrawMode != SDM_TEXTURED_CEIL && s_sectorDrawMode != SDM_LIGHTING) { return; }

		const size_t count = s_level.sectors.size();
		const EditorSector* sector = s_level.sectors.data();
		for (size_t s = 0; s < count; s++, sector++)
		{
			if (sector->layer != s_curLayer) { continue; }
			const u32 colorIndex = (s_editFlags & LEF_FULLBRIGHT) && s_sectorDrawMode != SDM_LIGHTING ? 31 : sector->ambient;

			if (s_sectorDrawMode == SDM_LIGHTING)
			{
				renderSectorPolygon2d(&sector->poly, c_sectorTexClr[colorIndex]);
			}
			else if (s_sectorDrawMode == SDM_TEXTURED_FLOOR)
			{
				renderTexturedSectorPolygon2d(&sector->poly, c_sectorTexClr[colorIndex], (EditorTexture*)getAssetData(sector->floorTex.handle), sector->floorTex.offset);
			}
			else if (s_sectorDrawMode == SDM_TEXTURED_CEIL)
			{
				renderTexturedSectorPolygon2d(&sector->poly, c_sectorTexClr[colorIndex], (EditorTexture*)getAssetData(sector->ceilTex.handle), sector->ceilTex.offset);
			}
		}

		TFE_RenderShared::triDraw2d_draw();
		TFE_RenderShared::triDraw2d_begin(s_viewportSize.x, s_viewportSize.z);
	}

	void renderSectorWalls2d(s32 layerStart, s32 layerEnd)
	{
		if (layerEnd < layerStart) { return; }

		const size_t count = s_level.sectors.size();
		const EditorSector* sector = s_level.sectors.data();
		for (size_t s = 0; s < count; s++, sector++)
		{
			if (sector->layer < layerStart || sector->layer > layerEnd) { continue; }
			if ((sector == s_hoveredSector || sector == s_selectedSector) && s_editMode == LEDIT_SECTOR) { continue; }

			drawSector2d(sector, HL_NONE);
		}
	}

	void drawVertex2d(const Vec2f* pos, f32 scale, Highlight highlight)
	{
		u32 color = c_vertexClr[highlight];
		u32 colors[] = { color, color };

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

	void drawVertex2d(const EditorSector* sector, s32 id, f32 extraScale, Highlight highlight)
	{
		const Vec2f* pos = &sector->vtx[id];
		const f32 scale = std::min(1.0f, 1.0f / s_zoom2d) * c_vertexSize * extraScale;
		drawVertex2d(pos, scale, highlight);
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
				// Skip drawing hovered/selected vertices.
				if ((sector == s_hoveredVtxSector  && v == s_hoveredVtxId) ||
					(sector == s_selectedVtxSector && v == s_selectedVtxId))
				{
					continue;
				}
				drawVertex2d(vtx, scale, HL_NONE);
			}
		}
	}
}
