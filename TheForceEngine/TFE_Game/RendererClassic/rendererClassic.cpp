#include "rendererClassic.h"
#include "fixedPoint.h"
#include "rsector.h"
#include "rwall.h"
#include "rflat.h"
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
		CVAR_INT(s_maxWallCount, "d_maxWallCount", 0, "Maximum wall count for a given sector.");
	}

	void changeResolution(s32 width, s32 height)
	{
		if (width == s_width && height == s_height) { return; }

		s_width = width;
		s_height = height;
		s_halfWidth = intToFixed16(s_width >> 1);
		s_halfHeight = intToFixed16(s_height >> 1);
		s_focalLength = s_halfWidth;
		s_focalLenAspect = s_focalLength;
		s_screenXMid = s_width >> 1;

		// HACK: TODO - compute correctly.
		if (width * 10 / height != 16)
		{
			const fixed16 mul1_2 = 78643;
			s_focalLenAspect = mul16(s_focalLenAspect, mul1_2);
		}

		s_minScreenX = 0;
		s_maxScreenX = s_width - 1;
		s_minScreenY = 1;
		s_maxScreenY = s_height - 1;
		s_minSegZ = 0;

		s_depth1d   = (fixed16*)realloc(s_depth1d, s_width * sizeof(fixed16));
		s_columnTop = (s32*)realloc(s_columnTop, s_width * sizeof(s32));
		s_columnBot = (s32*)realloc(s_columnBot, s_width * sizeof(s32));
		s_windowTop = (s32*)realloc(s_windowTop, s_width * sizeof(s32));
		s_windowBot = (s32*)realloc(s_windowBot, s_width * sizeof(s32));

		// Build tables
		s_column_Y_Over_X = (fixed16*)realloc(s_column_Y_Over_X, s_width * sizeof(fixed16));
		s_column_X_Over_Y = (fixed16*)realloc(s_column_X_Over_Y, s_width * sizeof(fixed16));
		s32 halfWidth = s_width >> 1;
		for (s32 x = 0; x < s_width; x++)
		{
			s_column_Y_Over_X[x] = (x != halfWidth) ? div16(s_halfWidth, intToFixed16(x - halfWidth)) : s_halfWidth;
			s_column_X_Over_Y[x] = div16(intToFixed16(x - halfWidth), s_halfWidth);
		}

		s_rcp_yMinusHalfHeight = (fixed16*)realloc(s_rcp_yMinusHalfHeight, s_height * sizeof(fixed16));
		s32 halfHeight = s_height >> 1;
		for (s32 y = 0; y < s_height; y++)
		{
			s32 yMinusHalf = y - halfHeight;
			s_rcp_yMinusHalfHeight[y] = (yMinusHalf != 0) ? 65536 / yMinusHalf : 65536;
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

	void setCamera(fixed16 cosYaw, fixed16 sinYaw, s32 x, s32 y, s32 z, s32 sectorId)
	{
		s_cosYaw = cosYaw;
		s_sinYaw = sinYaw;
		s_negSinYaw = -sinYaw;

		s_zCameraTrans = mul16(-z, s_cosYaw) + mul16(-x, s_negSinYaw);
		s_xCameraTrans = mul16(-x, s_cosYaw) + mul16(-z, s_sinYaw);
		s_eyeHeight = y;

		s_cameraPosX = x;
		s_cameraPosZ = z;

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
		s_yMin = s_minScreenY;
		s_yMax = s_maxScreenY;
		s_flatCount  = 0;
		s_nextWall   = 0;
		s_curWallSeg = 0;
		memset(s_depth1d, 0, s_width * sizeof(fixed16));

		for (s32 i = 0; i < s_width; i++)
		{
			s_columnTop[i] = s_minScreenY;
			s_columnBot[i] = s_maxScreenY;
			s_windowTop[i] = s_minScreenY;
			s_windowBot[i] = s_maxScreenY;
		}
						
		// Draws a single sector.
		sector_setCurrent(sector_get() + s_sectorId);
		sector_draw();
	}
		
	void loadLevel()
	{
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
	}
}
