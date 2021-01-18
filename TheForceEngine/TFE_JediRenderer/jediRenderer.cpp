#include "jediRenderer.h"
#include "fixedPoint.h"
#include "rcommon.h"
#include "rsector.h"
#include "robject.h"
#include "RClassic_Float/robjVoxel_float/robjVoxelFloat.h"
#include "RClassic_Fixed/rcommonFixed.h"
#include "RClassic_Fixed/rclassicFixed.h"
#include "RClassic_Fixed/rsectorFixed.h"

#include "RClassic_Float/rclassicFloat.h"
#include "RClassic_Float/rsectorFloat.h"

#include <TFE_System/profiler.h>
// VVV - this needs to be moved or fixed.
#include <TFE_Game/gameObject.h>
#include <TFE_Asset/levelObjectsAsset.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Asset/voxelAsset.h>
#include <TFE_Game/level.h>
#include <TFE_FrontEndUI/console.h>
#include <TFE_System/memoryPool.h>
#include <TFE_FileSystem/paths.h>
// Experimental only.
#include <TFE_FileSystem/filestream.h>

namespace TFE_JediRenderer
{
	static s32 s_sectorId = -1;

	static bool s_init = false;
	static MemoryPool s_memPool;
	static TFE_SubRenderer s_subRenderer = TSR_INVALID;
	static TFE_Sectors* s_sectors = nullptr;
	static Vec3f s_cameraPos = { 0 };

	/////////////////////////////////////////////
	// Forward Declarations
	/////////////////////////////////////////////
	void clear1dDepth();
	void updateSectors();
	void updateGameObjects();
	void buildLevelData();
	void console_setSubRenderer(const std::vector<std::string>& args);
	void console_getSubRenderer(const std::vector<std::string>& args);
	void console_addVoxel(const std::vector<std::string>& args);
	void console_saveVoxels(const std::vector<std::string>& args);
	void console_loadVoxels(const std::vector<std::string>& args);
	void console_clearVoxels(const std::vector<std::string>& args);

	SecObject* allocateObject();
	void obj3d_computeTransform_Float(SecObject* obj, f32 yaw, f32 pitch, f32 roll);

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
		s_frame = 0;
		CVAR_INT(s_maxWallCount, "d_maxWallCount", CVFLAG_DO_NOT_SERIALIZE, "Maximum wall count for a given sector.");
		CVAR_INT(s_maxDepthCount, "d_maxDepthCount", CVFLAG_DO_NOT_SERIALIZE, "Maximum adjoin depth count.");

		CCMD("rsetSubRenderer", console_setSubRenderer, 1, "Set the sub-renderer - valid values are: Classic_Fixed, Classic_Float, Classic_GPU");
		CCMD("rgetSubRenderer", console_getSubRenderer, 0, "Get the current sub-renderer.");
		CCMD("raddVoxel", console_addVoxel, 1, "Add a voxel model to the current player position - raddVoxel \"voxelFile\" [base](floor or ceil) [offset] [up](y or z)");
		CCMD("rsaveVoxels", console_saveVoxels, 1, "Saves voxels currently setup in the level.");
		CCMD("rloadVoxels", console_loadVoxels, 1, "Loads voxel placements from disk.");
		CCMD("rclearVoxels", console_clearVoxels, 0, "Remove voxels.");

		// Setup performance counters.
		TFE_COUNTER(s_maxAdjoinDepth, "Maximum Adjoin Depth");
		TFE_COUNTER(s_maxAdjoinIndex, "Maximum Adjoin Count");
		TFE_COUNTER(s_sectorIndex, "Sector Count");
		TFE_COUNTER(s_flatCount, "Flat Count");
		TFE_COUNTER(s_curWallSeg, "Wall Segment Count");
		TFE_COUNTER(s_adjoinSegCount, "Adjoin Segment Count");

		RClassic_Float::robjVoxel_init();
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

