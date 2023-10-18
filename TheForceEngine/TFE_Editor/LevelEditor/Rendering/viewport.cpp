#include "viewport.h"
#include "grid2d.h"
#include "grid3d.h"
#include <TFE_System/math.h>
#include <TFE_Editor/LevelEditor/levelEditor.h>
#include <TFE_Editor/LevelEditor/levelEditorData.h>
#include <TFE_Editor/LevelEditor/sharedState.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderShared/lineDraw2d.h>
#include <TFE_RenderShared/triDraw2d.h>
#include <TFE_RenderShared/triDraw3d.h>
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
		VCOLOR_SELECTED = 0xff4a76ff,
		VCOLOR_HOV_AND_SEL = 0xff94ec8f,
	};

	static const SectorColor c_sectorPolyClr[] = { SCOLOR_POLY_NORM, SCOLOR_POLY_HOVERED, SCOLOR_POLY_SELECTED };
	static const SectorColor c_sectorLineClr[] = { SCOLOR_LINE_NORM, SCOLOR_LINE_HOVERED, SCOLOR_LINE_SELECTED };
	static const SectorColor c_sectorLineClrAdjoin[] = { SCOLOR_LINE_NORM_ADJ, SCOLOR_LINE_HOVERED_ADJ, SCOLOR_LINE_SELECTED_ADJ };
	static const VertexColor c_vertexClr[] = { VCOLOR_NORM, VCOLOR_HOVERED, VCOLOR_SELECTED, VCOLOR_HOV_AND_SEL };

	#define AMBIENT(x) (u32)(x * 255/31) | ((x * 255/31)<<8) | ((x * 255/31)<<16) | (0xff << 24)
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
	f32 s_gridSize = 0.0625f;
	f32 s_zoom2d = 0.25f;			// current zoom level in 2D.
	f32 s_gridHeight = 0.0f;

	void renderLevel2D();
	void renderLevel3D();
	void renderSectorWalls2d(s32 layerStart, s32 layerEnd);
	void renderSectorPreGrid();
	void drawSector2d(const EditorSector* sector, Highlight highlight);
	void drawVertex2d(const Vec2f* pos, f32 scale, Highlight highlight);
	void drawVertex2d(const EditorSector* sector, s32 id, f32 extraScale, Highlight highlight);
	void drawWall2d(const EditorSector* sector, const EditorWall* wall, f32 extraScale, Highlight highlight, bool drawNormal = false);
	void renderSectorVertices2d();
	void drawBox(const Vec3f* center, f32 side, f32 lineWidth, u32 color);
	void drawSolidBox(const Vec3f* center, f32 side, u32 color);

	void viewport_init()
	{
		grid2d_init();
		tri2d_init();
		tri3d_init();
		grid3d_init();
		TFE_RenderShared::line3d_init();
	}

	void viewport_destroy()
	{
		TFE_RenderBackend::freeRenderTarget(s_viewportRt);
		grid2d_destroy();
		tri2d_destroy();
		tri3d_destroy();
		grid3d_destroy();
		TFE_RenderShared::line3d_destroy();
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
			grid2d_computeScale(s_viewportSize, s_gridSize, s_zoom2d, s_viewportPos);
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
		bool alsoHovered = false;
		if (s_selectedVertices.size())
		{
			const size_t count = s_selectedVertices.size();
			const u64* list = s_selectedVertices.data();
			for (size_t i = 0; i < count; i++)
			{
				s32 featureIndex;
				bool overlapped;
				EditorSector* sector = unpackID(list[i], &featureIndex, &overlapped);
				if (overlapped || !sector) { continue; }

				const bool thisAlsoHovered = s_hoveredVtxId == featureIndex && s_hoveredVtxSector == sector;
				if (thisAlsoHovered) { alsoHovered = true; }
				drawVertex2d(sector, featureIndex, 1.5f, thisAlsoHovered ? Highlight(HL_SELECTED + 1) : HL_SELECTED);
			}
		}
		if (s_hoveredVtxSector && s_hoveredVtxId >= 0 && !alsoHovered)
		{
			drawVertex2d(s_hoveredVtxSector, s_hoveredVtxId, 1.5f, HL_HOVERED);
		}

		// Submit.
		TFE_RenderShared::triDraw2d_draw();
		TFE_RenderShared::lineDraw2d_drawLines();
	}

	void renderCoordinateAxis()
	{
		const f32 len = 4096.0f;
		const f32 lenY = 50.0f;
		const u32 xAxisClr = 0xc00000ff;
		const u32 yAxisClr = 0xc000ff00;
		const u32 zAxisClr = 0xc0ff0000;

		const u32 axisColor[] = { /*left*/xAxisClr, yAxisClr, zAxisClr, /*right*/xAxisClr, yAxisClr, zAxisClr };
		const Vec3f axis[] =
		{
			// Left
			{0.1f, 0.0f, 0.0f}, {len, 0.0f, 0.0f},
			{0.0f, 0.1f, 0.0f}, {0.0f, lenY, 0.0f},
			{0.0f, 0.0f, 0.1f}, {0.0f, 0.0f, len},
			// Right
			{-len, 0.0f, 0.0f}, {0.1f, 0.0f, 0.0f},
			{0.0f,-lenY, 0.0f}, {0.0f, 0.1f, 0.0f},
			{0.0f, 0.0f, -len}, {0.0f, 0.0f, 0.1f},
		};
		TFE_RenderShared::lineDraw3d_addLines(6, 3.0f, axis, axisColor);
	}

	static bool s_gridAutoAdjust = true;

	const EditorTexture* calculateTextureCoords(const EditorWall* wall, const LevelTexture* ltex, f32 wallLengthTexels, f32 partHeight, bool flipHorz, Vec2f* uvCorners)
	{
		const EditorTexture* tex = (EditorTexture*)getAssetData(ltex->handle);
		if (!tex) { return nullptr; }
		Vec2f texScale = { 1.0f / f32(tex->width), 1.0f / f32(tex->height) };

		uvCorners[0] = { ltex->offset.x * 8.0f, (ltex->offset.z + partHeight) * 8.0f };
		uvCorners[1] = { uvCorners[0].x + wallLengthTexels, ltex->offset.z * 8.0f };
		uvCorners[0].x *= texScale.x;
		uvCorners[1].x *= texScale.x;
		uvCorners[0].z *= texScale.z;
		uvCorners[1].z *= texScale.z;
		if (flipHorz)
		{
			uvCorners[0].x = 1.0f - uvCorners[0].x;
			uvCorners[1].x = 1.0f - uvCorners[1].x;
		}
		return tex;
	}

	const EditorTexture* calculateSignTextureCoords(const EditorWall* wall, const LevelTexture* baseTex, const LevelTexture* ltex, f32 wallLengthTexels, f32 partHeight, bool flipHorz, Vec2f* uvCorners)
	{
		const EditorTexture* tex = (EditorTexture*)getAssetData(ltex->handle);
		const Vec2f texScale = { 1.0f / f32(tex->width), 1.0f / f32(tex->height) };

		Vec2f offset = { baseTex->offset.x - ltex->offset.x, -TFE_Math::fract(std::max(baseTex->offset.z, 0.0f)) + ltex->offset.z };

		uvCorners[0] = { offset.x * 8.0f, (offset.z + partHeight) * 8.0f };
		uvCorners[1] = { uvCorners[0].x + wallLengthTexels, offset.z * 8.0f };
		uvCorners[0].x *= texScale.x;
		uvCorners[1].x *= texScale.x;
		uvCorners[0].z *= texScale.z;
		uvCorners[1].z *= texScale.z;
		if (flipHorz)
		{
			uvCorners[0].x = 1.0f - uvCorners[0].x;
			uvCorners[1].x = 1.0f - uvCorners[1].x;
		}
		return tex;
	}

	void drawWallLines3D_Highlighted(const EditorSector* sector, const EditorSector* next, const EditorWall* wall, f32 width, Highlight highlight, bool halfAlpha)
	{
		const Vec2f& v0 = sector->vtx[wall->idx[0]];
		const Vec2f& v1 = sector->vtx[wall->idx[1]];

		u32 color = c_sectorLineClr[highlight];
		if (halfAlpha)
		{
			color &= 0x00ffffff;
			color |= 0x80000000;
		}

		Vec3f lines[] =
		{
			{ v0.x, sector->floorHeight, v0.z },
			{ v1.x, sector->floorHeight, v1.z },
			{ v0.x, sector->ceilHeight, v0.z },
			{ v1.x, sector->ceilHeight, v1.z },

			{ v0.x, sector->floorHeight, v0.z },
			{ v0.x, sector->ceilHeight,  v0.z },
			{ v1.x, sector->floorHeight, v1.z },
			{ v1.x, sector->ceilHeight,  v1.z },
		};
		u32 colors[4] = { color, color, color, color };
		TFE_RenderShared::lineDraw3d_addLines(4, width, lines, colors);

		if (next)
		{
			// Top
			if (next->ceilHeight < sector->ceilHeight)
			{
				Vec3f line0[] =
				{ { v0.x, next->ceilHeight, v0.z },
				  { v1.x, next->ceilHeight, v1.z } };
				TFE_RenderShared::lineDraw3d_addLine(width, line0, &color);
			}
			// Bottom
			if (next->floorHeight > sector->floorHeight)
			{
				Vec3f line0[] =
				{ { v0.x, next->floorHeight, v0.z },
				  { v1.x, next->floorHeight, v1.z } };
				TFE_RenderShared::lineDraw3d_addLine(width, line0, &color);
			}
		}
	}

	void drawFlat3D_Highlighted(const EditorSector* sector, HitPart part, f32 width, Highlight highlight, bool halfAlpha, bool skipLines)
	{
		if (part != HP_FLOOR && part != HP_CEIL) { return; }

		u32 color = c_sectorLineClr[highlight];
		if (halfAlpha)
		{
			color &= 0x00ffffff;
			color |= 0x80000000;
		}

		f32 height = part == HP_FLOOR ? sector->floorHeight : sector->ceilHeight;
		if (!skipLines)
		{
			const size_t wallCount = sector->walls.size();
			const EditorWall* wall = sector->walls.data();
			for (size_t w = 0; w < wallCount; w++, wall++)
			{
				const Vec2f* v0 = &sector->vtx[wall->idx[0]];
				const Vec2f* v1 = &sector->vtx[wall->idx[1]];

				Vec3f lines[2] =
				{
					{ v0->x, height, v0->z },
					{ v1->x, height, v1->z },
				};
				TFE_RenderShared::lineDraw3d_addLine(width, lines, &color);
			}
		}

		Vec3f flatVtx[512];
		const size_t vtxCount = sector->poly.triVtx.size();
		const Vec2f* vtx = sector->poly.triVtx.data();
		for (size_t v = 0; v < vtxCount; v++)
		{
			flatVtx[v] = { vtx[v].x, height, vtx[v].z };
		}
		triDraw3d_addColored(TRIMODE_BLEND, sector->poly.triIdx.size(), vtxCount, flatVtx, sector->poly.triIdx.data(), c_sectorPolyClr[highlight], part == HP_CEIL);
	}

	void drawWallColor3d_Highlighted(const EditorSector* sector, const Vec2f* vtx, const EditorWall* wall, u32 color, HitPart part)
	{
		const Vec2f* v0 = &vtx[wall->idx[0]];
		const Vec2f* v1 = &vtx[wall->idx[1]];

		if (wall->adjoinId < 0 && (part == HP_COUNT || part == HP_MID))
		{
			// No adjoin - a single quad.
			Vec3f corners[] = { {v0->x, sector->ceilHeight, v0->z}, {v1->x, sector->floorHeight, v1->z} };
			triDraw3d_addQuadColored(TRIMODE_BLEND, corners, color);
		}
		else if (wall->adjoinId >= 0)
		{
			// lower
			f32 nextFloor = s_level.sectors[wall->adjoinId].floorHeight;
			if (nextFloor > sector->floorHeight && (part == HP_COUNT || part == HP_BOT))
			{
				Vec3f corners[] = { {v0->x, nextFloor, v0->z}, {v1->x, sector->floorHeight, v1->z} };
				triDraw3d_addQuadColored(TRIMODE_BLEND, corners, color);
			}

			// upper
			f32 nextCeil = s_level.sectors[wall->adjoinId].ceilHeight;
			if (nextCeil < sector->ceilHeight && (part == HP_COUNT || part == HP_TOP))
			{
				Vec3f corners[] = { {v0->x, sector->ceilHeight, v0->z}, {v1->x, nextCeil, v1->z} };
				triDraw3d_addQuadColored(TRIMODE_BLEND, corners, color);
			}

			// mask wall or highlight.
			if (part == HP_FLOOR || part == HP_CEIL || part == HP_MID)
			{
				const f32 ceil  = std::min(nextCeil, sector->ceilHeight);
				const f32 floor = std::max(nextFloor, sector->floorHeight);
				Vec3f corners[] = { {v0->x, ceil, v0->z}, {v1->x, floor, v1->z} };
				triDraw3d_addQuadColored(TRIMODE_BLEND, corners, color);
			}
		}
	}

	void drawWallLines3D(const EditorSector* sector, const EditorSector* next, const EditorWall* wall, f32 width, Highlight highlight, bool halfAlpha)
	{
		f32 bias = 1.0f / 1024.0f;
		f32 floorBias = (s_camera.pos.y >= sector->floorHeight) ? bias : -bias;
		f32 ceilBias  = (s_camera.pos.y <= sector->ceilHeight) ? -bias : bias;

		const Vec2f& v0 = sector->vtx[wall->idx[0]];
		const Vec2f& v1 = sector->vtx[wall->idx[1]];

		u32 color = c_sectorLineClr[highlight];
		if (halfAlpha)
		{
			color &= 0x00ffffff;
			color |= 0x80000000;
		}

		if ((s_editFlags & LEF_SECTOR_EDGES) || !next || next->floorHeight != sector->floorHeight || s_camera.pos.y > sector->floorHeight)
		{
			Vec3f line0[] =
			{ { v0.x, sector->floorHeight + floorBias, v0.z },
			  { v1.x, sector->floorHeight + floorBias, v1.z } };
			TFE_RenderShared::lineDraw3d_addLine(width, line0, &color);
		}
		if ((s_editFlags & LEF_SECTOR_EDGES) || !next || next->ceilHeight != sector->ceilHeight || s_camera.pos.y < sector->ceilHeight)
		{
			Vec3f line1[] =
			{ { v0.x, sector->ceilHeight + ceilBias, v0.z },
			  { v1.x, sector->ceilHeight + ceilBias, v1.z } };
			TFE_RenderShared::lineDraw3d_addLine(width, line1, &color);
		}
		
		Vec2f vtx[] = { v0, v1 };
		s32 count = highlight == HL_NONE ? 1 : 2;
		for (s32 v = 0; v < count; v++)
		{
			if (s_editFlags & LEF_SECTOR_EDGES)
			{
				Vec3f line2[] =
				{ { vtx[v].x, sector->floorHeight + floorBias, vtx[v].z },
				  { vtx[v].x, sector->ceilHeight + ceilBias, vtx[v].z } };
				TFE_RenderShared::lineDraw3d_addLine(width, line2, &color);
			}
			else
			{
				// Show edges only where geometry exists.
				if (wall->adjoinId < 0)
				{
					Vec3f line2[] =
					{ { vtx[v].x, sector->floorHeight + floorBias, vtx[v].z },
					  { vtx[v].x, sector->ceilHeight + ceilBias, vtx[v].z } };
					TFE_RenderShared::lineDraw3d_addLine(width, line2, &color);
				}
				else
				{
					// Top
					if (next->ceilHeight < sector->ceilHeight)
					{
						f32 topBias = (s_camera.pos.y <= next->ceilHeight) ? -bias : bias;

						Vec3f line0[] =
						{ { vtx[v].x, next->ceilHeight + topBias, vtx[v].z },
						  { vtx[v].x, next->ceilHeight + topBias, vtx[v].z } };
						TFE_RenderShared::lineDraw3d_addLine(width, line0, &color);
					}
					// Bottom
					if (next->floorHeight > sector->floorHeight)
					{
						f32 botBias = (s_camera.pos.y >= next->floorHeight) ? bias : -bias;

						Vec3f line0[] =
						{ { vtx[v].x, next->floorHeight + botBias, vtx[v].z },
						  { vtx[v].x, next->floorHeight + botBias, vtx[v].z } };
						TFE_RenderShared::lineDraw3d_addLine(width, line0, &color);
					}
				}
			}
		}

		if (next)
		{
			// Top
			if (next->ceilHeight < sector->ceilHeight && next->layer != s_curLayer)
			{
				f32 topBias = (s_camera.pos.y <= next->ceilHeight) ? -bias : bias;

				Vec3f line0[] =
				{ { v0.x, next->ceilHeight + topBias, v0.z },
				  { v1.x, next->ceilHeight + topBias, v1.z } };
				TFE_RenderShared::lineDraw3d_addLine(width, line0, &color);
			}
			// Bottom
			if (next->floorHeight > sector->floorHeight && next->layer != s_curLayer)
			{
				f32 botBias = (s_camera.pos.y >= next->floorHeight) ? bias : -bias;

				Vec3f line0[] =
				{ { v0.x, next->floorHeight + botBias, v0.z },
				  { v1.x, next->floorHeight + botBias, v1.z } };
				TFE_RenderShared::lineDraw3d_addLine(width, line0, &color);
			}
		}
	}

	void drawSelectedSector3D()
	{
		bool hoverAndSelect = s_selectedSector == s_hoveredSector;
		if (s_selectedSector)
		{
			drawFlat3D_Highlighted(s_selectedSector, HP_FLOOR, 3.5f, HL_SELECTED, false, true);
			drawFlat3D_Highlighted(s_selectedSector, HP_CEIL, 3.5f, HL_SELECTED, false, true);
			const size_t wallCount = s_selectedSector->walls.size();
			const EditorWall* wall = s_selectedSector->walls.data();
			for (size_t w = 0; w < wallCount; w++, wall++)
			{
				EditorSector* next = wall->adjoinId < 0 ? nullptr : &s_level.sectors[wall->adjoinId];
				drawWallLines3D_Highlighted(s_selectedSector, next, wall, 3.5f, HL_SELECTED, false);
			}
		}
		if (s_hoveredSector)
		{
			drawFlat3D_Highlighted(s_hoveredSector, HP_FLOOR, 3.5f, HL_HOVERED, hoverAndSelect, true);
			drawFlat3D_Highlighted(s_hoveredSector, HP_CEIL, 3.5f, HL_HOVERED, hoverAndSelect, true);
			const size_t wallCount = s_hoveredSector->walls.size();
			const EditorWall* wall = s_hoveredSector->walls.data();
			for (size_t w = 0; w < wallCount; w++, wall++)
			{
				EditorSector* next = wall->adjoinId < 0 ? nullptr : &s_level.sectors[wall->adjoinId];
				drawWallLines3D_Highlighted(s_hoveredSector, next, wall, 3.5f, HL_HOVERED, hoverAndSelect);
			}
		}
	}

	void drawSelectedSurface3D()
	{
		// In 3D, the floor and ceiling are surfaces too.
		bool hoverAndSelect = s_selectedSector == s_hoveredSector;
		if (s_selectedSector && (s_selectedWallPart == HP_FLOOR || s_selectedWallPart == HP_CEIL))
		{
			drawFlat3D_Highlighted(s_selectedSector, s_selectedWallPart, 3.5f, HL_SELECTED, false, false);
		}
		if (s_hoveredSector && (s_hoveredWallPart == HP_FLOOR || s_hoveredWallPart == HP_CEIL))
		{
			drawFlat3D_Highlighted(s_hoveredSector, s_hoveredWallPart, 3.5f, HL_HOVERED, hoverAndSelect, false);
		}

		hoverAndSelect = s_selectedWallId == s_hoveredWallId && s_selectedWallSector == s_hoveredWallSector;
		// Draw thicker lines.
		if (s_selectedWallId >= 0 && s_selectedWallSector)
		{
			EditorWall* wall = &s_selectedWallSector->walls[s_selectedWallId];
			EditorSector* next = wall->adjoinId < 0 ? nullptr : &s_level.sectors[wall->adjoinId];
			drawWallLines3D_Highlighted(s_selectedWallSector, next, wall, 3.5f, HL_SELECTED, false);
		}
		if (s_hoveredWallId >= 0 && s_hoveredWallSector)
		{
			EditorWall* wall = &s_hoveredWallSector->walls[s_hoveredWallId];
			EditorSector* next = wall->adjoinId < 0 ? nullptr : &s_level.sectors[wall->adjoinId];
			drawWallLines3D_Highlighted(s_hoveredWallSector, next, wall, 3.5f, HL_HOVERED, hoverAndSelect);
		}

		// Draw translucent surfaces.
		if (s_selectedWallId >= 0 && s_selectedWallSector)
		{
			u32 color = SCOLOR_POLY_SELECTED;
			drawWallColor3d_Highlighted(s_selectedWallSector, s_selectedWallSector->vtx.data(), s_selectedWallSector->walls.data() + s_selectedWallId, color, s_selectedWallPart);
		}
		if (s_hoveredWallId >= 0 && s_hoveredWallSector)
		{
			u32 color = SCOLOR_POLY_HOVERED;
			drawWallColor3d_Highlighted(s_hoveredWallSector, s_hoveredWallSector->vtx.data(), s_hoveredWallSector->walls.data() + s_hoveredWallId, color, s_hoveredWallPart);
		}
	}

	void drawVertex3d(const Vec3f* vtx, const EditorSector* sector, u32 color)
	{
		const f32 scale = TFE_Math::distance(vtx, &s_camera.pos) / f32(s_viewportSize.z);
		const f32 size = 8.0f * scale;
		drawBox(vtx, size, 3.0f, color);
	}

	// Render highlighted 3d elements.
	void renderHighlighted3d()
	{
		lineDraw3d_begin(s_viewportSize.x, s_viewportSize.z);
		triDraw3d_begin();

		if (s_editMode == LEDIT_SECTOR)
		{
			drawSelectedSector3D();
		}
		else if (s_editMode == LEDIT_WALL)
		{
			drawSelectedSurface3D();
		}
		else if (s_editMode == LEDIT_VERTEX)
		{
			bool alsoHovered = false;
			if (s_selectedVertices.size())
			{
				const size_t count = s_selectedVertices.size();
				const u64* list = s_selectedVertices.data();
				for (size_t i = 0; i < count; i++)
				{
					s32 featureIndex;
					bool overlapped;
					EditorSector* sector = unpackID(list[i], &featureIndex, &overlapped);
					if (overlapped || !sector) { continue; }

					bool thisAlsoHovered = s_hoveredVtxId == featureIndex && s_hoveredVtxSector == sector;
					if (thisAlsoHovered) { alsoHovered = true; }

					Vec3f pos = { sector->vtx[featureIndex].x, sector->floorHeight, sector->vtx[featureIndex].z };
					drawVertex3d(&pos, sector, thisAlsoHovered ? c_vertexClr[HL_SELECTED + 1] : c_vertexClr[HL_SELECTED]);
				}
			}
			if (s_hoveredVtxId >= 0 && !alsoHovered)
			{
				drawVertex3d(&s_hoveredVtxPos, s_hoveredVtxSector, c_vertexClr[HL_HOVERED]);
			}
		}

		triDraw3d_draw(&s_camera, s_gridSize, 0.0f);
		lineDraw3d_drawLines(&s_camera, false, false);
	}

	void renderLevel3D()
	{
		// Prepare for drawing.
		TFE_RenderShared::lineDraw3d_begin(s_viewportSize.x, s_viewportSize.z);
		TFE_RenderShared::triDraw3d_begin();

		if (s_camera.pos.y >= s_gridHeight)
		{
			grid3d_draw(s_gridSize, s_gridOpacity, s_gridHeight);
			renderCoordinateAxis();
		}
		else
		{
			renderCoordinateAxis();
			grid3d_draw(s_gridSize, s_gridOpacity, s_gridHeight);
		}

		const f32 width = 2.5f;
		const size_t count = s_level.sectors.size();
		const EditorSector* sector = s_level.sectors.data();
		for (size_t s = 0; s < count; s++, sector++)
		{
			// Skip other layers unless all layers is enabled.
			if (sector->layer != s_curLayer && !(s_editFlags & LEF_SHOW_ALL_LAYERS)) { continue; }

			Highlight highlight = HL_NONE;

			// Sector lighting.
			const u32 colorIndex = (s_editFlags & LEF_FULLBRIGHT) && s_sectorDrawMode != SDM_LIGHTING ? 31 : sector->ambient;

			// Floor/ceiling line bias.
			f32 bias = 1.0f / 1024.0f;
			f32 floorBias = (s_camera.pos.y >= sector->floorHeight) ?  bias : -bias;
			f32 ceilBias  = (s_camera.pos.y <= sector->ceilHeight)  ? -bias :  bias;

			// Draw lines.
			const size_t wallCount = sector->walls.size();
			const EditorWall* wall = sector->walls.data();
			const Vec2f* vtx = sector->vtx.data();
			for (size_t w = 0; w < wallCount; w++, wall++)
			{
				// Skip hovered or selected walls.
				bool skipLines = false;
				if ((s_hoveredWallSector == sector && s_hoveredWallId == w) ||
					(s_selectedWallSector == sector && s_selectedWallId == w))
				{
					skipLines = true;
				}

				EditorSector* next = wall->adjoinId < 0 ? nullptr : &s_level.sectors[wall->adjoinId];
				const Vec2f& v0 = sector->vtx[wall->idx[0]];
				const Vec2f& v1 = sector->vtx[wall->idx[1]];

				if (!skipLines)
				{
					drawWallLines3D(sector, next, wall, width, highlight, true);
				}

				s32 wallColorIndex = (s32)colorIndex;
				if (wallColorIndex < 31)
				{
					wallColorIndex = std::max(0, std::min(31, wallColorIndex + wall->wallLight));
				}

				u32 wallColor = 0xff1a0f0d;
				if (s_sectorDrawMode != SDM_WIREFRAME)
				{
					wallColor = c_sectorTexClr[wallColorIndex];
				}
								
				// Wall Parts
				const Vec2f wallOffset = { v1.x - v0.x, v1.z - v0.z };
				const f32 wallLengthTexels = sqrtf(wallOffset.x*wallOffset.x + wallOffset.z*wallOffset.z) * 8.0f;
				const f32 sectorHeight = sector->ceilHeight - sector->floorHeight;
				const bool flipHorz = (wall->flags[0] & WF1_FLIP_HORIZ) != 0u;
				Vec2f uvCorners[2];

				// Skip backfacing walls.
				const Vec2f cameraOffset = { v0.x - s_camera.pos.x, v0.z - s_camera.pos.z };
				const f32 facing = -wallOffset.z * cameraOffset.x + wallOffset.x * cameraOffset.z;
				if (facing < 0.0f) { continue; }

				if (wall->adjoinId < 0)
				{
					Vec3f corners[] = { {v0.x, sector->ceilHeight,  v0.z},
										{v1.x, sector->floorHeight, v1.z} };

					if (s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL)
					{
						const EditorTexture* tex = calculateTextureCoords(wall, &wall->tex[WP_MID], wallLengthTexels, sectorHeight, flipHorz, uvCorners);
						TFE_RenderShared::triDraw3d_addQuadTextured(TRIMODE_OPAQUE, corners, uvCorners, wallColor, tex ? tex->frames[0] : nullptr);
					}
					else
					{
						TFE_RenderShared::triDraw3d_addQuadColored(TRIMODE_OPAQUE, corners, wallColor);
					}

					// Sign?
					if (wall->tex[WP_SIGN].handle && (s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL))
					{
						const EditorTexture* tex = calculateSignTextureCoords(wall, &wall->tex[WP_MID], &wall->tex[WP_SIGN], wallLengthTexels, sectorHeight, false, uvCorners);
						if (tex)
						{
							TFE_RenderShared::triDraw3d_addQuadTextured(TRIMODE_CLAMP, corners, uvCorners, wallColor, tex->frames[0]);
						}
					}
				}
				else
				{
					EditorSector* next = &s_level.sectors[wall->adjoinId];
					bool botSign = false;
					// Bottom
					if (next->floorHeight > sector->floorHeight)
					{
						const f32 botHeight = next->floorHeight - sector->floorHeight;
						Vec3f corners[] = { {v0.x, next->floorHeight,   v0.z},
											{v1.x, sector->floorHeight, v1.z} };
						if (s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL)
						{
							const EditorTexture* tex = calculateTextureCoords(wall, &wall->tex[WP_BOT], wallLengthTexels, botHeight, flipHorz, uvCorners);
							TFE_RenderShared::triDraw3d_addQuadTextured(TRIMODE_OPAQUE, corners, uvCorners, wallColor, tex->frames[0]);
						}
						else
						{
							TFE_RenderShared::triDraw3d_addQuadColored(TRIMODE_OPAQUE, corners, wallColor);
						}

						// Sign?
						if (wall->tex[WP_SIGN].handle && (s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL))
						{
							const EditorTexture* tex = calculateSignTextureCoords(wall, &wall->tex[WP_BOT], &wall->tex[WP_SIGN], wallLengthTexels, botHeight, false, uvCorners);
							TFE_RenderShared::triDraw3d_addQuadTextured(TRIMODE_CLAMP, corners, uvCorners, wallColor, tex->frames[0]);
							botSign = true;
						}
					}
					// Top
					if (next->ceilHeight < sector->ceilHeight)
					{
						const f32 topHeight = sector->ceilHeight - next->ceilHeight;
						Vec3f corners[] = { {v0.x, sector->ceilHeight, v0.z},
										    {v1.x, next->ceilHeight,   v1.z} };

						if (s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL)
						{
							const EditorTexture* tex = calculateTextureCoords(wall, &wall->tex[WP_TOP], wallLengthTexels, topHeight, flipHorz, uvCorners);
							TFE_RenderShared::triDraw3d_addQuadTextured(TRIMODE_OPAQUE, corners, uvCorners, wallColor, tex ? tex->frames[0] : nullptr);
						}
						else
						{
							TFE_RenderShared::triDraw3d_addQuadColored(TRIMODE_OPAQUE, corners, wallColor);
						}

						// Sign?
						if (!botSign && wall->tex[WP_SIGN].handle && (s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL))
						{
							const EditorTexture* tex = calculateSignTextureCoords(wall, &wall->tex[WP_TOP], &wall->tex[WP_SIGN], wallLengthTexels, topHeight, false, uvCorners);
							TFE_RenderShared::triDraw3d_addQuadTextured(TRIMODE_CLAMP, corners, uvCorners, wallColor, tex->frames[0]);
						}
					}
					// Mid only for mask textures.
					if (next && (wall->flags[0] & WF1_ADJ_MID_TEX) && (s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL))
					{
						Vec3f corners[] = { {v0.x, min(next->ceilHeight, sector->ceilHeight), v0.z},
											{v1.x, max(next->floorHeight, sector->floorHeight), v1.z} };

						const EditorTexture* tex = calculateTextureCoords(wall, &wall->tex[WP_MID], wallLengthTexels, fabsf(corners[1].y - corners[0].y), flipHorz, uvCorners);
						TFE_RenderShared::triDraw3d_addQuadTextured(TRIMODE_BLEND, corners, uvCorners, wallColor, tex->frames[0]);
					}
				}
			}

			// Draw the floor and ceiling.
			u32 floorColor = 0xff402020;
			const u32 idxCount = (u32)sector->poly.triIdx.size();
			const u32 vtxCount = (u32)sector->poly.triVtx.size();
			const Vec2f* triVtx = sector->poly.triVtx.data();
			Vec3f vtxDataFlr[512];
			Vec3f vtxDataCeil[512];
			Vec2f uvFlr[512];
			Vec2f uvCeil[512];

			EditorTexture* floorTex = (EditorTexture*)getAssetData(sector->floorTex.handle);
			EditorTexture* ceilTex  = (EditorTexture*)getAssetData(sector->ceilTex.handle);
			const Vec2f& floorOffset = sector->floorTex.offset;
			const Vec2f& ceilOffset = sector->ceilTex.offset;

			for (u32 v = 0; v < vtxCount; v++)
			{
				vtxDataFlr[v] = { triVtx[v].x, sector->floorHeight, triVtx[v].z };
				vtxDataCeil[v] = { triVtx[v].x, sector->ceilHeight,  triVtx[v].z };
			}
						
			if (s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL)
			{
				for (u32 v = 0; v < vtxCount; v++)
				{
					uvFlr[v]  = { (triVtx[v].x - floorOffset.x) / 8.0f, (triVtx[v].z - floorOffset.z) / 8.0f };
					uvCeil[v] = { (triVtx[v].x - ceilOffset.x) / 8.0f, (triVtx[v].z - ceilOffset.z) / 8.0f };
				}
			}

			if (s_camera.pos.y > sector->floorHeight)
			{
				if (s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL)
				{
					triDraw3d_addTextured(TRIMODE_OPAQUE, idxCount, vtxCount, vtxDataFlr, uvFlr, sector->poly.triIdx.data(), c_sectorTexClr[colorIndex], false, floorTex->frames[0]);
				}
				else if (s_sectorDrawMode == SDM_LIGHTING)
				{
					triDraw3d_addColored(TRIMODE_OPAQUE, idxCount, vtxCount, vtxDataFlr, sector->poly.triIdx.data(), c_sectorTexClr[colorIndex], false);
				}
				else
				{
					triDraw3d_addColored(TRIMODE_OPAQUE, idxCount, vtxCount, vtxDataFlr, sector->poly.triIdx.data(), floorColor, false);
				}
			}
			if (s_camera.pos.y < sector->ceilHeight)
			{
				if (s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL)
				{
					triDraw3d_addTextured(TRIMODE_OPAQUE, idxCount, vtxCount, vtxDataCeil, uvCeil, sector->poly.triIdx.data(), c_sectorTexClr[colorIndex], true, ceilTex->frames[0]);
				}
				else if (s_sectorDrawMode == SDM_LIGHTING)
				{
					triDraw3d_addColored(TRIMODE_OPAQUE, idxCount, vtxCount, vtxDataCeil, sector->poly.triIdx.data(), c_sectorTexClr[colorIndex], true);
				}
				else
				{
					triDraw3d_addColored(TRIMODE_OPAQUE, idxCount, vtxCount, vtxDataCeil, sector->poly.triIdx.data(), floorColor, true);
				}
			}
		}

		// Draw the 3D cursor.
		{
			const f32 distFromCam = TFE_Math::distance(&s_cursor3d, &s_camera.pos);
			const f32 size = distFromCam * 16.0f / f32(s_viewportSize.z);
			const f32 sizeExp = distFromCam * 18.0f / f32(s_viewportSize.z);
			drawBox(&s_cursor3d, size, 3.0f, 0x80ff8020);
		}

		TFE_RenderShared::triDraw3d_draw(&s_camera, s_gridSize, s_gridOpacity);
		TFE_RenderShared::lineDraw3d_drawLines(&s_camera, true, false);

		renderHighlighted3d();
						
		/*
		if (s_camera.pos.y >= s_gridHeight)
		{
			grid3d_draw(s_gridSize, s_gridOpacity, s_gridHeight);
			renderCoordinateAxis();
		}
		else
		{
			renderCoordinateAxis();
			grid3d_draw(s_gridSize, s_gridOpacity, s_gridHeight);
		}
		*/
	}

	void renderSectorPolygon2d(const Polygon* poly, u32 color)
	{
		const size_t idxCount = poly->triIdx.size();
		if (idxCount)
		{
			const s32*    idxData = poly->triIdx.data();
			const Vec2f*  vtxData = poly->triVtx.data();
			const size_t vtxCount = poly->triVtx.size();

			s_transformedVtx.resize(vtxCount);
			Vec2f* transVtx = s_transformedVtx.data();
			for (size_t v = 0; v < vtxCount; v++, vtxData++)
			{
				transVtx[v] = { vtxData->x * s_viewportTrans2d.x + s_viewportTrans2d.y, vtxData->z * s_viewportTrans2d.z + s_viewportTrans2d.w };
			}
			triDraw2d_addColored((u32)idxCount, (u32)vtxCount, transVtx, idxData, color);
		}
	}

	void renderTexturedSectorPolygon2d(const Polygon* poly, u32 color, EditorTexture* tex, const Vec2f& offset)
	{
		const size_t idxCount = poly->triIdx.size();
		if (idxCount)
		{
			const s32*    idxData = poly->triIdx.data();
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
			triDraw2D_addTextured((u32)idxCount, (u32)vtxCount, transVtx, uv, idxData, color, tex->frames[0]);
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

	void drawSolidBox(const Vec3f* center, f32 side, u32 color)
	{
		const f32 w = side * 0.5f;
		const Vec3f vtx[8] =
		{
			{center->x - w, center->y - w, center->z - w},
			{center->x + w, center->y - w, center->z - w},
			{center->x + w, center->y - w, center->z + w},
			{center->x - w, center->y - w, center->z + w},

			{center->x - w, center->y + w, center->z - w},
			{center->x + w, center->y + w, center->z - w},
			{center->x + w, center->y + w, center->z + w},
			{center->x - w, center->y + w, center->z + w},
		};
		const s32 idx[36] =
		{
			0, 1, 2, 0, 2, 3,	// bot
			4, 6, 5, 4, 7, 6,	// top
			0, 5, 1, 0, 4, 5,
			1, 6, 2, 1, 5, 6,
			2, 7, 3, 2, 6, 7,
			3, 4, 0, 3, 7, 4
		};
		triDraw3d_addColored(TRIMODE_OPAQUE, 36, 8, vtx, idx, color, false);
	}

	void drawBox(const Vec3f* center, f32 side, f32 lineWidth, u32 color)
	{
		const f32 w = side * 0.5f;
		const Vec3f lines[] =
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

		lineDraw3d_addLines(12, lineWidth, lines, colors);
	}
}
