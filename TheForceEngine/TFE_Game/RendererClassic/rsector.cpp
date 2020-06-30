#include "rwall.h"
#include "rflat.h"
#include "rlighting.h"
#include "rsector.h"
#include "redgePair.h"
#include "rtexture.h"
#include "fixedPoint.h"
#include "rmath.h"
#include "rcommon.h"
#include <TFE_Renderer/renderer.h>
#include <TFE_System/system.h>
#include <TFE_System/math.h>
#include <TFE_System/profiler.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Asset/levelAsset.h>
#include <TFE_Asset/levelObjectsAsset.h>
#include <TFE_Asset/colormapAsset.h>
#include <TFE_Game/level.h>
#include <TFE_Asset/spriteAsset.h>
#include <TFE_Asset/textureAsset.h>
#include <TFE_Asset/paletteAsset.h>
#include <TFE_Asset/colormapAsset.h>
#include <TFE_Asset/modelAsset.h>
#include <TFE_LogicSystem/logicSystem.h>
#include <TFE_FrontEndUI/console.h>
#include <algorithm>
#include <assert.h>

using namespace RendererClassic;
using namespace RClassicWall;
using namespace RClassicFlat;
using namespace RClassicEdgePair;
using namespace RClassicLighting;
using namespace RClassicTexture;
using namespace FixedPoint;
using namespace RMath;

namespace RClassicSector
{
	// Values saved at a given level of the stack while traversing sectors.
	// This is 80 bytes in the original DOS code but will be more in The Force Engine
	// due to 64 bit pointers.
	struct SectorSaveValues
	{
		RSector* curSector;
		RSector* prevSector;
		fixed16* depth1d;
		u32     windowX0;
		u32     windowX1;
		s32     windowMinY;
		s32     windowMaxY;
		s32     windowMaxCeil;
		s32     windowMinFloor;
		s32     wallMaxCeilY;
		s32     wallMinFloorY;
		u32     windowMinX;
		u32     windowMaxX;
		s32*    windowTop;
		s32*    windowBot;
		s32*    windowTop2;
		s32*    windowBot2;
		s32     sectorAmbient;
		s32     scaledAmbient;
		s32     scaledAmbient2k;
	};
	static SectorSaveValues s_sectorStack[MAX_ADJOIN_DEPTH];

	static RSector* s_curSector;
	static RSector* s_rsectors;
	static MemoryPool* s_memPool;
	static u32 s_sectorCount;

	void adjoin_setupAdjoinWindow(s32* winBot, s32* winBotNext, s32* winTop, s32* winTopNext, EdgePair* adjoinEdges, s32 adjoinCount);
	void adjoin_computeWindowBounds(EdgePair* adjoinEdges);
	void sector_saveValues(s32 index);
	void sector_restoreValues(s32 index);
	
	void sector_setMemoryPool(MemoryPool* memPool)
	{
		s_memPool = memPool;
	}

	void sector_allocate(u32 count)
	{
		s_sectorCount = count;
		s_rsectors = (RSector*)s_memPool->allocate(sizeof(RSector) * count);
	}

	RSector* sector_get()
	{
		return s_rsectors;
	}

	s32 wallSortX(const void* r0, const void* r1)
	{
		return ((const RWallSegment*)r0)->wallX0 - ((const RWallSegment*)r1)->wallX0;
	}

