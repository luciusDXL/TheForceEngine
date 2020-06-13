#include "rwall.h"
#include "rflat.h"
#include "rlighting.h"
#include "rsector.h"
#include "fixedPoint.h"
#include "rmath.h"
#include "rcommon.h"
#include <TFE_Renderer/renderer.h>
#include <TFE_System/system.h>
#include <TFE_System/math.h>
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
using namespace RClassicLighting;
using namespace FixedPoint;
using namespace RMath;

namespace RClassicSector
{
	static RSector* s_curSector;
	static RSector* s_rsectors;
	static MemoryPool* s_memPool;
	static u32 s_sectorCount;
	
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

	void sector_setCurrent(RSector* sector)
	{
		s_curSector = sector;
	}
		
	s32 wallSortX(const void* r0, const void* r1)
	{
		return ((const RWallSegment*)r0)->wallX0 - ((const RWallSegment*)r1)->wallX0;
	}

	void sector_draw()
	{
		s32 startWall = s_curSector->startWall;
		s32 drawWallCount = s_curSector->drawWallCnt;

		s_sectorAmbient = round16(s_curSector->ambientFixed);
		s_scaledAmbient = (s_sectorAmbient >> 1) + (s_sectorAmbient >> 2) + (s_sectorAmbient >> 3);

		s_wallMaxCeilY = s_windowMinY;
		s_wallMinFloorY = s_windowMaxY;

		if (s_drawFrame != s_curSector->prevDrawFrame)
		{
			vec2* vtxWS = s_curSector->verticesWS;
			vec2* vtxVS = s_curSector->verticesVS;
			for (s32 v = 0; v < s_curSector->vertexCount; v++)
			{
				vtxVS->x = mul16(vtxWS->x, s_cosYaw) + mul16(vtxWS->z, s_sinYaw) + s_xCameraTrans;
				vtxVS->z = mul16(vtxWS->x, s_negSinYaw) + mul16(vtxWS->z, s_cosYaw) + s_zCameraTrans;
				vtxVS++;
				vtxWS++;
			}

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
		}

		RWallSegment* wallSegment = &s_wallSegListDst[s_curWallSeg];
		s32 drawSegCnt = wall_mergeSort(wallSegment, MAX_SEG - s_curWallSeg, startWall, drawWallCount);
		s_curWallSeg += drawSegCnt;

		qsort(wallSegment, drawSegCnt, sizeof(RWallSegment), wallSortX);

		s32 flatCount = s_flatCount;
		FlatEdges* lowerFlatEdge = &s_lowerFlatEdgeList[s_flatCount];
		s_lowerFlatEdge = lowerFlatEdge;

		// Draw each wall segment in the sector.
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

		// Draw flats
		// Note: in the DOS code flat drawing functions are called through function pointers.
		// Since the function pointers always seem to be the same, the functions are called directly in this code.
		// Most likely this was used for testing or debug drawing and may be added back in the future.
		const s32 newFlatCount = s_flatCount - flatCount;
		if (s_curSector->flags1 & SEC_FLAGS1_EXTERIOR)
		{
			// TODO - just leave black for now.
		}
		else
		{
			flat_drawCeiling(s_curSector, lowerFlatEdge, newFlatCount);
		}
		if (s_curSector->flags1 & SEC_FLAGS1_PIT)
		{
			// TODO - just leave black for now.
		}
		else
		{
			flat_drawFloor(s_curSector, lowerFlatEdge, newFlatCount);
		}
	}

