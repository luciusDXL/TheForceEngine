#include "jediRenderer.h"
#include <TFE_Level/fixedPoint.h>
#include <TFE_Level/robject.h>
#include "rcommon.h"
#include "rsectorRender.h"
#include "RClassic_Fixed/rcommonFixed.h"
#include "RClassic_Fixed/rclassicFixed.h"
#include "RClassic_Fixed/rsectorFixed.h"

//#include "RClassic_Float/rclassicFloat.h"
//#include "RClassic_Float/rsectorFloat.h"

#include <TFE_System/profiler.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_Asset/modelAsset_jedi.h>
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
	void console_setSubRenderer(const std::vector<std::string>& args);
	void console_getSubRenderer(const std::vector<std::string>& args);

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
		CVAR_INT(s_maxWallCount, "d_maxWallCount", CVFLAG_DO_NOT_SERIALIZE, "Maximum wall count for a given sector.");
		CVAR_INT(s_maxDepthCount, "d_maxDepthCount", CVFLAG_DO_NOT_SERIALIZE, "Maximum adjoin depth count.");

		CCMD("rsetSubRenderer", console_setSubRenderer, 1, "Set the sub-renderer - valid values are: Classic_Fixed, Classic_Float, Classic_GPU");
		CCMD("rgetSubRenderer", console_getSubRenderer, 0, "Get the current sub-renderer.");

		// Setup performance counters.
		TFE_COUNTER(s_maxAdjoinDepth, "Maximum Adjoin Depth");
		TFE_COUNTER(s_maxAdjoinIndex, "Maximum Adjoin Count");
		TFE_COUNTER(s_sectorIndex, "Sector Count");
		TFE_COUNTER(s_flatCount, "Flat Count");
		TFE_COUNTER(s_curWallSeg, "Wall Segment Count");
		TFE_COUNTER(s_adjoinSegCount, "Adjoin Segment Count");
	}

	void destroy()
	{
		delete s_sectors;
	}

	void setResolution(s32 width, s32 height)
	{
		if (s_subRenderer == TSR_CLASSIC_FIXED) { RClassic_Fixed::setResolution(width, height); }
		//else { RClassic_Float::setResolution(width, height); }
	}

	void setupLevel(s32 width, s32 height)
	{
		init();
		setResolution(width, height);

		s_memPool.init(32 * 1024 * 1024, "Classic Renderer - Software");
		s_sectorId = -1;
		s_sectors->setMemoryPool(&s_memPool);
	}

	void console_setSubRenderer(const std::vector<std::string>& args)
	{
		if (args.size() < 2) { return; }
		const char* value = args[1].c_str();

		s32 width = s_width, height = s_height;
		if (strcasecmp(value, "Classic_Fixed") == 0)
		{
			setSubRenderer(TSR_CLASSIC_FIXED);
			setupLevel(width, height);
		}
		else if (strcasecmp(value, "Classic_Float") == 0)
		{
			setSubRenderer(TSR_CLASSIC_FLOAT);
			setupLevel(width, height);
		}
		else if (strcasecmp(value, "Classic_GPU") == 0)
		{
			setSubRenderer(TSR_CLASSIC_GPU);
			setupLevel(width, height);
		}
	}

	void console_getSubRenderer(const std::vector<std::string>& args)
	{
		const char* c_subRenderers[] =
		{
			"Classic_Fixed",	// TSR_CLASSIC_FIXED
			"Classic_Float",	// TSR_CLASSIC_FLOAT
			"Classic_GPU",		// TSR_CLASSIC_GPU
		};
		TFE_Console::addToHistory(c_subRenderers[s_subRenderer]);
	}

	void setSubRenderer(TFE_SubRenderer subRenderer/* = TSR_CLASSIC_FIXED*/)
	{
		// HACK:
		// Force sub-renderer to fixed for now until the refactoring is complete.
		subRenderer = TSR_CLASSIC_FIXED;

		if (subRenderer != s_subRenderer)
		{
			s_subRenderer = subRenderer;
			// Reset the resolution so it is set properly.
			s_width = 0;
			s_height = 0;

			// Setup the sub-renderer sector system.
			TFE_Sectors* prev = s_sectors;

			if (s_subRenderer == TSR_CLASSIC_FIXED)
			{
				s_sectors = new TFE_Sectors_Fixed();
			}
			/*else
			{
				s_sectors = new TFE_Sectors_Float();
			}*/
			s_sectors->subrendererChanged();
			delete prev;
		}
	}

	void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId, s32 worldAmbient, bool cameraLightSource)
	{
		if (s_subRenderer == TSR_CLASSIC_FIXED) { RClassic_Fixed::setCamera(yaw, pitch, x, y, z, sectorId); }
		// else { RClassic_Float::setCamera(yaw, pitch, x, y, z, sectorId); }

		s_sectorId = sectorId;
		s_cameraLightSource = 0;
		s_worldAmbient = worldAmbient;
		s_cameraLightSource = cameraLightSource ? -1 : 0;

		s_drawFrame++;
	}

	void draw(u8* display, const ColorMap* colormap)
	{
		// Clear the top pixel row.
		memset(display, 0, s_width);

		s_display = display;
		s_colorMap = colormap->colorMap;
		s_lightSourceRamp = colormap->lightSourceRamp;
		clear1dDepth();

		s_windowMinX = s_minScreenX;
		s_windowMaxX = s_maxScreenX;
		s_windowMinY = 1;
		s_windowMaxY = s_height - 1;
		s_windowMaxCeil = s_minScreenY;
		s_windowMinFloor = s_maxScreenY;
		s_flatCount = 0;
		s_nextWall = 0;
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
}
