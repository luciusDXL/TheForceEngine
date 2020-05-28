#include "rendererClassic.h"
#include "fixedPoint.h"
#include "rsector.h"
#include "rwall.h"

#include <TFE_Renderer/renderer.h>
#include <TFE_System/system.h>
#include <TFE_System/math.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Asset/levelAsset.h>
#include <TFE_Asset/levelObjectsAsset.h>
#include <TFE_Asset/textureAsset.h>
#include <TFE_Game/level.h>
#include <TFE_Asset/spriteAsset.h>
#include <TFE_Asset/textureAsset.h>
#include <TFE_Asset/paletteAsset.h>
#include <TFE_Asset/colormapAsset.h>
#include <TFE_Asset/modelAsset.h>
#include <TFE_LogicSystem/logicSystem.h>
#include <TFE_FrontEndUI/console.h>
#include <TFE_System/memoryPool.h>
#include <algorithm>
#include <assert.h>

using namespace FixedPoint;
using namespace RClassicWall;

namespace RendererClassic
{
	s32 s_xCameraTrans;
	s32 s_zCameraTrans;

	s32 s_cosYaw;
	s32 s_sinYaw;
	s32 s_negSinYaw;

	s32 s_sectorId = -1;

	static bool s_init = false;
	static MemoryPool s_memPool;
	static RSector* s_rsectors;
	static RSector* s_curSector;
	static u32 s_sectorCount;

	static s32 s_nextWall;
	static s32 s_curWallSeg;
	static s32 s_drawFrame = 0;
		
	static RWallSegment s_wallSegListDst[MAX_SEG];
	static RWallSegment s_wallSegListSrc[MAX_SEG];

	void loadLevel();
	void copySector(RSector* out, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures);

	void init()
	{
		if (s_init) { return; }
		s_init = false;

		// ...
	}
	
	void setupLevel()
	{
		init();
		s_memPool.init(2 * 1024 * 1024, "Classic Renderer - Software");
		s_sectorId = -1;

		loadLevel();
	}

	void setCamera(s32 cosYaw, s32 sinYaw, s32 x, s32 y, s32 z, s32 sectorId)
	{
		s_cosYaw = cosYaw;
		s_sinYaw = sinYaw;
		s_negSinYaw = -sinYaw;

		s_zCameraTrans = mul16(z, s_cosYaw) + mul16(x, s_negSinYaw);
		s_xCameraTrans = mul16(x, s_cosYaw) + mul16(z, s_sinYaw);

		if (s_sectorId != sectorId)
		{
			s_memPool.clear();
		}
		s_sectorId = sectorId;
		s_curSector = &s_rsectors[sectorId];

		s_nextWall = 0;
		s_curWallSeg = 0;
		s_drawFrame++;
	}

	s32 wallSortX(const void* r0, const void* r1)	// eax, edx
	{
		return ((const RWallSegment*)r0)->wallX0 - ((const RWallSegment*)r1)->wallX0;
	}