	// Setup wall flags not from the original code, still to be replaced.
	void sector_setupWallDrawFlags(RSector* sector)
	{
		RWall* wall = sector->walls;
		const f32 midHeight = mul16(intToFixed16(8), sector->floorHeight - sector->ceilingHeight);
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
			}
			else if (next->floorHeight < sector->floorHeight)
			{
				wall->drawFlags = WDF_BOT;
				wall->botTexelHeight = mul16(intToFixed16(8), sector->floorHeight - next->floorHeight);
			}
			else if (next->ceilingHeight > sector->ceilingHeight)
			{
				wall->drawFlags = WDF_TOP;
				wall->topTexelHeight = mul16(intToFixed16(8), next->ceilingHeight - sector->ceilingHeight);
			}
		}
	}

	void sector_copy(RSector* out, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures)
	{
		out->vertexCount = sector->vtxCount;
		out->wallCount = sector->wallCount;

		out->ambientFixed = sector->ambient << 16;
		out->floorHeight = s32(sector->floorAlt * 65536.0f);
		out->ceilingHeight = s32(sector->ceilAlt * 65536.0f);
		out->secHeight = s32(sector->secAlt * 65536.0f);
		out->flags1 = sector->flags[0];
		out->flags2 = sector->flags[1];
		out->flags3 = sector->flags[2];
		out->startWall = 0;
		out->drawWallCnt = 0;
		out->floorTex = &textures[sector->floorTexture.texId]->frames[0];
		out->ceilTex = &textures[sector->ceilTexture.texId]->frames[0];
		out->floorOffsetX = s32(sector->floorTexture.offsetX * 65536.0f);
		out->floorOffsetZ = s32(sector->floorTexture.offsetY * 65536.0f);
		out->ceilOffsetX = s32(sector->ceilTexture.offsetX  * 65536.0f);
		out->ceilOffsetZ = s32(sector->ceilTexture.offsetY  * 65536.0f);

		if (!out->verticesWS)
		{
			out->verticesWS = (vec2*)s_memPool->allocate(sizeof(vec2) * out->vertexCount);
			out->verticesVS = (vec2*)s_memPool->allocate(sizeof(vec2) * out->vertexCount);
			out->walls = (RWall*)s_memPool->allocate(sizeof(RWall) * out->wallCount);
		}
		
		for (s32 v = 0; v < out->vertexCount; v++)
		{
			out->verticesWS[v].x = s32(vertices[v].x * 65536.0f);
			out->verticesWS[v].z = s32(vertices[v].z * 65536.0f);
		}

		const s32 midTexelHeight = mul16(intToFixed16(8), s32((sector->floorAlt - sector->ceilAlt) * 65536.0f));

		RWall* wall = out->walls;
		for (s32 w = 0; w < out->wallCount; w++, wall++)
		{
			wall->sector = out;
			wall->nextSector = (walls[w].adjoin >= 0) ? &s_rsectors[walls[w].adjoin] : nullptr;

			wall->w0 = &out->verticesWS[walls[w].i0];
			wall->w1 = &out->verticesWS[walls[w].i1];
			wall->v0 = &out->verticesVS[walls[w].i0];
			wall->v1 = &out->verticesVS[walls[w].i1];

			wall->topTex = walls[w].top.texId >= 0 && textures[walls[w].top.texId] ? &textures[walls[w].top.texId]->frames[0] : nullptr;
			wall->midTex = walls[w].mid.texId >= 0 && textures[walls[w].mid.texId] ? &textures[walls[w].mid.texId]->frames[0] : nullptr;
			wall->botTex = walls[w].bot.texId >= 0 && textures[walls[w].bot.texId] ? &textures[walls[w].bot.texId]->frames[0] : nullptr;
			wall->signTex = walls[w].sign.texId >= 0 && textures[walls[w].sign.texId] ? &textures[walls[w].sign.texId]->frames[0] : nullptr;

			const Vec2f offset = { vertices[walls[w].i1].x - vertices[walls[w].i0].x, vertices[walls[w].i1].z - vertices[walls[w].i0].z };
			f32 len = sqrtf(offset.x * offset.x + offset.z * offset.z);
			wall->texelLength = mul16(intToFixed16(8), s32(len * 65536.0f));

			// For now just assume solid walls.
			wall->topTexelHeight = 0;
			wall->botTexelHeight = 0;
			wall->midTexelHeight = midTexelHeight;

			// Texture Offsets
			wall->topUOffset = mul16(intToFixed16(8), s32(walls[w].top.offsetX * 65536.0f));
			wall->topVOffset = mul16(intToFixed16(8), s32(walls[w].top.offsetY * 65536.0f));
			wall->midUOffset = mul16(intToFixed16(8), s32(walls[w].mid.offsetX * 65536.0f));
			wall->midVOffset = mul16(intToFixed16(8), s32(walls[w].mid.offsetY * 65536.0f));
			wall->botUOffset = mul16(intToFixed16(8), s32(walls[w].bot.offsetX * 65536.0f));
			wall->botVOffset = mul16(intToFixed16(8), s32(walls[w].bot.offsetY * 65536.0f));

			wall->drawFrame = 0;
			wall->drawFlags = WDF_MIDDLE;

			wall->flags1 = walls[w].flags[0];
			wall->flags2 = walls[w].flags[1];
			wall->flags3 = walls[w].flags[2];

			wall->wallLight = walls[w].light;
		}
	}

	// In the future, renderer sectors can be changed directly by INF, but for now just copy from the level data.
	void sector_update(u32 sectorId)
	{
		LevelData* level = TFE_LevelAsset::getLevelData();
		Texture** textures = level->textures.data();

		Sector* sector = &level->sectors[sectorId];
		SectorWall* walls = level->walls.data() + sector->wallOffset;
		Vec2f* vertices = level->vertices.data() + sector->vtxOffset;

		RSector* out = &s_rsectors[sectorId];
		out->ambientFixed = sector->ambient << 16;
		out->floorHeight = s32(sector->floorAlt * 65536.0f);
		out->ceilingHeight = s32(sector->ceilAlt * 65536.0f);
		out->secHeight = s32(sector->secAlt * 65536.0f);
		out->flags1 = sector->flags[0];
		out->flags2 = sector->flags[1];
		out->flags3 = sector->flags[2];

		out->floorOffsetX = s32(sector->floorTexture.offsetX * 65536.0f);
		out->floorOffsetZ = s32(sector->floorTexture.offsetY * 65536.0f);
		out->ceilOffsetX = s32(sector->ceilTexture.offsetX  * 65536.0f);
		out->ceilOffsetZ = s32(sector->ceilTexture.offsetY  * 65536.0f);

		for (s32 v = 0; v < out->vertexCount; v++)
		{
			out->verticesWS[v].x = s32(vertices[v].x * 65536.0f);
			out->verticesWS[v].z = s32(vertices[v].z * 65536.0f);
		}

		const s32 midTexelHeight = mul16(intToFixed16(8), s32((sector->floorAlt - sector->ceilAlt) * 65536.0f));

		RWall* wall = out->walls;
		for (s32 w = 0; w < out->wallCount; w++)
		{
			wall->nextSector = (walls[w].adjoin >= 0) ? &s_rsectors[walls[w].adjoin] : nullptr;

			wall->topTex = walls[w].top.texId >= 0 ? &textures[walls[w].top.texId]->frames[0] : nullptr;
			wall->midTex = walls[w].mid.texId >= 0 ? &textures[walls[w].mid.texId]->frames[0] : nullptr;
			wall->botTex = walls[w].bot.texId >= 0 ? &textures[walls[w].bot.texId]->frames[0] : nullptr;
			wall->signTex = walls[w].sign.texId >= 0 ? &textures[walls[w].sign.texId]->frames[0] : nullptr;

			const Vec2f offset = { vertices[walls[w].i1].x - vertices[walls[w].i0].x, vertices[walls[w].i1].z - vertices[walls[w].i0].z };
			const f32 len = sqrtf(offset.x * offset.x + offset.z * offset.z);
			wall->texelLength = mul16(intToFixed16(8), s32(len * 65536.0f));

			// For now just assume solid walls.
			wall->topTexelHeight = 0;
			wall->botTexelHeight = 0;
			wall->midTexelHeight = midTexelHeight;

			// Texture Offsets
			wall->topUOffset = mul16(intToFixed16(8), s32(walls[w].top.offsetX * 65536.0f));
			wall->topVOffset = mul16(intToFixed16(8), s32(walls[w].top.offsetY * 65536.0f));
			wall->midUOffset = mul16(intToFixed16(8), s32(walls[w].mid.offsetX * 65536.0f));
			wall->midVOffset = mul16(intToFixed16(8), s32(walls[w].mid.offsetY * 65536.0f));
			wall->botUOffset = mul16(intToFixed16(8), s32(walls[w].bot.offsetX * 65536.0f));
			wall->botVOffset = mul16(intToFixed16(8), s32(walls[w].bot.offsetY * 65536.0f));

			wall->flags1 = walls[w].flags[0];
			wall->flags2 = walls[w].flags[1];
			wall->flags3 = walls[w].flags[2];

			wall->wallLight = walls[w].light;
		}
	}
}