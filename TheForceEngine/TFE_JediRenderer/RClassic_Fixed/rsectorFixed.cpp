#include <TFE_System/profiler.h>
// TODO: Either move level.h or fix it.
#include <TFE_Game/level.h>

#include "rsectorFixed.h"
#include "rwallFixed.h"
#include "rflatFixed.h"
#include "rlightingFixed.h"
#include "redgePairFixed.h"
#include "rcommonFixed.h"
#include "../fixedPoint.h"
#include "../rmath.h"
#include "../rcommon.h"
#include "../rtexture.h"

using namespace TFE_JediRenderer::RClassic_Fixed;

namespace TFE_JediRenderer
{
	void TFE_Sectors_Fixed::draw(RSector* sector)
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

		s_depth1d_Fixed = &s_depth1d_all_Fixed[(s_adjoinDepth - 1) * s_width];

		s32 startWall = s_curSector->startWall;
		s32 drawWallCount = s_curSector->drawWallCnt;

		s_sectorAmbient = round16(s_curSector->ambient.f16_16);
		s_scaledAmbient = (s_sectorAmbient >> 1) + (s_sectorAmbient >> 2) + (s_sectorAmbient >> 3);

		s_windowTop = winTop;
		s_windowBot = winBot;
		fixed16_16* depthPrev = nullptr;
		if (s_adjoinDepth > 1)
		{
			depthPrev = &s_depth1d_all_Fixed[(s_adjoinDepth - 2) * s_width];
			memcpy(&s_depth1d_Fixed[s_minScreenX], &depthPrev[s_minScreenX], s_width * 4);
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
					vtxVS->x.f16_16 = mul16(vtxWS->x.f16_16, s_cosYaw_Fixed) + mul16(vtxWS->z.f16_16, s_sinYaw_Fixed) + s_xCameraTrans_Fixed;
					vtxVS->z.f16_16 = mul16(vtxWS->x.f16_16, s_negSinYaw_Fixed) + mul16(vtxWS->z.f16_16, s_cosYaw_Fixed) + s_zCameraTrans_Fixed;
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
				setupWallDrawFlags(s_curSector);
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
					saveValues(index);

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

					s_windowMinZ_Fixed = min(curAdjoinSeg->z0.f16_16, curAdjoinSeg->z1.f16_16);
					draw(nextSector);
					
					if (s_adjoinDepth)
					{
						s32 index = s_adjoinDepth - 2;
						s_adjoinDepth--;
						restoreValues(index);
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
			memcpy(&depthPrev[s_windowMinX], &s_depth1d_Fixed[s_windowMinX], (s_windowMaxX - s_windowMinX + 1) * sizeof(fixed16_16));
		}

		s_curSector->flags1 |= SEC_FLAGS1_RENDERED;
		s_curSector->prevDrawFrame2 = s_drawFrame;
	}
		
	void TFE_Sectors_Fixed::setupWallDrawFlags(RSector* sector)
	{
		RWall* wall = sector->walls;
		for (s32 w = 0; w < sector->wallCount; w++, wall++)
		{
			if (wall->nextSector)
			{
				RSector* wSector = wall->sector;
				fixed16_16 wFloorHeight = wSector->floorHeight.f16_16;
				fixed16_16 wCeilHeight = wSector->ceilingHeight.f16_16;

				RWall* mirror = wall->mirrorWall;
				RSector* mSector = mirror->sector;
				fixed16_16 mFloorHeight = mSector->floorHeight.f16_16;
				fixed16_16 mCeilHeight = mSector->ceilingHeight.f16_16;

				wall->drawFlags = 0;
				mirror->drawFlags = 0;

				if (wCeilHeight < mCeilHeight)
				{
					wall->drawFlags |= WDF_TOP;
				}
				if (wFloorHeight > mFloorHeight)
				{
					wall->drawFlags |= WDF_BOT;
				}
				if (mCeilHeight < wCeilHeight)
				{
					mirror->drawFlags |= WDF_TOP;
				}
				if (mFloorHeight > wFloorHeight)
				{
					mirror->drawFlags |= WDF_BOT;
				}
				wall_computeTexelHeights(wall->mirrorWall);
			}
			wall_computeTexelHeights(wall);
		}
	}