	void sector_draw(RSector* sector)
	{
		s_curSector = sector;
		s_sectorIndex++;
		s_adjoinIndex++;
		if (s_adjoinIndex > s_maxAdjoinIndex)
		{
			s_maxAdjoinIndex = s_adjoinIndex;
		}

		s32* winTop = &s_windowTop_all[(s_adjoinDepth - 1) * s_width];
		s32* winBot = &s_windowBot_all[(s_adjoinDepth - 1) * s_width];
		s32* winTopNext = &s_windowTop_all[s_adjoinDepth * s_width];
		s32* winBotNext = &s_windowBot_all[s_adjoinDepth * s_width];

		s_depth1d = &s_depth1d_all[(s_adjoinDepth - 1) * s_width];

		s32 startWall = s_curSector->startWall;
		s32 drawWallCount = s_curSector->drawWallCnt;

		s_sectorAmbient = round16(s_curSector->ambientFixed);
		s_scaledAmbient = (s_sectorAmbient >> 1) + (s_sectorAmbient >> 2) + (s_sectorAmbient >> 3);

		s_windowTop = winTop;
		s_windowBot = winBot;
		fixed16* depthPrev = nullptr;
		if (s_adjoinDepth > 1)
		{
			depthPrev = &s_depth1d_all[(s_adjoinDepth - 2) * s_width];
			memcpy(&s_depth1d[s_minScreenX], &depthPrev[s_minScreenX], s_width * 4);
		}

		s_wallMaxCeilY = s_windowMinY;
		s_wallMinFloorY = s_windowMaxY;

		if (s_drawFrame != s_curSector->prevDrawFrame)
		{
			TFE_ZONE_BEGIN(secXform, "Sector Vertex Transform");
				vec2* vtxWS = s_curSector->verticesWS;
				vec2* vtxVS = s_curSector->verticesVS;
				for (s32 v = 0; v < s_curSector->vertexCount; v++)
				{
					vtxVS->x = mul16(vtxWS->x, s_cosYaw) + mul16(vtxWS->z, s_sinYaw) + s_xCameraTrans;
					vtxVS->z = mul16(vtxWS->x, s_negSinYaw) + mul16(vtxWS->z, s_cosYaw) + s_zCameraTrans;
					vtxVS++;
					vtxWS++;
				}
			TFE_ZONE_END(secXform);

			TFE_ZONE_BEGIN(wallProcess, "Sector Wall Process");
				startWall = s_nextWall;
				RWall* wall = s_curSector->walls;
				for (s32 i = 0; i < s_curSector->wallCount; i++, wall++)
				{
					wall_process(wall);
				}
				drawWallCount = s_nextWall - startWall;

				s_curSector->startWall = startWall;
				s_curSector->drawWallCnt = drawWallCount;
				s_curSector->prevDrawFrame = s_drawFrame;

				// Setup wall flags not from the original code, still to be replaced.
				sector_setupWallDrawFlags(s_curSector);
			TFE_ZONE_END(wallProcess);
		}

		RWallSegment* wallSegment = &s_wallSegListDst[s_curWallSeg];
		s32 drawSegCnt = wall_mergeSort(wallSegment, MAX_SEG - s_curWallSeg, startWall, drawWallCount);
		s_curWallSeg += drawSegCnt;

		TFE_ZONE_BEGIN(wallQSort, "Wall QSort");
			qsort(wallSegment, drawSegCnt, sizeof(RWallSegment), wallSortX);
		TFE_ZONE_END(wallQSort);

		s32 flatCount = s_flatCount;
		EdgePair* flatEdge = &s_flatEdgeList[s_flatCount];
		s_flatEdge = flatEdge;

		s32 adjoinStart = s_adjoinSegCount;
		EdgePair* adjoinEdges = &s_adjoinEdgeList[adjoinStart];
		RWallSegment* adjoinList[MAX_ADJOIN_DEPTH];

		s_adjoinEdge = adjoinEdges;
		s_adjoinSegment = adjoinList;

		// Draw each wall segment in the sector.
		TFE_ZONE_BEGIN(secDrawWalls, "Draw Walls");
		for (s32 i = 0; i < drawSegCnt; i++, wallSegment++)
		{
			RWall* srcWall = wallSegment->srcWall;
			RSector* nextSector = srcWall->nextSector;

			// This will always be true for now.
			if (!nextSector)
			{
				wall_drawSolid(wallSegment);
			}
			else
			{
				const s32 df = srcWall->drawFlags;
				assert(df >= 0);
				if (df <= WDF_MIDDLE)
				{
					if (df == WDF_MIDDLE || (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
					{
						wall_drawMask(wallSegment);
					}
					else
					{
						wall_drawBottom(wallSegment);
					}
				}
				else if (df == WDF_TOP)
				{
					if (nextSector->flags1 & SEC_FLAGS1_EXT_ADJ)
					{
						wall_drawMask(wallSegment);
					}
					else
					{
						wall_drawTop(wallSegment);
					}
				}
				else if (df == WDF_TOP_AND_BOT)
				{
					if ((nextSector->flags1 & SEC_FLAGS1_EXT_ADJ) && (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
					{
						wall_drawMask(wallSegment);
					}
					else if (nextSector->flags1 & SEC_FLAGS1_EXT_ADJ)
					{
						wall_drawBottom(wallSegment);
					}
					else if (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ)
					{
						wall_drawTop(wallSegment);
					}
					else
					{
						wall_drawTopAndBottom(wallSegment);
					}
				}
				else // WDF_BOT
				{
					if (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ)
					{
						wall_drawMask(wallSegment);
					}
					else
					{
						wall_drawBottom(wallSegment);
					}
				}
			}
		}
		TFE_ZONE_END(secDrawWalls);

		TFE_ZONE_BEGIN(secDrawFlats, "Draw Flats");
			// Draw flats
			// Note: in the DOS code flat drawing functions are called through function pointers.
			// Since the function pointers always seem to be the same, the functions are called directly in this code.
			// Most likely this was used for testing or debug drawing and may be added back in the future.
			const s32 newFlatCount = s_flatCount - flatCount;
			if (s_curSector->flags1 & SEC_FLAGS1_EXTERIOR)
			{
				if (s_curSector->flags1 & SEC_FLAGS1_NOWALL_DRAW)
				{
					wall_drawSkyTopNoWall(s_curSector);
				}
				else
				{
					wall_drawSkyTop(s_curSector);
				}
			}
			else
			{
				flat_drawCeiling(s_curSector, flatEdge, newFlatCount);
			}
			if (s_curSector->flags1 & SEC_FLAGS1_PIT)
			{
				if (s_curSector->flags1 & SEC_FLAGS1_NOWALL_DRAW)
				{
					wall_drawSkyBottomNoWall(s_curSector);
				}
				else
				{
					wall_drawSkyBottom(s_curSector);
				}
			}
			else
			{
				flat_drawFloor(s_curSector, flatEdge, newFlatCount);
			}
		TFE_ZONE_END(secDrawFlats);

		// Adjoins
		s32 adjoinCount = s_adjoinSegCount - adjoinStart;
		if (adjoinCount && s_adjoinDepth < MAX_ADJOIN_DEPTH)
		{
			adjoin_setupAdjoinWindow(winBot, winBotNext, winTop, winTopNext, adjoinEdges, adjoinCount);
			RWallSegment** seg = adjoinList;
			RWallSegment* prevAdjoinSeg = nullptr;
			RWallSegment* curAdjoinSeg  = nullptr;

			s32 adjoinEnd = adjoinCount - 1;
			for (s32 i = 0; i < adjoinCount; i++, seg++, adjoinEdges++)
			{
				prevAdjoinSeg = curAdjoinSeg;
				curAdjoinSeg = *seg;

				RWall* srcWall = curAdjoinSeg->srcWall;
				RWallSegment* nextAdjoin = (i < adjoinEnd) ? *(seg + 1) : nullptr;
				RSector* nextSector = srcWall->nextSector;
				if (s_adjoinDepth < MAX_ADJOIN_DEPTH && s_adjoinDepth < s_maxDepthCount)
				{
					s32 index = s_adjoinDepth - 1;
					sector_saveValues(index);

					adjoin_computeWindowBounds(adjoinEdges);
					s_adjoinDepth++;
					if (s_adjoinDepth > s_maxAdjoinDepth)
					{
						s_maxAdjoinDepth = s_adjoinDepth;
					}

					srcWall->drawFrame = s_drawFrame;
					s_windowTop = winTopNext;
					s_windowBot = winBotNext;
					if (prevAdjoinSeg != 0)
					{
						if (prevAdjoinSeg->wallX1 + 1 == curAdjoinSeg->wallX0)
						{
							s_windowX0 = s_windowMinX;
						}
					}
					if (nextAdjoin)
					{
						if (curAdjoinSeg->wallX1 == nextAdjoin->wallX0 - 1)
						{
							s_windowX1 = s_windowMaxX;
						}
					}

					s_minSegZ = min(curAdjoinSeg->z0, curAdjoinSeg->z1);
					sector_draw(nextSector);

					if (s_adjoinDepth)
					{
						s32 index = s_adjoinDepth - 2;
						s_adjoinDepth--;
						sector_restoreValues(index);
					}
					srcWall->drawFrame = 0;
					if (srcWall->flags1 & WF1_ADJ_MID_TEX)
					{
						TFE_ZONE("Draw Transparent Walls");
						wall_drawTransparent(curAdjoinSeg, adjoinEdges);
					}
				}
			}
		}

		if (!(s_curSector->flags1 & SEC_FLAGS1_SUBSECTOR) && depthPrev && s_drawFrame != s_prevSector->prevDrawFrame2)
		{
			memcpy(&depthPrev[s_windowMinX], &s_depth1d[s_windowMinX], (s_windowMaxX - s_windowMinX + 1) * sizeof(fixed16));
		}

		s_curSector->flags1 |= SEC_FLAGS1_RENDERED;
		s_curSector->prevDrawFrame2 = s_drawFrame;
	}

	// Setup wall flags not from the original code, still to be replaced.
	void sector_setupWallDrawFlags(RSector* sector)
	{
		RWall* wall = sector->walls;
		const fixed16 midHeight = 8 * (sector->floorHeight - sector->ceilingHeight);
		for (s32 w = 0; w < sector->wallCount; w++, wall++)
		{
			const RSector* next = wall->nextSector;
			wall->drawFlags = WDF_MIDDLE;
			if (!next)
			{
				wall->midTexelHeight = midHeight;
				continue;
			}

			if (next->floorHeight < sector->floorHeight && next->ceilingHeight > sector->ceilingHeight)
			{
				wall->drawFlags = WDF_TOP_AND_BOT;
				wall->botTexelHeight = mul16(intToFixed16(8), sector->floorHeight - next->floorHeight);
				wall->topTexelHeight = mul16(intToFixed16(8), next->ceilingHeight - sector->ceilingHeight);
				wall->midTexelHeight = mul16(intToFixed16(8), next->floorHeight - next->ceilingHeight);
			}
			else if (next->floorHeight < sector->floorHeight)
			{
				wall->drawFlags = WDF_BOT;
				wall->botTexelHeight = mul16(intToFixed16(8), sector->floorHeight - next->floorHeight);
				wall->midTexelHeight = mul16(intToFixed16(8), next->floorHeight - sector->ceilingHeight);
			}
			else if (next->ceilingHeight > sector->ceilingHeight)
			{
				wall->drawFlags = WDF_TOP;
				wall->topTexelHeight = mul16(intToFixed16(8), next->ceilingHeight - sector->ceilingHeight);
				wall->midTexelHeight = mul16(intToFixed16(8), sector->floorHeight - next->ceilingHeight);
			}
		}
	}

	void sector_copy(RSector* out, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures)
	{
		out->vertexCount = sector->vtxCount;
		out->wallCount = sector->wallCount;

		// Initial setup.
		if (!out->verticesWS)
		{
			out->verticesWS = (vec2*)s_memPool->allocate(sizeof(vec2) * out->vertexCount);
			out->verticesVS = (vec2*)s_memPool->allocate(sizeof(vec2) * out->vertexCount);
			out->walls = (RWall*)s_memPool->allocate(sizeof(RWall) * out->wallCount);

			out->startWall = 0;
			out->drawWallCnt = 0;

			RWall* wall = out->walls;
			for (s32 w = 0; w < out->wallCount; w++, wall++)
			{
				wall->sector = out;
				wall->drawFrame = 0;
				wall->drawFlags = WDF_MIDDLE;
				wall->topTexelHeight = 0;
				wall->botTexelHeight = 0;

				wall->w0 = &out->verticesWS[walls[w].i0];
				wall->w1 = &out->verticesWS[walls[w].i1];
				wall->v0 = &out->verticesVS[walls[w].i0];
				wall->v1 = &out->verticesVS[walls[w].i1];
			}

			out->prevDrawFrame = 0;
			out->prevDrawFrame2 = 0;
		}

		sector_update(sector->id);
	}

	void sector_updateAll()
	{
		for (u32 s = 0; s < s_sectorCount; s++)
		{
			sector_update(s);
		}
	}
			
	// In the future, renderer sectors can be changed directly by INF, but for now just copy from the level data.
	// TODO: Currently all sector data is updated - get the "dirty" flag to work reliably so only partial data needs to be updated (textures).
	// TODO: Properly handle switch textures (after reverse-engineering of switch rendering is done).
	void sector_update(u32 sectorId, u32 updateFlags)
	{
		TFE_ZONE("Sector Update");
		if (updateFlags == 0) { return; }

		LevelData* level = TFE_LevelAsset::getLevelData();
		Texture** textures = level->textures.data();

		Sector* sector = &level->sectors[sectorId];
		SectorWall* walls = level->walls.data() + sector->wallOffset;
		Vec2f* vertices = level->vertices.data() + sector->vtxOffset;

		RSector* out = &s_rsectors[sectorId];
		out->vertexCount = sector->vtxCount;
		out->wallCount = sector->wallCount;

		const SectorBaseHeight* baseHeight = TFE_Level::getBaseSectorHeight(sectorId);
		fixed16 ceilDelta  = floatToFixed16(8.0f * (sector->ceilAlt - baseHeight->ceilAlt));
		fixed16 floorDelta = floatToFixed16(8.0f * (sector->floorAlt - baseHeight->floorAlt));

		out->ambientFixed  = intToFixed16(sector->ambient);
		out->floorHeight   = floatToFixed16(sector->floorAlt);
		out->ceilingHeight = floatToFixed16(sector->ceilAlt);
		out->secHeight     = floatToFixed16(sector->secAlt);
		out->flags1        = sector->flags[0];
		out->flags2        = sector->flags[1];
		out->flags3        = sector->flags[2];
		out->floorTex      = texture_getFrame(textures[sector->floorTexture.texId]);
		out->ceilTex       = texture_getFrame(textures[sector->ceilTexture.texId]);
		out->floorOffsetX  = floatToFixed16(sector->floorTexture.offsetX);
		out->floorOffsetZ  = floatToFixed16(sector->floorTexture.offsetY);
		out->ceilOffsetX   = floatToFixed16(sector->ceilTexture.offsetX);
		out->ceilOffsetZ   = floatToFixed16(sector->ceilTexture.offsetY);

		TFE_ZONE_BEGIN(secVtx, "Sector Update Vertices");
		if (updateFlags & SEC_UPDATE_GEO)
		{
			for (s32 v = 0; v < out->vertexCount; v++)
			{
				out->verticesWS[v].x = floatToFixed16(vertices[v].x);
				out->verticesWS[v].z = floatToFixed16(vertices[v].z);
			}
		}
		TFE_ZONE_END(secVtx);
		
		TFE_ZONE_BEGIN(secWall, "Sector Update Walls");
		RWall* wall = out->walls;
		const fixed16 midTexelHeight = mul16(intToFixed16(8), floatToFixed16(sector->floorAlt - sector->ceilAlt));
		for (s32 w = 0; w < out->wallCount; w++, wall++)
		{
			wall->nextSector = (walls[w].adjoin >= 0) ? &s_rsectors[walls[w].adjoin] : nullptr;

			wall->topTex  = texture_getFrame(walls[w].top.texId  >= 0 ? textures[walls[w].top.texId]  : nullptr);
			wall->midTex  = texture_getFrame(walls[w].mid.texId  >= 0 ? textures[walls[w].mid.texId]  : nullptr);
			wall->botTex  = texture_getFrame(walls[w].bot.texId  >= 0 ? textures[walls[w].bot.texId]  : nullptr);
			wall->signTex = texture_getFrame(walls[w].sign.texId >= 0 ? textures[walls[w].sign.texId] : nullptr, walls[w].sign.frame);

			if (updateFlags & SEC_UPDATE_GEO)
			{
				const Vec2f offset = { vertices[walls[w].i1].x - vertices[walls[w].i0].x, vertices[walls[w].i1].z - vertices[walls[w].i0].z };
				wall->texelLength = floatToFixed16(8.0f * sqrtf(offset.x*offset.x + offset.z*offset.z));
			}

			// Prime with mid texture height, other heights will be computed as needed.
			wall->midTexelHeight = midTexelHeight;

			// Texture Offsets
			wall->topUOffset = floatToFixed16(8.0f * walls[w].top.offsetX);
			wall->topVOffset = floatToFixed16(8.0f * walls[w].top.offsetY);
			wall->midUOffset = floatToFixed16(8.0f * walls[w].mid.offsetX);
			wall->midVOffset = floatToFixed16(8.0f * walls[w].mid.offsetY);
			wall->botUOffset = floatToFixed16(8.0f * walls[w].bot.offsetX);
			wall->botVOffset = floatToFixed16(8.0f * walls[w].bot.offsetY);
			wall->signUOffset = floatToFixed16(8.0f * walls[w].sign.offsetX);
			wall->signVOffset = floatToFixed16(8.0f * walls[w].sign.offsetY);

			if (walls[w].flags[0] & WF1_TEX_ANCHORED)
			{
				wall->midVOffset -= floorDelta;
				wall->botVOffset -= floorDelta;
				wall->signVOffset -= floorDelta;

				const s32 nextId = walls[w].adjoin;
				const SectorBaseHeight* baseHeightNext = (nextId >= 0) ? TFE_Level::getBaseSectorHeight(nextId) : nullptr;
				const Sector* nextSrc = (nextId >= 0) ? &level->sectors[nextId] : nullptr;
				if (nextSrc)
				{
					// Handle next sector moving.
					wall->botVOffset  -= floatToFixed16(8.0f * (baseHeightNext->floorAlt - nextSrc->floorAlt));
					wall->signVOffset -= floatToFixed16(8.0f * (baseHeightNext->floorAlt - nextSrc->floorAlt));
					wall->topVOffset   = -wall->topVOffset + (wall->topTex ? wall->topTex->height : 0);
				}
			}

			wall->flags1 = walls[w].flags[0];
			wall->flags2 = walls[w].flags[1];
			wall->flags3 = walls[w].flags[2];

			wall->wallLight = walls[w].light;
		}
		TFE_ZONE_END(secWall);
	}
		
	void adjoin_setupAdjoinWindow(s32* winBot, s32* winBotNext, s32* winTop, s32* winTopNext, EdgePair* adjoinEdges, s32 adjoinCount)
	{
		TFE_ZONE("Setup Adjoin Window");

		// Note: This is pretty inefficient, especially at higher resolutions.
		// The column loops below can be adjusted to do the copy only in the required ranges.
		memcpy(&winTopNext[s_minScreenX], &winTop[s_minScreenX], s_width * 4);
		memcpy(&winBotNext[s_minScreenX], &winBot[s_minScreenX], s_width * 4);

		// Loop through each adjoin and setup the column range based on the edge pair and the parent
		// column range.
		for (s32 i = 0; i < adjoinCount; i++, adjoinEdges++)
		{
			const s32 x0 = adjoinEdges->x0;
			const s32 x1 = adjoinEdges->x1;

			const fixed16 ceil_dYdX = adjoinEdges->dyCeil_dx;
			fixed16 y = adjoinEdges->yCeil0;
			for (s32 x = x0; x <= x1; x++, y += ceil_dYdX)
			{
				s32 yPixel = round16(y);
				s32 yBot = winBotNext[x];
				s32 yTop = winTop[x];
				if (yPixel > yTop)
				{
					winTopNext[x] = (yPixel <= yBot) ? yPixel : yBot + 1;
				}
			}
			const fixed16 floor_dYdX = adjoinEdges->dyFloor_dx;
			y = adjoinEdges->yFloor0;
			for (s32 x = x0; x <= x1; x++, y += floor_dYdX)
			{
				s32 yPixel = round16(y);
				s32 yTop = winTop[x];
				s32 yBot = winBot[x];
				if (yPixel < yBot)
				{
					winBotNext[x] = (yPixel >= yTop) ? yPixel : yTop - 1;
				}
			}
		}
	}

	void adjoin_computeWindowBounds(EdgePair* adjoinEdges)
	{
		s32 yC = adjoinEdges->yPixel_C0;
		if (yC > s_windowMinY)
		{
			s_windowMinY = yC;
		}
		s32 yF = adjoinEdges->yPixel_F0;
		if (yF < s_windowMaxY)
		{
			s_windowMaxY = yF;
		}
		yC = adjoinEdges->yPixel_C1;
		if (yC > s_windowMaxCeil)
		{
			s_windowMaxCeil = yC;
		}
		yF = adjoinEdges->yPixel_F1;
		if (yF < s_windowMinFloor)
		{
			s_windowMinFloor = yF;
		}
		s_wallMaxCeilY = s_windowMinY - 1;
		s_wallMinFloorY = s_windowMaxY + 1;
		s_windowMinX = adjoinEdges->x0;
		s_windowMaxX = adjoinEdges->x1;
		//s_windowTop2 = s_windowTop;
		//s_windowBot2 = s_windowBot;
		s_prevSector = s_curSector;
	}
			
	void sector_saveValues(s32 index)
	{
		SectorSaveValues* dst = &s_sectorStack[index];
		dst->curSector = s_curSector;
		dst->prevSector = s_prevSector;
		dst->depth1d = s_depth1d;
		dst->windowX0 = s_windowX0;
		dst->windowX1 = s_windowX1;
		dst->windowMinY = s_windowMinY;
		dst->windowMaxY = s_windowMaxY;
		dst->windowMaxCeil = s_windowMaxCeil;
		dst->windowMinFloor = s_windowMinFloor;
		dst->wallMaxCeilY = s_wallMaxCeilY;
		dst->wallMinFloorY = s_wallMinFloorY;
		dst->windowMinX = s_windowMinX;
		dst->windowMaxX = s_windowMaxX;
		dst->windowTop = s_windowTop;
		dst->windowBot = s_windowBot;
		//dst->windowTop2 = s_windowTop2;
		//dst->windowBot2 = s_windowBot2;
		dst->sectorAmbient = s_sectorAmbient;
		dst->scaledAmbient = s_scaledAmbient;
		//dst->scaledAmbient2k = s_scaledAmbient2k;
	}

	void sector_restoreValues(s32 index)
	{
		const SectorSaveValues* src = &s_sectorStack[index];
		s_curSector = src->curSector;
		s_prevSector = src->prevSector;
		s_depth1d = src->depth1d;
		s_windowX0 = src->windowX0;
		s_windowX1 = src->windowX1;
		s_windowMinY = src->windowMinY;
		s_windowMaxY = src->windowMaxY;
		s_windowMaxCeil = src->windowMaxCeil;
		s_windowMinFloor = src->windowMinFloor;
		s_wallMaxCeilY = src->wallMaxCeilY;
		s_wallMinFloorY = src->wallMinFloorY;
		s_windowMinX = src->windowMinX;
		s_windowMaxX = src->windowMaxX;
		s_windowTop = src->windowTop;
		s_windowBot = src->windowBot;
		//s_windowTop2 = src->windowTop2;
		//s_windowBot2 = src->windowBot2;
		s_sectorAmbient = src->sectorAmbient;
		s_scaledAmbient = src->scaledAmbient;
		//s_scaledAmbient2k = src->scaledAmbient2k;
	}
}