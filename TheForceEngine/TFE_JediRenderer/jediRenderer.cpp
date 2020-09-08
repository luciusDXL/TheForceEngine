#include "jediRenderer.h"
#include "fixedPoint.h"
#include "rcommon.h"
#include "rsector.h"
#include "RClassic_Fixed/rcommonFixed.h"
#include "RClassic_Fixed/rclassicFixed.h"
#include "RClassic_Fixed/rsectorFixed.h"

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
	static TFE_SubRenderer s_subRenderer = TSR_CLASSIC_FIXED;
	static TFE_Sectors* s_sectors = nullptr;

	/////////////////////////////////////////////
	// Forward Declarations
	/////////////////////////////////////////////
	void clear1dDepth();
	void updateSectors();
	void buildLevelData();
	void setResolution_Float(s32 width, s32 height);
	void setCamera_Float(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId);

	/////////////////////////////////////////////
	// Implementation
	/////////////////////////////////////////////
	void init()
	{
		if (s_init) { return; }
		s_init = true;
		s_maxWallCount = 0xffff;
		s_maxDepthCount = 0xffff;
		CVAR_INT(s_maxWallCount, "d_maxWallCount", 0, "Maximum wall count for a given sector.");
		CVAR_INT(s_maxDepthCount, "d_maxDepthCount", 0, "Maximum adjoin depth count.");

		TFE_COUNTER(s_maxAdjoinDepth, "Maximum Adjoin Depth");
		TFE_COUNTER(s_maxAdjoinIndex, "Maximum Adjoin Count");
		TFE_COUNTER(s_sectorIndex,    "Sector Count");
		TFE_COUNTER(s_flatCount,      "Flat Count");
		TFE_COUNTER(s_curWallSeg,     "Wall Segment Count");
		TFE_COUNTER(s_adjoinSegCount, "Adjoin Segment Count");

		if (s_subRenderer == TSR_CLASSIC_FIXED)
		{
			s_sectors = new TFE_Sectors_Fixed();
		}
		else
		{
			//s_sectors = new TFE_Sectors_Float();
		}
	}

	void destroy()
	{
		delete s_sectors;
	}

	void setResolution(s32 width, s32 height)
	{
		if (s_subRenderer == TSR_CLASSIC_FIXED) { RClassic_Fixed::setResolution(width, height); }
		else { setResolution_Float(width, height); }
	}

	void setupLevel(s32 width, s32 height)
	{
		init();
		if (s_subRenderer == TSR_CLASSIC_FIXED) { RClassic_Fixed::setResolution(width, height); }
		else { setResolution_Float(width, height); }
		
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
			// TODO: Apply any required changes, such as re-processing level data.

			delete s_sectors;
			if (s_subRenderer == TSR_CLASSIC_FIXED)
			{
				s_sectors = new TFE_Sectors_Fixed();
			}
			else
			{
				//s_sectors = new TFE_Sectors_Float();
			}
		}
	}
		
	void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId, s32 worldAmbient, bool cameraLightSource)
	{
		if (s_subRenderer == TSR_CLASSIC_FIXED) { RClassic_Fixed::setCamera(yaw, pitch, x, y, z, sectorId); }
		else { setCamera_Float(yaw, pitch, x, y, z, sectorId); }

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
			s_sectors->adjustHeights(sector, 0, 0, 0);
			s_sectors->computeBounds(sector);
		}
	}

	void setCamera_Float(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId)
	{
		s_cosYaw = cosf(yaw);
		s_sinYaw = sinf(yaw);
		s_negSinYaw = -s_sinYaw;
		s_cameraYaw = yaw;
		s_cameraPitch = pitch;

		s_zCameraTrans = -z * s_cosYaw - x * s_negSinYaw;
		s_xCameraTrans = -x * s_cosYaw - z * s_sinYaw;
		s_eyeHeight = y;

		s_cameraPosX = x;
		s_cameraPosZ = z;

		const f32 pitchOffset = sinf(pitch) * s_focalLenAspect;
		s_halfHeight = s_halfHeightBase + pitchOffset;
		s_heightInPixels = s_heightInPixelsBase + (s32)floorf(pitchOffset);

		const LevelData* level = TFE_LevelAsset::getLevelData();
		s_skyYawOffset = s_cameraYaw / (2.0f * PI) * level->parallax[0];
		s_skyPitchOffset = -s_cameraPitch / (2.0f * PI) * level->parallax[1];
	}

	void setResolution_Float(s32 width, s32 height)
	{
		if (width == s_width && height == s_height) { return; }

		s_width = width;
		s_height = height;
		s_halfWidth = f32(s_width >> 1);
		s_halfHeight = f32(s_height >> 1);
		s_halfHeightBase = s_halfHeight;
		s_focalLength = s_halfWidth;
		s_focalLenAspect = s_focalLength;
		s_screenXMid = s_width >> 1;

		s_heightInPixels = s_height;
		s_heightInPixelsBase = s_height;

		// HACK: TODO - compute correctly.
		if (width * 10 / height != 16)
		{
			s_focalLenAspect = (height / 2.0f) * 8.0f / 5.0f;
		}

		s_minScreenX = 0;
		s_maxScreenX = s_width - 1;
		s_minScreenY = 1;
		s_maxScreenY = s_height - 1;
		s_windowMinZ = 0;

		s_windowX0 = s_minScreenX;
		s_windowX1 = s_maxScreenX;

		s_columnTop = (s32*)realloc(s_columnTop, s_width * sizeof(s32));
		s_columnBot = (s32*)realloc(s_columnBot, s_width * sizeof(s32));
		s_depth1d_all = (f32*)realloc(s_depth1d_all, s_width * sizeof(f32) * (MAX_ADJOIN_DEPTH + 1));
		s_windowTop_all = (s32*)realloc(s_windowTop, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));
		s_windowBot_all = (s32*)realloc(s_windowBot, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));

		// Build tables
		s_column_Y_Over_X = (f32*)realloc(s_column_Y_Over_X, s_width * sizeof(f32));
		s_column_X_Over_Y = (f32*)realloc(s_column_X_Over_Y, s_width * sizeof(f32));
		s_skyTable = (f32*)realloc(s_skyTable, s_width * sizeof(f32));
		s32 halfWidth = s_width >> 1;
		for (s32 x = 0; x < s_width; x++)
		{
			s_column_Y_Over_X[x] = (x != halfWidth) ? s_halfWidth / f32(x - halfWidth) : s_halfWidth;
			s_column_X_Over_Y[x] = f32(x - halfWidth) / s_halfWidth;

			// This result isn't *exactly* the same as the original, but it is accurate to within 1 decimal point (example: -88.821585 vs -88.8125 @ x=63)
			// The original result is likely from a arc tangent table, which is more approximate. However the end difference is less
			// than a single pixel at 320x200. The more accurate result will look better at higher resolutions as well. :)
			// TODO: Extract the original table to use at 320x200?
			s_skyTable[x] = 512.0f * atanf(f32(x - halfWidth) / f32(halfWidth)) / PI;
		}

		s_rcp_yMinusHalfHeight = (f32*)realloc(s_rcp_yMinusHalfHeight, 3 * s_height * sizeof(f32));
		s32 halfHeight = s_height >> 1;
		for (s32 y = 0; y < s_height * 3; y++)
		{
			f32 yMinusHalf = f32(-s_height + y - halfHeight);
			s_rcp_yMinusHalfHeight[y] = (yMinusHalf != 0) ? 1.0f / yMinusHalf : 1.0f;
		}
	}
}