	void draw(u8* display)
	{
		// Clear the screen for now so we can get away with only drawing walls.
		memset(display, 15, 320 * 200);

		// Draws a single sector.

		// For now we are just drawing a single sector...
		// Always updating the sector for now since there is only one.
		s32 startWall = 0;
		s32 drawWallCount = 0;
		if (s_drawFrame != s_curSector->prevDrawFrame)
		{
			vec2* vtxWS = s_curSector->verticesWS;
			vec2* vtxVS = s_curSector->verticesVS;
			for (s32 v = 0; v < s_curSector->vertexCount; v++)
			{
				vtxVS->x = mul16(vtxWS->x, s_cosYaw)    + mul16(vtxWS->z, s_sinYaw) + s_xCameraTrans;
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

			s_curSector->startWall = startWall;
			s_curSector->prevDrawFrame = s_drawFrame;
			drawWallCount = s_nextWall - startWall;
			s_curSector->drawWallCnt = drawWallCount;
		}
		else
		{
			startWall = s_curSector->startWall;
			drawWallCount = s_curSector->drawWallCnt;
		}

		RWallSegment* wallSegment = &s_wallSegListDst[s_curWallSeg];
		s32 addedCount = wall_mergeSort(wallSegment, MAX_SEG - s_curWallSeg, startWall, drawWallCount);
		s_curWallSeg += addedCount;

		qsort(wallSegment, addedCount, sizeof(RWallSegment), wallSortX);

		// for now assume every wall is solid
		for (s32 i = 0; i < addedCount; i++, wallSegment++)
		{
			RWall* srcWall = wallSegment->srcWall;
			RSector* nextSector = srcWall->sector;

			// This will always be true for now.
			if (!nextSector)
			{
				wall_drawSolid(wallSegment);
			}
			else
			{
				// ...
			}
		}
	}

	void updateSector(u32 sectorId)
	{
		LevelData* level = TFE_LevelAsset::getLevelData();
		Texture** textures = level->textures.data();

		Sector* sector = &level->sectors[sectorId];
		SectorWall* walls = level->walls.data() + sector->wallOffset;
		Vec2f* vertices = level->vertices.data() + sector->vtxOffset;

		copySector(&s_rsectors[sectorId], sector, walls, vertices, textures);
	}

	// Internal
	void copySector(RSector* out, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures)
	{
		memset(out, 0, sizeof(RSector));
		out->vertexCount = sector->vtxCount;
		out->wallCount = sector->wallCount;

		out->ambientFixed = sector->ambient << 16;
		out->floorHeight = s32(sector->floorAlt * 65536.0f);
		out->ceilingHeight = s32(sector->ceilAlt * 65536.0f);
		out->secHeight = s32(sector->secAlt * 65536.0f);
		out->flags1 = sector->flags[0];
		out->startWall = 0;
		out->drawWallCnt = 0;
		
		out->verticesWS = (vec2*)s_memPool.allocate(sizeof(vec2) * out->vertexCount);
		out->verticesVS = (vec2*)s_memPool.allocate(sizeof(vec2) * out->vertexCount);
		out->walls = (RWall*)s_memPool.allocate(sizeof(RWall) * out->wallCount);

		for (s32 v = 0; v < out->vertexCount; v++)
		{
			out->verticesWS[v].x = s32(vertices[v].x * 65536.0f);
			out->verticesWS[v].z = s32(vertices[v].z * 65536.0f);
		}
		memset(out->walls, 0, sizeof(RWall) * out->wallCount);
		RWall* wall = out->walls;
		for (s32 w = 0; w < out->wallCount; w++, wall++)
		{
			wall->sector = out;
			wall->nextSector = nullptr;

			wall->w0 = &out->verticesWS[walls[w].i0];
			wall->w1 = &out->verticesWS[walls[w].i1];
			wall->v0 = &out->verticesVS[walls[w].i0];
			wall->v1 = &out->verticesVS[walls[w].i1];

			wall->topTex  = walls[w].top.texId  >= 0 ? &textures[walls[w].top.texId]->frames[0]  : nullptr;
			wall->midTex  = walls[w].mid.texId  >= 0 ? &textures[walls[w].mid.texId]->frames[0]  : nullptr;
			wall->botTex  = walls[w].bot.texId  >= 0 ? &textures[walls[w].bot.texId]->frames[0]  : nullptr;
			wall->signTex = walls[w].sign.texId >= 0 ? &textures[walls[w].sign.texId]->frames[0] : nullptr;

			const Vec2f offset = { vertices[walls[w].i1].x - vertices[walls[w].i0].x, vertices[walls[w].i1].z - vertices[walls[w].i0].z };
			f32 len = sqrtf(offset.x * offset.x + offset.z * offset.z);
			wall->texelLength = mul16(intToFixed16(8), s32(len * 65536.0f));

			// For now just assume solid walls.
			wall->topTexelHeight = 0;
			wall->botTexelHeight = 0;
			wall->midTexelHeight = mul16(intToFixed16(8), s32((sector->floorAlt - sector->ceilAlt) * 65536.0f));

			// Texture Offsets
			wall->topUOffset = mul16(intToFixed16(8), s32(walls[w].top.offsetX * 65536.0f));
			wall->topVOffset = mul16(intToFixed16(8), s32(walls[w].top.offsetY * 65536.0f));
			wall->midUOffset = mul16(intToFixed16(8), s32(walls[w].mid.offsetX * 65536.0f));
			wall->midVOffset = mul16(intToFixed16(8), s32(walls[w].mid.offsetY * 65536.0f));
			wall->botUOffset = mul16(intToFixed16(8), s32(walls[w].bot.offsetX * 65536.0f));
			wall->botVOffset = mul16(intToFixed16(8), s32(walls[w].bot.offsetY * 65536.0f));

			wall->drawFrame = 0;

			wall->flags1 = walls[w].flags[0];
			wall->flags2 = walls[w].flags[1];
			wall->flags3 = walls[w].flags[2];

			wall->wallLight = walls[w].light;
		}
	}

	void loadLevel()
	{
		LevelData* level = TFE_LevelAsset::getLevelData();
		s_sectorCount = (u32)level->sectors.size();
		s_rsectors = (RSector*)s_memPool.allocate(sizeof(RSector) * level->sectors.size());
		Texture** textures = level->textures.data();
		for (u32 i = 0; i < s_sectorCount; i++)
		{
			Sector* sector = &level->sectors[i];
			SectorWall* walls = level->walls.data() + sector->wallOffset;
			Vec2f* vertices = level->vertices.data() + sector->vtxOffset;

			copySector(&s_rsectors[i], sector, walls, vertices, textures);
		}
	}
}
