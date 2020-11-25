#include "jediRenderer.h"
#include "fixedPoint.h"
#include "rcommon.h"
#include "rsector.h"
#include "robject.h"
#include "RClassic_Fixed/rcommonFixed.h"
#include "RClassic_Fixed/rclassicFixed.h"
#include "RClassic_Fixed/rsectorFixed.h"

#include "RClassic_Float/rclassicFloat.h"
#include "RClassic_Float/rsectorFloat.h"

#include <TFE_System/profiler.h>
// VVV - this needs to be moved or fixed.
#include <TFE_Asset/levelObjectsAsset.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
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
		CVAR_INT(s_maxWallCount,  "d_maxWallCount",  0, "Maximum wall count for a given sector.");
		CVAR_INT(s_maxDepthCount, "d_maxDepthCount", 0, "Maximum adjoin depth count.");

		CCMD("rsetSubRenderer", console_setSubRenderer, 1, "Set the sub-renderer - valid values are: Classic_Fixed, Classic_Float, Classic_GPU");
		CCMD("rgetSubRenderer", console_getSubRenderer, 0, "Get the current sub-renderer.");

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
		const char* c_subRenderers[]=
		{
			"Classic_Fixed",	// TSR_CLASSIC_FIXED
			"Classic_Float",	// TSR_CLASSIC_FLOAT
			"Classic_GPU",		// TSR_CLASSIC_GPU
		};
		TFE_Console::addToHistory(c_subRenderers[s_subRenderer]);
	}

	void setSubRenderer(TFE_SubRenderer subRenderer/* = TSR_CLASSIC_FIXED*/)
	{
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

	SecObject* allocateObject()
	{
		SecObject* obj = (SecObject*)malloc(sizeof(SecObject));
		obj->yaw = 0;
		obj->pitch = 0;
		obj->roll = 0;
		obj->frame = 0;
		obj->anim = 0;
		obj->worldWidth = -1;
		obj->ptr = nullptr;
		obj->sector = nullptr;
		obj->type = 0;
		obj->typeFlags = 0; // OTFLAG_NONE;
		obj->worldHeight = -1;
		obj->flags = 0;
		obj->self = obj;
		return obj;
	}

	void frame_setData(SecObject* obj, u8* basePtr, WaxFrame* data)
	{
		obj->type = OBJ_TYPE_FRAME;
		obj->fme = data;
		WaxCell* cell = WAX_CellPtr(basePtr, data);

		if (obj->worldWidth == -1)
		{
			s32 width = abs(cell->sizeX) << 16;
			obj->worldWidth = div16(width, SPRITE_SCALE_FIXED);
		}
		if (obj->worldHeight == -1)
		{
			s32 height = abs(cell->sizeY) << 16;
			obj->worldHeight = div16(height, SPRITE_SCALE_FIXED);
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

		///////////////////////////////////////
		// Objects
		///////////////////////////////////////
		const LevelObjectData* levelObj = TFE_LevelObjects::getLevelObjectData();
		std::vector<JediFrame*> frames;
		const u32 frameCount = (u32)levelObj->frames.size();
		frames.resize(frameCount);
		for (u32 i = 0; i < frameCount; i++)
		{
			frames[i] = TFE_Sprite_Jedi::getFrame(levelObj->frames[i].c_str());
		}
		
		std::vector<JediWax*> waxes;
		const u32 waxCount = (u32)levelObj->sprites.size();
		waxes.resize(waxCount);
		for (u32 i = 0; i < waxCount; i++)
		{
			waxes[i] = TFE_Sprite_Jedi::getWax(levelObj->sprites[i].c_str());
		}

		const LevelObject* srcObj = levelObj->objects.data();
		const u32 objCount = (u32)levelObj->objects.size();
		for (u32 i = 0; i < objCount; i++, srcObj++)
		{
			// for now only worry about frames.
			if (srcObj->oclass == CLASS_FRAME)
			{
				SecObject* obj = allocateObject();

				if (s_subRenderer == TSR_CLASSIC_FIXED)
				{
					obj->posWS.x.f16_16 = s32(srcObj->pos.x * 65536.0f);
					obj->posWS.y.f16_16 = s32(srcObj->pos.y * 65536.0f);
					obj->posWS.z.f16_16 = s32(srcObj->pos.z * 65536.0f);
				}
				else
				{
					obj->posWS.x.f32 = srcObj->pos.x;
					obj->posWS.y.f32 = srcObj->pos.y;
					obj->posWS.z.f32 = srcObj->pos.z;
				}
				obj->pitch = s16(srcObj->orientation.x / 360.0f * 16484.0f);
				obj->yaw = s16(srcObj->orientation.y / 360.0f * 16484.0f);
				obj->roll = s16(srcObj->orientation.z / 360.0f * 16484.0f);

				obj->fme = frames[srcObj->dataOffset]->frame;

				RSector* sector = s_sectors->which3D(obj->posWS.x, obj->posWS.y, obj->posWS.z);
				if (!sector)
				{
					continue;
				}

				s_sectors->addObject(sector, obj);
				frame_setData(obj, (u8*)obj->fme, obj->fme);
			}
		}
	}
}
