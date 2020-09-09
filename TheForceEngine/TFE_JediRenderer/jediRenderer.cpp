#include "jediRenderer.h"
#include "fixedPoint.h"
#include "rcommon.h"
#include "rsector.h"
#include "RClassic_Fixed/rcommonFixed.h"
#include "RClassic_Fixed/rclassicFixed.h"
#include "RClassic_Fixed/rsectorFixed.h"

#include "RClassic_Float/rclassicFloat.h"
#include "RClassic_Float/rsectorFloat.h"

#include <TFE_System/profiler.h>
// VVV - this needs to be moved or fixed.
#include <TFE_Game/level.h>
#include <TFE_FrontEndUI/console.h>
#include <TFE_System/memoryPool.h>

namespace TFE_JediRenderer
{
	static s32 s_sectorId = -1;

	static bool s_init = false;
	static MemoryPool s_memPool;
	static TFE_SubRenderer s_subRenderer = TSR_INVALID;
	static TFE_Sectors* s_sectors = nullptr;

	/////////////////////////////////////////////
	// Forward Declarations
	/////////////////////////////////////////////
	void clear1dDepth();
	void updateSectors();
	void buildLevelData();

	/////////////////////////////////////////////
	// Implementation
	/////////////////////////////////////////////
	void init()
	{
		if (s_init) { return; }
		s_init = true;
		// Setup Debug CVars.
		s_maxWallCount = 0xffff;
		s_maxDepthCount = 0xffff;
		CVAR_INT(s_maxWallCount,  "d_maxWallCount",  0, "Maximum wall count for a given sector.");
		CVAR_INT(s_maxDepthCount, "d_maxDepthCount", 0, "Maximum adjoin depth count.");
		// Setup performance counters.
		TFE_COUNTER(s_maxAdjoinDepth, "Maximum Adjoin Depth");
		TFE_COUNTER(s_maxAdjoinIndex, "Maximum Adjoin Count");
		TFE_COUNTER(s_sectorIndex,    "Sector Count");
		TFE_COUNTER(s_flatCount,      "Flat Count");
		TFE_COUNTER(s_curWallSeg,     "Wall Segment Count");
		TFE_COUNTER(s_adjoinSegCount, "Adjoin Segment Count");
	}

	void destroy()
	{
		delete s_sectors;
	}

	void setResolution(s32 width, s32 height)
	{
		if (s_subRenderer == TSR_CLASSIC_FIXED) { RClassic_Fixed::setResolution(width, height); }
		else { RClassic_Float::setResolution(width, height); }
	}

	void setupLevel(s32 width, s32 height)
	{
		init();
		setResolution(width, height);
				
		s_memPool.init(32 * 1024 * 1024, "Classic Renderer - Software");
		s_sectorId = -1;
		s_sectors->setMemoryPool(&s_memPool);
		
		buildLevelData();
	}

	void setSubRenderer(TFE_SubRenderer subRenderer/* = TSR_CLASSIC_FIXED*/)
	{
		if (subRenderer != s_subRenderer)
		{
			s_subRenderer = subRenderer;

			// Setup the sub-renderer sector system.
			TFE_Sectors* prev = s_sectors;
			
			if (s_subRenderer == TSR_CLASSIC_FIXED)
			{
				s_sectors = new TFE_Sectors_Fixed();
			}
			else
			{
				s_sectors = new TFE_Sectors_Float();
			}
			s_sectors->copyFrom(prev);
			delete prev;
		}
	}
		
