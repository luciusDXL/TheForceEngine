//////////////////////////////////////////////////////////////////////////
// Renderer Classic TODO
// {6/23-6/28}
// -- Finish Adjoins --
// 1. [X] wall_drawTop() - finally handle this correctly, including adjoins.
// 2. [X] wall_drawTransparent() - handle correctly.
// 3. [X] fix any adjoin bugs that don't exist in the original.
// -- Finish Sector Rendering --
// 4. [X] Draw sky (exterior/pit/exterior adjoin/pit adjoin/etc.).
// 5. [X]Draw signs.
// {6/29-7/05}
// 6. [X] Texture animation.
// 7. [X] Find code that handles texture offset calculations and fixup (handle
//    where sectors move due to INF and fix up texture offsets based on
//    flags).
// -- Sprite drawing --
// 8. [X] Object sorting.
// 9. [X] Sprite drawing code.
// 10.[~] Loading code / object-sector assignment.
// {7/06-7/12}
// -- 3D object drawing --
// 11.[~] Loading code / object-sector assignment.
// 12. Model rendering.
// 13. Perspective correct texturing support (optional feature).
// -- Verify correct rendering using vanilla and test levels --
// {7/13-7/19}
// -- Refactor renderer - replace with Classic.
// -- GPU Rendering.
// {7/20-7/26}
// -- Cleanup.
// -- Finish UI setup.
// -- Finish rendering features.
// {7/27}
// ** Release **
//////////////////////////////////////////////////////////////////////////
#include "rendererClassic.h"
#include "fixedPoint.h"
#include "rsector.h"
#include "rwall.h"
#include "rflat.h"
#include "rcommon.h"

#include <TFE_Renderer/renderer.h>
#include <TFE_System/system.h>
#include <TFE_System/math.h>
#include <TFE_System/profiler.h>
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
using namespace RClassicFlat;
using namespace RClassicSector;

namespace RendererClassic
{
	s32 s_sectorId = -1;

	static bool s_init = false;
	static MemoryPool s_memPool;
				