	void setupLevel(s32 width, s32 height, bool enableExperiment)
	{
		init();
		setResolution(width, height);
				
		s_memPool.init(32 * 1024 * 1024, "Classic Renderer - Software");
		s_sectorId = -1;
		s_sectors->setMemoryPool(&s_memPool);

		buildLevelData();

		if (enableExperiment)
		{
			StringList argList;
			argList.push_back("rloadVoxels");
			argList.push_back("secbase.vob");
			console_loadVoxels(argList);
		}
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

	bool isNumeric(char ch)
	{
		if (ch == '.' || ch == '-') { return true; }
		if (ch >= '0' && ch <= '9') { return true; }
		return false;
	}

	struct VoxelObject
	{
		std::string file;
		u8 offsetFromFloor;
		u8 yUp;
		Vec2f pos;
		f32 offset;
		s32 sectorId;
		// unused - put here for forward compatibility.
		f32 scale;
		f32 angles[3];
	};
	std::vector<VoxelObject> s_voxelObjects;

	void console_saveVoxels(const std::vector<std::string>& args)
	{
		const u32 count = (u32)s_voxelObjects.size();

		if (args.size() < 2 || count < 1) { return; }
		const char* voxelFile = args[1].c_str();

		char localPath[TFE_MAX_PATH];
		char filePath[TFE_MAX_PATH];
		sprintf(localPath, "Mods/%s", voxelFile);
		TFE_Paths::appendPath(PATH_PROGRAM, localPath, filePath);

		FileStream file;
		if (file.open(filePath, FileStream::MODE_WRITE))
		{
			const VoxelObject* objects = s_voxelObjects.data();
			file.write(&count);
			for (u32 i = 0; i < count; i++)
			{
				u32 fileLen = (u32)objects[i].file.length();
				file.write(&fileLen);
				file.writeBuffer(objects[i].file.c_str(), fileLen);

				file.write(&objects[i].offsetFromFloor);
				file.write(&objects[i].yUp);
				file.write(objects[i].pos.m, 2);
				file.write(&objects[i].offset);
				file.write(&objects[i].sectorId);
				file.write(&objects[i].scale);
				file.write(objects[i].angles, 3);
			}
			file.close();
		}
	}

	void console_clearVoxels(const std::vector<std::string>& args)
	{
		s_voxelObjects.clear();

		s_memPool.init(32 * 1024 * 1024, "Classic Renderer - Software");
		s_sectorId = -1;
		s_sectors->setMemoryPool(&s_memPool);

		buildLevelData();
	}

	void console_loadVoxels(const std::vector<std::string>& args)
	{
		if (args.size() < 2)
		{
			TFE_System::logWrite(LOG_ERROR, "VoxelExperiment", "Load voxel patch - no file specified.");
			return;
		}
		if (!s_voxelObjects.empty())
		{
			TFE_System::logWrite(LOG_MSG, "VoxelExperiment", "Clearing the existing voxels.");

			// Clear the level first.
			s_voxelObjects.clear();
			
			s_memPool.init(32 * 1024 * 1024, "Classic Renderer - Software");
			s_sectorId = -1;
			s_sectors->setMemoryPool(&s_memPool);

			buildLevelData();
		}

		const char* voxelFile = args[1].c_str();

		char localPath[TFE_MAX_PATH];
		char filePath[TFE_MAX_PATH];
		sprintf(localPath, "Mods/%s", voxelFile);
		TFE_Paths::appendPath(PATH_PROGRAM, localPath, filePath);

		FileStream file;
		if (!file.open(filePath, FileStream::MODE_READ))
		{
			TFE_System::logWrite(LOG_ERROR, "VoxelExperiment", "Cannot load voxel level patch - file: \"%s\", path: \"%s\"", voxelFile, filePath);
		}
		else
		{
			u32 count = 0;
			file.read(&count);
			s_voxelObjects.resize(count);
			VoxelObject* objects = s_voxelObjects.data();

			// Load the object data.
			for (u32 i = 0; i < count; i++)
			{
				u32 fileLen;
				char voxelFile[TFE_MAX_PATH];
				file.read(&fileLen);
				file.readBuffer(voxelFile, fileLen);
				voxelFile[fileLen] = 0;
				objects[i].file = voxelFile;

				file.read(&objects[i].offsetFromFloor);
				file.read(&objects[i].yUp);
				file.read(objects[i].pos.m, 2);
				file.read(&objects[i].offset);
				file.read(&objects[i].sectorId);
				file.read(&objects[i].scale);
				file.read(objects[i].angles, 3);
			}
			file.close();

			// Then add the objects themselves.
			for (u32 i = 0; i < count; i++)
			{
				VoxelModel* voxel = TFE_VoxelModel::get(objects[i].file.c_str(), objects[i].yUp);
				if (!voxel)
				{
					char tmp[TFE_MAX_PATH];
					sprintf(tmp, "Cannot find or load \"%s\"", objects[i].file.c_str());
					TFE_Console::addToHistory(tmp);
					continue;
				}

				if (objects[i].sectorId < 0 || objects[i].sectorId >= s_sectors->getCount())
				{
					continue;
				}

				RSector* sector = &s_sectors->get()[objects[i].sectorId];

				// TODO: This will leak memory, fix when this comes out of experimental.
				SecObject* obj = allocateObject();
				obj->self = obj;
				obj->type = OBJ_TYPE_VOXEL;
				obj->typeFlags = 0;

				obj->posWS.x.f32 = objects[i].pos.x;
				obj->posWS.z.f32 = objects[i].pos.z;
				obj->posWS.y.f32 = objects[i].offsetFromFloor ? (sector->floorHeight.f32 + objects[i].offset) : (sector->ceilingHeight.f32 + objects[i].offset + f32(voxel->height)*0.1f);
				obj3d_computeTransform_Float(obj, 0.0f, 0.0f, 0.0f);

				obj->voxel = voxel;

				obj->frame = 0;
				obj->anim = 0;
				obj->sector = sector;
				obj->flags = OBJ_FLAG_RENDERABLE;

				obj->yaw = 0;
				obj->pitch = 0;
				obj->roll = 0;

				obj->gameObjId = 0xffffffff;
				s_sectors->addObject(sector, obj);
			}
		}
	}

	void console_addVoxel(const std::vector<std::string>& args)
	{
		if (args.size() < 2) { return; }
		const char* voxelFile = args[1].c_str();

		bool offsetFromFloor = true;
		bool yUp = true;
		f32 offset = 0.0f;
		char* endPtr = nullptr;
		for (size_t i = 2; i < args.size(); i++)
		{
			if (args[i] == "ceil")
			{
				offsetFromFloor = false;
			}
			else if (args[i] == "z")
			{
				yUp = false;
			}
			else if (isNumeric(args[i].c_str()[0]))
			{
				offset = strtof(args[i].c_str(), &endPtr);
			}
		}

		VoxelModel* voxel = TFE_VoxelModel::get(voxelFile, yUp);
		if (!voxel)
		{
			char tmp[TFE_MAX_PATH];
			sprintf(tmp, "Cannot find or load \"%s\"", voxelFile);
			TFE_Console::addToHistory(tmp);
			return;
		}

		// Add to the list so it can be serialized later.
		s_voxelObjects.push_back({ voxelFile, offsetFromFloor, yUp, {s_cameraPos.x, s_cameraPos.z}, offset, s_sectorId, 1.0f, 0.0f, 0.0f, 0.0f });

		RSector* sector = &s_sectors->get()[s_sectorId];

		// TODO: This will leak memory, fix when this comes out of experimental.
		SecObject* obj = allocateObject();
		obj->self = obj;
		obj->type = OBJ_TYPE_VOXEL;
		obj->typeFlags = 0;

		obj->posWS.x.f32 = s_cameraPos.x;
		obj->posWS.z.f32 = s_cameraPos.z;
		obj->posWS.y.f32 = offsetFromFloor ? (sector->floorHeight.f32 + offset) : (sector->ceilingHeight.f32 + offset + f32(voxel->height)*0.1f);
		obj3d_computeTransform_Float(obj, 0.0f, 0.0f, 0.0f);

		obj->voxel = voxel;

		obj->frame = 0;
		obj->anim = 0;
		obj->sector = sector;
		obj->flags = OBJ_FLAG_RENDERABLE;

		obj->yaw = 0;
		obj->pitch = 0;
		obj->roll = 0;

		obj->gameObjId = 0xffffffff;
		s_sectors->addObject(sector, obj);
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
			s_sectors->subrendererChanged();
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
		s_cameraPos = { x, y, z };

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

		// For now setup sector and object data each frame.
		updateGameObjects();
		updateSectors();

		// Recursively draws sectors and their contents (sprites, 3D objects).
		{
			TFE_ZONE("Sector Draw");
			RSector* sector = s_sectors->get() + s_sectorId;
			s_sectors->draw(sector);
		}

		s_frame++;
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
		obj->flags |= OBJ_FLAG_RENDERABLE;
		WaxCell* cell = WAX_CellPtr(basePtr, data);

		if (obj->worldWidth == -1)
		{
			const fixed16_16 width = intToFixed16(abs(cell->sizeX));
			obj->worldWidth = div16(width, SPRITE_SCALE_FIXED);
		}
		if (obj->worldHeight == -1)
		{
			const fixed16_16 height = intToFixed16(abs(cell->sizeY));
			obj->worldHeight = div16(height, SPRITE_SCALE_FIXED);
		}
	}

	void wax_setData(SecObject* obj, u8* basePtr, Wax* data)
	{
		obj->type = OBJ_TYPE_SPRITE;
		obj->wax = data;
		obj->flags |= OBJ_FLAG_RENDERABLE;

		WaxAnim* anim = WAX_AnimPtr(basePtr, data, 0);
		WaxView* view = WAX_ViewPtr(basePtr, anim, 0);
		WaxFrame* frame = WAX_FramePtr(basePtr, view, 0);
		WaxCell* cell = WAX_CellPtr(basePtr, frame);

		if (obj->worldWidth == -1)
		{
			const fixed16_16 width = intToFixed16(abs(cell->sizeX));
			obj->worldWidth = div16(mul16(data->xScale, width), SPRITE_SCALE_FIXED);
		}
		if (obj->worldHeight == -1)
		{
			const fixed16_16 height = intToFixed16(abs(cell->sizeY));
			obj->worldHeight = div16(mul16(data->yScale, height), SPRITE_SCALE_FIXED);
		}
	}

	void obj3d_setData(SecObject* obj, JediModel* pod)
	{
		obj->model = pod;
		obj->type = OBJ_TYPE_3D;
		obj->flags |= OBJ_FLAG_RENDERABLE;
		obj->worldWidth = 0;
		obj->worldHeight = 0;
	}
		
	void obj3d_computeTransform_Fixed(SecObject* obj, s16 yaw, s16 pitch, s16 roll)
	{
		computeTransformFromAngles_Fixed(yaw, pitch, roll, obj->transform);
	}

	void obj3d_computeTransform_Float(SecObject* obj, f32 yaw, f32 pitch, f32 roll)
	{
		computeTransformFromAngles_Float(yaw, pitch, roll, obj->transformFlt);
	}

	void addObject(const char* assetName, u32 gameObjId, u32 sectorId)
	{
		if (!s_init || !assetName || sectorId >= s_sectors->getCount()) { return; }

		GameObject* gameObjects = LevelGameObjects::getGameObjectList()->data();
		GameObject* gameObj = &gameObjects[gameObjId];
				
		if (gameObj->oclass == CLASS_FRAME || gameObj->oclass == CLASS_SPRITE || gameObj->oclass == CLASS_3D)
		{
			SecObject* obj = allocateObject();
			obj->gameObjId = gameObjId;

			if (s_subRenderer == TSR_CLASSIC_FIXED)
			{
				obj->posWS.x.f16_16 = floatToFixed16(gameObj->position.x);
				obj->posWS.y.f16_16 = floatToFixed16(gameObj->position.y);
				obj->posWS.z.f16_16 = floatToFixed16(gameObj->position.z);
			}
			else
			{
				obj->posWS.x.f32 = gameObj->position.x;
				obj->posWS.y.f32 = gameObj->position.y;
				obj->posWS.z.f32 = gameObj->position.z;
			}
			obj->pitch = s16(gameObj->angles.x / 360.0f * 16484.0f) % 16384;
			obj->yaw   = s16(gameObj->angles.y / 360.0f * 16484.0f) % 16384;
			obj->roll  = s16(gameObj->angles.z / 360.0f * 16484.0f) % 16384;

			if (gameObj->oclass == CLASS_FRAME)
			{
				JediFrame* jFrame = TFE_Sprite_Jedi::getFrame(assetName);
				if (!jFrame)
				{
					free(obj);
					return;
				}
				obj->fme = jFrame->frame;
			}
			else if (gameObj->oclass == CLASS_SPRITE)
			{
				JediWax* jWax = TFE_Sprite_Jedi::getWax(assetName);
				if (!jWax)
				{
					free(obj);
					return;
				}
				obj->wax = jWax->wax;
			}
			else if (gameObj->oclass == CLASS_3D)
			{
				JediModel* jModel = TFE_Model_Jedi::get(assetName);
				if (!jModel)
				{
					free(obj);
					return;
				}
				obj->model = jModel;
				if (s_subRenderer == TSR_CLASSIC_FIXED)
				{
					obj3d_computeTransform_Fixed(obj, obj->yaw, obj->pitch, obj->roll);
				}
				else
				{
					obj3d_computeTransform_Float(obj, gameObj->angles.y, gameObj->angles.x, gameObj->angles.z);
				}
			}

			s_sectors->addObject(&s_sectors->get()[sectorId], obj);
			frame_setData(obj, (u8*)obj->fme, obj->fme);
		}
	}

	void updateGameObjects()
	{
		TFE_ZONE("Sector Object Update");
		RSector* sector = s_sectors->get();
		const u32 count = s_sectors->getCount();
		const LevelObjectData* levelObj = TFE_LevelObjects::getLevelObjectData();

		GameObject* gameObjects = LevelGameObjects::getGameObjectList()->data();
		for (u32 i = 0; i < count; i++, sector++)
		{
			SecObject** obj = sector->objectList;
			for (s32 i = sector->objectCount - 1; i >= 0; i--, obj++)
			{
				SecObject* curObj = *obj;
				while (!curObj)
				{
					obj++;
					curObj = *obj;
				}
				GameObject* gameObj = curObj->gameObjId < 0xffffffff ? &gameObjects[curObj->gameObjId] : nullptr;
				if (gameObj)
				{
					if (!gameObj->update) { continue; }
					gameObj->update = false;

					if (!gameObj->show)
					{
						// Remove the object from the sector.
						*obj = nullptr;
						sector->objectCount--;
						continue;
					}
				}
			
				if (gameObj && (curObj->type == OBJ_TYPE_FRAME || curObj->type == OBJ_TYPE_SPRITE || curObj->type == OBJ_TYPE_3D || curObj->type == OBJ_TYPE_VOXEL))
				{
					if (s_subRenderer == TSR_CLASSIC_FIXED)
					{
						curObj->posWS.x.f16_16 = floatToFixed16(gameObj->position.x);
						curObj->posWS.y.f16_16 = floatToFixed16(gameObj->position.y);
						curObj->posWS.z.f16_16 = floatToFixed16(gameObj->position.z);
					}
					else
					{
						curObj->posWS.x.f32 = gameObj->position.x;
						curObj->posWS.y.f32 = gameObj->position.y;
						curObj->posWS.z.f32 = gameObj->position.z;
					}
					curObj->pitch = s16(gameObj->angles.x / 360.0f * 16484.0f) % 16384;
					curObj->yaw   = s16(gameObj->angles.y / 360.0f * 16484.0f) % 16384;
					curObj->roll  = s16(gameObj->angles.z / 360.0f * 16484.0f) % 16384;

					curObj->frame = gameObj->frameIndex;
					curObj->anim  = gameObj->animId;
				}

				if (gameObj && (curObj->type == OBJ_TYPE_3D || curObj->type == OBJ_TYPE_VOXEL))
				{
					if (s_subRenderer == TSR_CLASSIC_FIXED)
					{
						obj3d_computeTransform_Fixed(curObj, curObj->yaw, curObj->pitch, curObj->roll);
					}
					else
					{
						obj3d_computeTransform_Float(curObj, gameObj->angles.y, gameObj->angles.x, gameObj->angles.z);
					}
				}
			}
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

		std::vector<JediModel*> models;
		const u32 mdlCount = (u32)levelObj->pods.size();
		models.resize(mdlCount);
		for (u32 i = 0; i < mdlCount; i++)
		{
			models[i] = TFE_Model_Jedi::get(levelObj->pods[i].c_str());
		}

		const LevelObject* srcObj = levelObj->objects.data();
		const u32 objCount = (u32)levelObj->objects.size();
		GameObject* gameObjects = LevelGameObjects::getGameObjectList()->data();
		for (u32 i = 0; i < objCount; i++, srcObj++)
		{
			// for now only worry about frames.
			if (srcObj->oclass == CLASS_FRAME || srcObj->oclass == CLASS_SPRITE || srcObj->oclass == CLASS_3D)
			{
				SecObject* obj = allocateObject();
				obj->gameObjId = i;
				gameObjects[i].update = true;

				if (s_subRenderer == TSR_CLASSIC_FIXED)
				{
					obj->posWS.x.f16_16 = floatToFixed16(srcObj->pos.x);
					obj->posWS.y.f16_16 = floatToFixed16(srcObj->pos.y);
					obj->posWS.z.f16_16 = floatToFixed16(srcObj->pos.z);
				}
				else
				{
					obj->posWS.x.f32 = srcObj->pos.x;
					obj->posWS.y.f32 = srcObj->pos.y;
					obj->posWS.z.f32 = srcObj->pos.z;
				}
				obj->pitch = s16(srcObj->orientation.x / 360.0f * 16484.0f) % 16384;
				obj->yaw   = s16(srcObj->orientation.y / 360.0f * 16484.0f) % 16384;
				obj->roll  = s16(srcObj->orientation.z / 360.0f * 16484.0f) % 16384;
								
				RSector* sector = s_sectors->which3D(obj->posWS.x, obj->posWS.y, obj->posWS.z);
				if (!sector)
				{
					continue;
				}

				if (srcObj->oclass == CLASS_FRAME)
				{
					obj->fme = frames[srcObj->dataOffset] ? frames[srcObj->dataOffset]->frame : nullptr;
					if (!obj->fme) { continue; }

					frame_setData(obj, (u8*)obj->fme, obj->fme);
				}
				else if (srcObj->oclass == CLASS_SPRITE)
				{
					obj->wax = waxes[srcObj->dataOffset] ? waxes[srcObj->dataOffset]->wax : nullptr;
					if (!obj->wax) { continue; }

					wax_setData(obj, (u8*)obj->wax, obj->wax);
				}
				else if (srcObj->oclass == CLASS_3D)
				{
					obj->model = models[srcObj->dataOffset] ? models[srcObj->dataOffset] : nullptr;
					if (!obj->model) { continue; }
										
					obj3d_setData(obj, obj->model);
					if (s_subRenderer == TSR_CLASSIC_FIXED)
					{
						obj3d_computeTransform_Fixed(obj, obj->yaw, obj->pitch, obj->roll);
					}
					else
					{
						obj3d_computeTransform_Float(obj, srcObj->orientation.y, srcObj->orientation.x, srcObj->orientation.z);
					}
				}
				s_sectors->addObject(sector, obj);
			}
		}

	#if 0
		// Test
		VoxelModel* testVoxel = TFE_VoxelModel::get("test");
		if (testVoxel)
		{
			SecObject* obj = allocateObject();
			obj->gameObjId = 0xffffffff;

			RSector* sector = &s_sectors->get()[173];
			if (!sector)
			{
				return;
			}

			obj->posWS.x.f32 = 220.0f;
			obj->posWS.y.f32 = sector->floorHeight.f32;
			obj->posWS.z.f32 = 260.0f;

			obj->pitch = 0;
			obj->yaw = 0;
			obj->roll = 0;

			obj->voxel = testVoxel;
			obj->type = OBJ_TYPE_VOXEL;
			obj->typeFlags = 0;
			obj->anim = 0;
			obj->frame = 0;
			obj->flags = OBJ_FLAG_RENDERABLE;
			obj3d_computeTransform_Float(obj, srcObj->orientation.y, srcObj->orientation.x, srcObj->orientation.z);

			s_sectors->addObject(sector, obj);
		}
	#endif
	}
}