	void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId, s32 worldAmbient, bool cameraLightSource)
	{
		if (s_subRenderer == TSR_CLASSIC_FIXED) { RClassic_Fixed::setCamera(yaw, pitch, x, y, z, sectorId); }
		else { RClassic_Float::setCamera(yaw, pitch, x, y, z, sectorId); }

		s_sectorId = sectorId;
		s_cameraLightSource = 0;
		s_worldAmbient = worldAmbient;
		s_cameraLightSource = cameraLightSource ? -1 : 0;

		s_drawFrame++;
	}
		
	void draw(u8* display, const ColorMap* colormap)
	{
		// Clear the screen for now so we can get away with only drawing walls.
		//memset(display, 0, s_width * s_height);
		s_display = display;
		s_colorMap = colormap->colorMap;
		s_lightSourceRamp = colormap->lightSourceRamp;
		clear1dDepth();

		s_windowMinX = s_minScreenX;
		s_windowMaxX = s_maxScreenX;
		s_windowMinY = 1;
		s_windowMaxY = s_height - 1;
		s_windowMaxCeil  = s_minScreenY;
		s_windowMinFloor = s_maxScreenY;
		s_flatCount  = 0;
		s_nextWall   = 0;
		s_curWallSeg = 0;

		s_prevSector = nullptr;
		s_sectorIndex = 0;
		s_maxAdjoinIndex = 0;
		s_adjoinSegCount = 1;
		s_adjoinIndex = 0;
		
		s_adjoinDepth = 1;
		s_maxAdjoinDepth = 1;

		for (s32 i = 0; i < s_width; i++)
		{
			s_columnTop[i] = s_minScreenY;
			s_columnBot[i] = s_maxScreenY;
			s_windowTop_all[i] = s_minScreenY;
			s_windowBot_all[i] = s_maxScreenY;
		}

		// For now setup sector data each frame.
		updateSectors();

		// Recursively draws sectors and their contents (sprites, 3D objects).
		{
			TFE_ZONE("Sector Draw");
			RSector* sector = s_sectors->get() + s_sectorId;
			s_sectors->draw(sector);
		}
	}

	/////////////////////////////////////////////
	// Internal
	/////////////////////////////////////////////
	void clear1dDepth()
	{
		if (s_subRenderer == TSR_CLASSIC_FIXED)
		{
			memset(RClassic_Fixed::s_depth1d_all_Fixed, 0, s_width * sizeof(s32));
			RClassic_Fixed::s_windowMinZ_Fixed = 0;
		}
		else
		{
			memset(s_depth1d_all, 0, s_width * sizeof(f32));
			s_windowMinZ = 0.0f;
		}
	}

	// TODO: Do one time at load and then update directly from INF.
	void updateSectors()
	{
		const u32 count = s_sectors->getCount();
		for (u32 i = 0; i < count; i++)
		{
			s_sectors->update(i);
		}
	}

	void buildLevelData()
	{
		LevelData* level = TFE_LevelAsset::getLevelData();
		u32 count = (u32)level->sectors.size();
		s_sectors->allocate(count);

		RSector* sectors = s_sectors->get();
		memset(sectors, 0, sizeof(RSector) * level->sectors.size());
		Texture** textures = level->textures.data();
		for (u32 i = 0; i < count; i++)
		{
			Sector* sector = &level->sectors[i];
			SectorWall* walls = level->walls.data() + sector->wallOffset;
			Vec2f* vertices = level->vertices.data() + sector->vtxOffset;

			s_sectors->copy(&sectors[i], sector, walls, vertices, textures);
		}

		///////////////////////////////////////
		// Process sectors after load
		///////////////////////////////////////
		RSector* sector = s_sectors->get();
		for (u32 i = 0; i < count; i++, sector++)
		{
			RWall* wall = sector->walls;
			for (s32 w = 0; w < sector->wallCount; w++, wall++)
			{
				RSector* nextSector = wall->nextSector;
				if (nextSector)
				{
					RWall* mirror = &nextSector->walls[wall->mirror];
					wall->mirrorWall = mirror;
					// Both sides of a mirror should have the same lower flags3 (such as walkability).
					wall->flags3 |= (mirror->flags3 & 0x0f);
					mirror->flags3 |= (wall->flags3 & 0x0f);
				}
			}
			s_sectors->setupWallDrawFlags(sector);
			s_sectors->adjustHeights(sector, { 0 }, { 0 }, { 0 });
			s_sectors->computeBounds(sector);
		}
	}
}