	void loadLevel();

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
	}

	void changeResolution(s32 width, s32 height)
	{
		if (width == s_width && height == s_height) { return; }

		s_width = width;
		s_height = height;
		s_halfWidth = intToFixed16(s_width >> 1);
		s_halfHeight = intToFixed16(s_height >> 1);
		s_halfHeightBase = s_halfHeight;
		s_focalLength = s_halfWidth;
		s_focalLenAspect = s_focalLength;
		s_screenXMid = s_width >> 1;

		s_heightInPixels = s_height;
		s_heightInPixelsBase = s_height;

		// Use improved fixed point math if the resolution is any higher than 320x200.
		s_enableHighPrecision = (width > 320 || height > 200);

		// HACK: TODO - compute correctly.
		if (width * 10 / height != 16)
		{
			s_focalLenAspect = intToFixed16((height / 2) * 8 / 5);
		}

		s_minScreenX = 0;
		s_maxScreenX = s_width - 1;
		s_minScreenY = 1;
		s_maxScreenY = s_height - 1;
		s_windowMinZ = 0;

		s_windowX0 = s_minScreenX;
		s_windowX1 = s_maxScreenX;

		s_columnTop   = (s32*)realloc(s_columnTop, s_width * sizeof(s32));
		s_columnBot   = (s32*)realloc(s_columnBot, s_width * sizeof(s32));
		s_depth1d_all = (fixed16_16*)realloc(s_depth1d, s_width * sizeof(fixed16_16) * (MAX_ADJOIN_DEPTH + 1));
		s_windowTop_all = (s32*)realloc(s_windowTop, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));
		s_windowBot_all = (s32*)realloc(s_windowBot, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));

		// Build tables
		s_column_Y_Over_X = (fixed16_16*)realloc(s_column_Y_Over_X, s_width * sizeof(fixed16_16));
		s_column_X_Over_Y = (fixed16_16*)realloc(s_column_X_Over_Y, s_width * sizeof(fixed16_16));
		s_skyTable = (fixed16_16*)realloc(s_skyTable, s_width * sizeof(fixed16_16));
		s32 halfWidth = s_width >> 1;
		for (s32 x = 0; x < s_width; x++)
		{
			s_column_Y_Over_X[x] = (x != halfWidth) ? div16(s_halfWidth, intToFixed16(x - halfWidth)) : s_halfWidth;
			s_column_X_Over_Y[x] = div16(intToFixed16(x - halfWidth), s_halfWidth);

			// This result isn't *exactly* the same as the original, but it is accurate to within 1 decimal point (example: -88.821585 vs -88.8125 @ x=63)
			// The original result is likely from a arc tangent table, which is more approximate. However the end difference is less
			// than a single pixel at 320x200. The more accurate result will look better at higher resolutions as well. :)
			// TODO: Extract the original table to use at 320x200?
			s_skyTable[x] = floatToFixed16(512.0f * atanf(f32(x - halfWidth) / f32(halfWidth)) / PI);
		}

		s_rcp_yMinusHalfHeight = (fixed16_16*)realloc(s_rcp_yMinusHalfHeight, 3 * s_height * sizeof(fixed16_16));
		s32 halfHeight = s_height >> 1;
		for (s32 y = 0; y < s_height * 3; y++)
		{
			fixed16_16 yMinusHalf = fixed16_16(-s_height + y - halfHeight);
			s_rcp_yMinusHalfHeight[y] = (yMinusHalf != 0) ? ONE_16 / yMinusHalf : ONE_16;
		}
	}
	
	void setupLevel(s32 width, s32 height)
	{
		init();
		changeResolution(width, height);

		s_memPool.init(32 * 1024 * 1024, "Classic Renderer - Software");
		s_sectorId = -1;
		sector_setMemoryPool(&s_memPool);

		loadLevel();
	}
			
	void setCamera(fixed16_16 yaw, fixed16_16 pitch, fixed16_16 cosYaw, fixed16_16 sinYaw, fixed16_16 sinPitch, fixed16_16 x, fixed16_16 y, fixed16_16 z, s32 sectorId)
	{
		s_cosYaw = cosYaw;
		s_sinYaw = sinYaw;
		s_negSinYaw = -sinYaw;
		s_cameraYaw = yaw;
		s_cameraPitch = pitch;

		s_zCameraTrans = mul16(-z, s_cosYaw) + mul16(-x, s_negSinYaw);
		s_xCameraTrans = mul16(-x, s_cosYaw) + mul16(-z, s_sinYaw);
		s_eyeHeight = y;

		s_cameraPosX = x;
		s_cameraPosZ = z;

		fixed16_16 pitchOffset = mul16(sinPitch, s_focalLenAspect);
		s_halfHeight = s_halfHeightBase + pitchOffset;
		s_heightInPixels = s_heightInPixelsBase + floor16(pitchOffset);

		s_sectorId = sectorId;
		s_cameraLightSource = 0;
		s_worldAmbient = MAX_LIGHT_LEVEL;
		const LightMode mode = TFE_RenderCommon::getLightMode();
		if (mode != LIGHT_OFF)
		{
			s_worldAmbient = (mode == LIGHT_NORMAL) ? 0 : -9;
			s_cameraLightSource = -1;
		}

		s_drawFrame++;
	}
		
	void computeSkyOffsets()
	{
		const LevelData* level = TFE_LevelAsset::getLevelData();

		// angles range from -16384 to 16383; multiply by 4 to convert to [-1, 1) range.
		s_skyYawOffset   =  mul16(s_cameraYaw   * ANGLE_TO_FIXED_SCALE, intToFixed16((s32)level->parallax[0]));
		s_skyPitchOffset = -mul16(s_cameraPitch * ANGLE_TO_FIXED_SCALE, intToFixed16((s32)level->parallax[1]));
	}

	void draw(u8* display, const ColorMap* colormap)
	{
		// Clear the screen for now so we can get away with only drawing walls.
		//memset(display, 0, s_width * s_height);
		s_display = display;
		s_colorMap = colormap->colorMap;
		s_lightSourceRamp = colormap->lightSourceRamp;

		s_windowMinX = s_minScreenX;
		s_windowMaxX = s_maxScreenX;
		s_windowMinY = 1;
		s_windowMaxY = s_height - 1;
		s_windowMaxCeil  = s_minScreenY;
		s_windowMinFloor = s_maxScreenY;
		s_flatCount  = 0;
		s_nextWall   = 0;
		s_curWallSeg = 0;
		memset(s_depth1d_all, 0, s_width * sizeof(fixed16_16));

		s_prevSector = nullptr;
		s_sectorIndex = 0;
		s_maxAdjoinIndex = 0;
		s_adjoinSegCount = 1;
		s_adjoinIndex = 0;
		s_windowMinZ = 0;

		s_adjoinDepth = 1;
		s_maxAdjoinDepth = 1;

		computeSkyOffsets();

		for (s32 i = 0; i < s_width; i++)
		{
			s_columnTop[i] = s_minScreenY;
			s_columnBot[i] = s_maxScreenY;
			s_windowTop_all[i] = s_minScreenY;
			s_windowBot_all[i] = s_maxScreenY;
		}
						
		// Recursively draws sectors and their contents (sprites, 3D objects).
		{
			TFE_ZONE("Sector Draw");
			sector_draw(sector_get() + s_sectorId);
		}
	}

	void loadLevel()
	{
		// Insert this zone here to make sure it gets added first...
		TFE_ZONE("Draw");

		LevelData* level = TFE_LevelAsset::getLevelData();
		u32 count = (u32)level->sectors.size();
		sector_allocate(count);
		RSector* sectors = sector_get();
		memset(sectors, 0, sizeof(RSector) * level->sectors.size());
		Texture** textures = level->textures.data();
		for (u32 i = 0; i < count; i++)
		{
			Sector* sector = &level->sectors[i];
			SectorWall* walls = level->walls.data() + sector->wallOffset;
			Vec2f* vertices = level->vertices.data() + sector->vtxOffset;

			sector_copy(&sectors[i], sector, walls, vertices, textures);
		}

		///////////////////////////////////////
		// Process sectors after load
		///////////////////////////////////////
		RSector* sector = sector_get();
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
			sector_setupWallDrawFlags(sector);
			sector_adjustHeights(sector, 0, 0, 0);
			sector_computeBounds(sector);
		}
	}

	void updateSector(s32 id)
	{
		sector_update(id);
	}
}