	void TFE_Sectors_Fixed::adjustHeights(RSector* sector, decimal floorOffset, decimal ceilOffset, decimal secondHeightOffset)
	{
		// Adjust objects.
		if (sector->objectCount)
		{
			fixed16_16 heightOffset = secondHeightOffset.f16_16 + floorOffset.f16_16;
			for (s32 i = 0; i < sector->objectCapacity; i++)
			{
				SecObject* obj = sector->objectList[i];
				if (obj)
				{
					// TODO: Adjust sector objects.
				}
			}
		}
		// Adjust sector heights.
		sector->ceilingHeight.f16_16 += ceilOffset.f16_16;
		sector->floorHeight.f16_16 += floorOffset.f16_16;
		sector->secHeight.f16_16 += secondHeightOffset.f16_16;

		// Update wall data.
		s32 wallCount = sector->wallCount;
		RWall* wall = sector->walls;
		for (s32 w = 0; w < wallCount; w++, wall++)
		{
			if (wall->nextSector)
			{
				wall_setupAdjoinDrawFlags(wall);
				wall_computeTexelHeights(wall->mirrorWall);
			}
			wall_computeTexelHeights(wall);
		}

		// Update collision data.
		fixed16_16 floorHeight = sector->floorHeight.f16_16;
		if (sector->flags1 & SEC_FLAGS1_PIT)
		{
			floorHeight += 100 * ONE_16;
		}
		fixed16_16 ceilHeight = sector->ceilingHeight.f16_16;
		if (sector->flags1 & SEC_FLAGS1_EXTERIOR)
		{
			ceilHeight -= 100 * ONE_16;
		}
		fixed16_16 secHeight = sector->floorHeight.f16_16 + sector->secHeight.f16_16;
		if (sector->secHeight.f16_16 >= 0 && floorHeight > secHeight)
		{
			secHeight = floorHeight;
		}

		sector->colFloorHeight.f16_16 = floorHeight;
		sector->colCeilHeight.f16_16 = ceilHeight;
		sector->colSecHeight.f16_16 = secHeight;
		sector->colMinHeight.f16_16 = ceilHeight;
	}

	void TFE_Sectors_Fixed::computeBounds(RSector* sector)
	{
		RWall* wall = sector->walls;
		vec2* w0 = wall->w0;
		fixed16_16 maxX = w0->x.f16_16;
		fixed16_16 maxZ = w0->z.f16_16;
		fixed16_16 minX = maxX;
		fixed16_16 minZ = maxZ;

		for (s32 i = 1; i < sector->wallCount; i++, wall++)
		{
			w0 = wall->w0;

			minX = min(minX, w0->x.f16_16);
			minZ = min(minZ, w0->z.f16_16);

			maxX = max(maxX, w0->x.f16_16);
			maxZ = max(maxZ, w0->z.f16_16);
		}

		sector->minX.f16_16 = minX;
		sector->maxX.f16_16 = maxX;
		sector->minZ.f16_16 = minZ;
		sector->maxZ.f16_16 = maxZ;

		// Setup when needed.
		//s_minX = minX;
		//s_maxX = maxX;
		//s_minZ = minZ;
		//s_maxZ = maxZ;
	}

	void TFE_Sectors_Fixed::copy(RSector* out, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures)
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
				wall->topTexelHeight.f16_16 = 0;
				wall->botTexelHeight.f16_16 = 0;

