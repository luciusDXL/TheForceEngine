#include "rendererClassic.h"
#include "fixedPoint.h"
#include "rsector.h"
#include "rwall.h"
#include "rcommon.h"

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

	static s32 s_curWallSeg;
	static s32 s_drawFrame = 0;
		
	void loadLevel();
	void drawSector();
	void copySector(RSector* out, RSector* sectorList, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures);

	void init()
	{
		if (s_init) { return; }
		s_init = true;

		// Build tables.
		// Setup resolution, projection parameters, etc.

		s_width  = 320;
		s_height = 200;
		s_halfWidth  = intToFixed16(s_width >> 1);
		s_halfHeight = intToFixed16(s_height >> 1);
		s_focalLength = s_halfWidth;

		s_minScreenX = 0;
		s_maxScreenX = s_width - 1;
		s_minSegZ = 0;

		s_depth1d = (s32*)realloc(s_depth1d, s_width * sizeof(s32));
		s_columnTop = (u8*)realloc(s_columnTop, s_width);
		s_columnBot = (u8*)realloc(s_columnBot, s_width);
		s_windowTop = (u8*)realloc(s_windowTop, s_width);
		s_windowBot = (u8*)realloc(s_windowBot, s_width);

		// Build tables
		s_column_Y_Over_X = (s32*)realloc(s_column_Y_Over_X, s_width * sizeof(s32));
		s_column_X_Over_Y = (s32*)realloc(s_column_X_Over_Y, s_width * sizeof(s32));
		s32 halfWidth = s_width >> 1;
		for (s32 x = 0; x < s_width; x++)
		{
			s_column_Y_Over_X[x] = (x != halfWidth) ? div16(s_halfWidth, intToFixed16(x - halfWidth)) : s_halfWidth;
			s_column_X_Over_Y[x] = div16(intToFixed16(x - halfWidth), s_halfWidth);
		}

		s_rcp_yMinusHalfHeight = (s32*)realloc(s_rcp_yMinusHalfHeight, s_height * sizeof(s32));
		s32 halfHeight = s_height >> 1;
		for (s32 y = 0; y < s_height; y++)
		{
			s32 yMinusHalf = y - halfHeight;
			s_rcp_yMinusHalfHeight[y] = (yMinusHalf != 0) ? 65536 / yMinusHalf : 65536;
		}

		s_maxWallCount = 0xffff;
		CVAR_INT(s_maxWallCount, "d_maxWallCount", 0, "Maximum wall count for a given sector.");
	}
	
	void setupLevel()
	{
		init();
		s_memPool.init(32 * 1024 * 1024, "Classic Renderer - Software");
		s_sectorId = -1;

		loadLevel();
	}

	void setCamera(s32 cosYaw, s32 sinYaw, s32 x, s32 y, s32 z, s32 sectorId)
	{
		s_cosYaw = cosYaw;
		s_sinYaw = sinYaw;
		s_negSinYaw = -sinYaw;

		s_zCameraTrans = mul16(-z, s_cosYaw) + mul16(-x, s_negSinYaw);
		s_xCameraTrans = mul16(-x, s_cosYaw) + mul16(-z, s_sinYaw);
		s_eyeHeight = y;

		s_sectorId = sectorId;
		s_cameraLightSource = 0;
		s_worldAmbient = 31;
		const LightMode mode = TFE_RenderCommon::getLightMode();
		if (mode != LIGHT_OFF)
		{
			s_worldAmbient = (mode == LIGHT_NORMAL) ? 0 : -9;
			s_cameraLightSource = -1;
		}
		
		s_drawFrame++;
	}
		
	void draw(u8* display, const ColorMap* colormap)
	{
		// Clear the screen for now so we can get away with only drawing walls.
		memset(display, 0, s_width * s_height);
		s_display = display;
		s_colorMap = colormap->colorMap;
		s_lightSourceRamp = colormap->lightSourceRamp;

		s_windowMinX = s_minScreenX;
		s_windowMaxX = s_maxScreenX;
		s_windowMinY = 1;
		s_windowMaxY = s_height - 1;
		s_wallCount  = 0;
		s_nextWall   = 0;
		s_curWallSeg = 0;
		memset(s_depth1d, 0, s_width * sizeof(u32));

		for (s32 i = 0; i < s_width; i++)
		{
			s_columnTop[i] = 1;
			s_columnBot[i] = s_height - 1;
			s_windowTop[i] = 1;
			s_windowBot[i] = s_height - 1;
		}
						
		// Draws a single sector.
		s_curSector = &s_rsectors[s_sectorId];
		drawSector();
	}

	// In the future, renderer sectors can be changed directly by INF, but for now just copy from the level data.
	void updateSector(u32 sectorId)
	{
		LevelData* level = TFE_LevelAsset::getLevelData();
		Texture** textures = level->textures.data();

		Sector* sector = &level->sectors[sectorId];
		SectorWall* walls = level->walls.data() + sector->wallOffset;
		Vec2f* vertices = level->vertices.data() + sector->vtxOffset;

		RSector* out = &s_rsectors[sectorId];
		out->ambientFixed  = sector->ambient << 16;
		out->floorHeight   = s32(sector->floorAlt * 65536.0f);
		out->ceilingHeight = s32(sector->ceilAlt * 65536.0f);
		out->secHeight     = s32(sector->secAlt * 65536.0f);
		out->flags1 = sector->flags[0];
		out->flags2 = sector->flags[1];
		out->flags3 = sector->flags[2];

		out->floorOffsetX = mul16(intToFixed16(8), s32(sector->floorTexture.offsetX * 65536.0f));
		out->floorOffsetZ = mul16(intToFixed16(8), s32(sector->floorTexture.offsetY * 65536.0f));
		out->ceilOffsetX  = mul16(intToFixed16(8), s32(sector->ceilTexture.offsetX  * 65536.0f));
		out->ceilOffsetZ  = mul16(intToFixed16(8), s32(sector->ceilTexture.offsetY  * 65536.0f));

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

	//////////////////////////////////////////////////
	// Internal
	//////////////////////////////////////////////////
	s32 wallSortX(const void* r0, const void* r1)
	{
		return ((const RWallSegment*)r0)->wallX0 - ((const RWallSegment*)r1)->wallX0;
	}

	void drawSector()
	{
		s32 startWall     = s_curSector->startWall;
		s32 drawWallCount = s_curSector->drawWallCnt;

		s_sectorAmbient = round16(s_curSector->ambientFixed);
		s_scaledAmbient = (s_sectorAmbient >> 1) + (s_sectorAmbient >> 2) + (s_sectorAmbient >> 3);

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

			startWall   = s_nextWall;
			RWall* wall = s_curSector->walls;
			for (s32 i = 0; i < s_curSector->wallCount; i++, wall++)
			{
				wall_process(wall);
			}
			drawWallCount = s_nextWall - startWall;

			s_curSector->startWall     = startWall;
			s_curSector->drawWallCnt   = drawWallCount;
			s_curSector->prevDrawFrame = s_drawFrame;
		}

		RWallSegment* wallSegment = &s_wallSegListDst[s_curWallSeg];
		s32 drawSegCnt = wall_mergeSort(wallSegment, MAX_SEG - s_curWallSeg, startWall, drawWallCount);
		s_curWallSeg  += drawSegCnt;

		qsort(wallSegment, drawSegCnt, sizeof(RWallSegment), wallSortX);

		// Draw each wall segment in the sector.
		for (s32 i = 0; i < drawSegCnt; i++, wallSegment++)
		{
			RWall* srcWall = wallSegment->srcWall;
			//RSector* nextSector = srcWall->sector;
			// TODO: port wallSegment render flags and handle different configurations.
			RSector* nextSector = nullptr;

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

	void copySector(RSector* out, RSector* sectorList, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures)
	{
		out->vertexCount = sector->vtxCount;
		out->wallCount = sector->wallCount;

		out->ambientFixed  = sector->ambient << 16;
		out->floorHeight   = s32(sector->floorAlt * 65536.0f);
		out->ceilingHeight = s32(sector->ceilAlt * 65536.0f);
		out->secHeight     = s32(sector->secAlt * 65536.0f);
		out->flags1        = sector->flags[0];
		out->flags2        = sector->flags[1];
		out->flags3        = sector->flags[2];
		out->startWall     = 0;
		out->drawWallCnt   = 0;
		out->floorTex = &textures[sector->floorTexture.texId]->frames[0];
		out->ceilTex = &textures[sector->ceilTexture.texId]->frames[0];
		out->floorOffsetX = mul16(intToFixed16(8), s32(sector->floorTexture.offsetX * 65536.0f));
		out->floorOffsetZ = mul16(intToFixed16(8), s32(sector->floorTexture.offsetY * 65536.0f));
		out->ceilOffsetX  = mul16(intToFixed16(8), s32(sector->ceilTexture.offsetX  * 65536.0f));
		out->ceilOffsetZ  = mul16(intToFixed16(8), s32(sector->ceilTexture.offsetY  * 65536.0f));

		if (!out->verticesWS)
		{
			out->verticesWS = (vec2*)s_memPool.allocate(sizeof(vec2) * out->vertexCount);
			out->verticesVS = (vec2*)s_memPool.allocate(sizeof(vec2) * out->vertexCount);
			out->walls = (RWall*)s_memPool.allocate(sizeof(RWall) * out->wallCount);
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
			wall->nextSector = (walls[w].adjoin >= 0) ? &sectorList[walls[w].adjoin] : nullptr;

			wall->w0 = &out->verticesWS[walls[w].i0];
			wall->w1 = &out->verticesWS[walls[w].i1];
			wall->v0 = &out->verticesVS[walls[w].i0];
			wall->v1 = &out->verticesVS[walls[w].i1];

			wall->topTex  = walls[w].top.texId  >= 0 && textures[walls[w].top.texId]  ? &textures[walls[w].top.texId]->frames[0]  : nullptr;
			wall->midTex  = walls[w].mid.texId  >= 0 && textures[walls[w].mid.texId]  ? &textures[walls[w].mid.texId]->frames[0]  : nullptr;
			wall->botTex  = walls[w].bot.texId  >= 0 && textures[walls[w].bot.texId]  ? &textures[walls[w].bot.texId]->frames[0]  : nullptr;
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
		memset(s_rsectors, 0, sizeof(RSector) * level->sectors.size());
		Texture** textures = level->textures.data();
		for (u32 i = 0; i < s_sectorCount; i++)
		{
			Sector* sector = &level->sectors[i];
			SectorWall* walls = level->walls.data() + sector->wallOffset;
			Vec2f* vertices = level->vertices.data() + sector->vtxOffset;

			copySector(&s_rsectors[i], s_rsectors, sector, walls, vertices, textures);
		}
	}
}