				wall->w0 = &out->verticesWS[walls[w].i0];
				wall->w1 = &out->verticesWS[walls[w].i1];
				wall->v0 = &out->verticesVS[walls[w].i0];
				wall->v1 = &out->verticesVS[walls[w].i1];
			}

			out->prevDrawFrame = 0;
			out->prevDrawFrame2 = 0;
		}

		update(sector->id);
	}
			
	// In the future, renderer sectors can be changed directly by INF, but for now just copy from the level data.
	// TODO: Currently all sector data is updated - get the "dirty" flag to work reliably so only partial data needs to be updated (textures).
	// TODO: Properly handle switch textures (after reverse-engineering of switch rendering is done).
	void TFE_Sectors_Fixed::update(u32 sectorId, u32 updateFlags)
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
		fixed16_16 ceilDelta  = floatToFixed16(8.0f * (sector->ceilAlt - baseHeight->ceilAlt));
		fixed16_16 floorDelta = floatToFixed16(8.0f * (sector->floorAlt - baseHeight->floorAlt));

		out->ambient.f16_16       = intToFixed16(sector->ambient);
		out->floorHeight.f16_16   = floatToFixed16(sector->floorAlt);
		out->ceilingHeight.f16_16 = floatToFixed16(sector->ceilAlt);
		out->secHeight.f16_16     = floatToFixed16(sector->secAlt);
		out->floorOffsetX.f16_16  = floatToFixed16(sector->floorTexture.offsetX);
		out->floorOffsetZ.f16_16  = floatToFixed16(sector->floorTexture.offsetY);
		out->ceilOffsetX.f16_16   = floatToFixed16(sector->ceilTexture.offsetX);
		out->ceilOffsetZ.f16_16   = floatToFixed16(sector->ceilTexture.offsetY);

		out->flags1        = sector->flags[0];
		out->flags2        = sector->flags[1];
		out->flags3        = sector->flags[2];
		out->floorTex      = texture_getFrame(textures[sector->floorTexture.texId]);
		out->ceilTex       = texture_getFrame(textures[sector->ceilTexture.texId]);
		
		TFE_ZONE_BEGIN(secVtx, "Sector Update Vertices");
		if (updateFlags & SEC_UPDATE_GEO)
		{
			for (s32 v = 0; v < out->vertexCount; v++)
			{
				out->verticesWS[v].x.f16_16 = floatToFixed16(vertices[v].x);
				out->verticesWS[v].z.f16_16 = floatToFixed16(vertices[v].z);
			}
		}
		TFE_ZONE_END(secVtx);
		
		TFE_ZONE_BEGIN(secWall, "Sector Update Walls");
		RWall* wall = out->walls;
		const fixed16_16 midTexelHeight = mul16(intToFixed16(8), floatToFixed16(sector->floorAlt - sector->ceilAlt));
		for (s32 w = 0; w < out->wallCount; w++, wall++)
		{
			wall->nextSector = (walls[w].adjoin >= 0) ? &s_rsectors[walls[w].adjoin] : nullptr;
			wall->mirror = walls[w].mirror;
			wall->mirrorWall = wall->nextSector ? &wall->nextSector->walls[wall->mirror] : nullptr;
			
			wall->topTex  = texture_getFrame(walls[w].top.texId  >= 0 ? textures[walls[w].top.texId]  : nullptr);
			wall->midTex  = texture_getFrame(walls[w].mid.texId  >= 0 ? textures[walls[w].mid.texId]  : nullptr);
			wall->botTex  = texture_getFrame(walls[w].bot.texId  >= 0 ? textures[walls[w].bot.texId]  : nullptr);
			wall->signTex = texture_getFrame(walls[w].sign.texId >= 0 ? textures[walls[w].sign.texId] : nullptr, walls[w].sign.frame);

			if (updateFlags & SEC_UPDATE_GEO)
			{
				const Vec2f offset = { vertices[walls[w].i1].x - vertices[walls[w].i0].x, vertices[walls[w].i1].z - vertices[walls[w].i0].z };
				wall->texelLength.f16_16 = floatToFixed16(8.0f * sqrtf(offset.x*offset.x + offset.z*offset.z));
			}

			// Prime with mid texture height, other heights will be computed as needed.
			wall->midTexelHeight.f16_16 = midTexelHeight;

			// Texture Offsets
			wall->topUOffset.f16_16 = floatToFixed16(8.0f * walls[w].top.offsetX);
			wall->topVOffset.f16_16 = floatToFixed16(8.0f * walls[w].top.offsetY);
			wall->midUOffset.f16_16 = floatToFixed16(8.0f * walls[w].mid.offsetX);
			wall->midVOffset.f16_16 = floatToFixed16(8.0f * walls[w].mid.offsetY);
			wall->botUOffset.f16_16 = floatToFixed16(8.0f * walls[w].bot.offsetX);
			wall->botVOffset.f16_16 = floatToFixed16(8.0f * walls[w].bot.offsetY);
			wall->signUOffset.f16_16 = floatToFixed16(8.0f * walls[w].sign.offsetX);
			wall->signVOffset.f16_16 = floatToFixed16(8.0f * walls[w].sign.offsetY);

			if (walls[w].flags[0] & WF1_TEX_ANCHORED)
			{
				wall->midVOffset.f16_16 -= floorDelta;
				wall->botVOffset.f16_16 -= floorDelta;
				
				const s32 nextId = walls[w].adjoin;
				const SectorBaseHeight* baseHeightNext = (nextId >= 0) ? TFE_Level::getBaseSectorHeight(nextId) : nullptr;
				const Sector* nextSrc = (nextId >= 0) ? &level->sectors[nextId] : nullptr;
				if (nextSrc)
				{
					// Handle next sector moving.
					wall->botVOffset.f16_16 -= floatToFixed16(8.0f * (baseHeightNext->floorAlt - nextSrc->floorAlt));
				}
				wall->topVOffset.f16_16 = -wall->topVOffset.f16_16 + (wall->topTex ? wall->topTex->height : 0);
			}
			if (walls[w].flags[0] & WF1_SIGN_ANCHORED)
			{
				wall->signVOffset.f16_16 -= floorDelta;
				const s32 nextId = walls[w].adjoin;
				const SectorBaseHeight* baseHeightNext = (nextId >= 0) ? TFE_Level::getBaseSectorHeight(nextId) : nullptr;
				const Sector* nextSrc = (nextId >= 0) ? &level->sectors[nextId] : nullptr;
				if (nextSrc)
				{
					// Handle next sector moving.
					wall->signVOffset.f16_16 -= floatToFixed16(8.0f * (baseHeightNext->floorAlt - nextSrc->floorAlt));
				}
			}

			wall->flags1 = walls[w].flags[0];
			wall->flags2 = walls[w].flags[1];
			wall->flags3 = walls[w].flags[2];

			wall->wallLight = walls[w].light;
		}
		TFE_ZONE_END(secWall);
	}
		
	void TFE_Sectors_Fixed::adjoin_setupAdjoinWindow(s32* winBot, s32* winBotNext, s32* winTop, s32* winTopNext, EdgePair* adjoinEdges, s32 adjoinCount)
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

			const fixed16_16 ceil_dYdX = adjoinEdges->dyCeil_dx.f16_16;
			fixed16_16 y = adjoinEdges->yCeil0.f16_16;
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
			const fixed16_16 floor_dYdX = adjoinEdges->dyFloor_dx.f16_16;
			y = adjoinEdges->yFloor0.f16_16;
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

	void TFE_Sectors_Fixed::adjoin_computeWindowBounds(EdgePair* adjoinEdges)
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
		s_windowTopPrev = s_windowTop;
		s_windowBotPrev = s_windowBot;
		s_prevSector = s_curSector;
	}

	void TFE_Sectors_Fixed::saveValues(s32 index)
	{
		SectorSaveValues* dst = &s_sectorStack[index];
		dst->curSector = s_curSector;
		dst->prevSector = s_prevSector;
		dst->depth1d.f16_16 = s_depth1d_Fixed;
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
		dst->windowTopPrev = s_windowTopPrev;
		dst->windowBotPrev = s_windowBotPrev;
		dst->sectorAmbient = s_sectorAmbient;
		dst->scaledAmbient = s_scaledAmbient;
		//dst->scaledAmbient2k = s_scaledAmbient2k;
	}

	void TFE_Sectors_Fixed::restoreValues(s32 index)
	{
		const SectorSaveValues* src = &s_sectorStack[index];
		s_curSector = src->curSector;
		s_prevSector = src->prevSector;
		s_depth1d_Fixed = src->depth1d.f16_16;
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
		s_windowTopPrev = src->windowTopPrev;
		s_windowBotPrev = src->windowBotPrev;
		s_sectorAmbient = src->sectorAmbient;
		s_scaledAmbient = src->scaledAmbient;
		//s_scaledAmbient2k = src->scaledAmbient2k;
	}
}