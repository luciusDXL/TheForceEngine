#include "robjData.h"
#include "robject.h"
#include "level.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <TFE_DarkForces/logic.h>
#include <TFE_DarkForces/generator.h>
#include <TFE_Memory/chunkedArray.h>
#include <TFE_System/system.h>
#include <cstring>

// Required for serialization.
namespace TFE_DarkForces
{
	extern void logic_serialize(Logic*& logic, Stream* stream);
}

namespace TFE_Jedi
{
	// Serialization - Keep a global list of objects...
	struct SectorObjectData
	{
		ChunkedArray* objectList = nullptr;
	};
	static SectorObjectData s_objData = {};
		
	void objData_clear()
	{
		s_objData = {};
	}

	SecObject* objData_allocFromArray()
	{
		if (!s_objData.objectList)
		{
			s_objData.objectList = TFE_Memory::createChunkedArray(sizeof(SecObject), 256, 1, s_levelRegion);
		}
		return (SecObject*)TFE_Memory::allocFromChunkedArray(s_objData.objectList);
	}

	void objData_freeToArray(SecObject* obj)
	{
		if (obj && s_objData.objectList)
		{
			obj->self = nullptr;
			TFE_Memory::freeToChunkedArray(s_objData.objectList, obj);
		}
	}

	void objData_serializeObject(SecObject* obj, Stream* stream)
	{
		if (serialization_getMode() == SMODE_READ)
		{
			obj->self = obj;
		}
		else
		{
			assert(obj && obj->sector && obj->sector->id == obj->sector->index);
		}

		const vec3_fixed def = { 0 };
		SERIALIZE(ObjState_InitVersion, obj->type, ObjectType::OBJ_TYPE_SPIRIT);
		SERIALIZE(ObjState_InitVersion, obj->entityFlags, 0);
		SERIALIZE(ObjState_InitVersion, obj->posWS, def);
		// obj->posVS is derived at runtime.

		SERIALIZE(ObjState_InitVersion, obj->worldWidth,  -1);
		SERIALIZE(ObjState_InitVersion, obj->worldHeight, -1);
		if (obj->type == OBJ_TYPE_3D)
		{
			SERIALIZE_BUF(ObjState_InitVersion, obj->transform, TFE_ARRAYSIZE(obj->transform) * sizeof(fixed16_16));
		}
		else if (serialization_getMode() == SMODE_READ)
		{
			memset(obj->transform, 0, TFE_ARRAYSIZE(obj->transform) * sizeof(fixed16_16));
		}

		// Handle object render data
		if (obj->type == OBJ_TYPE_SPRITE)
		{
			serialization_serializeWaxPtr(stream, ObjState_InitVersion, obj->wax);
		}
		else if (obj->type == OBJ_TYPE_3D)
		{
			serialization_serialize3doPtr(stream, ObjState_InitVersion, obj->model);
		}
		else if (obj->type == OBJ_TYPE_FRAME)
		{
			serialization_serializeFramePtr(stream, ObjState_InitVersion, obj->fme);
		}
		else if (serialization_getMode() == SMODE_READ)
		{
			obj->ptr = nullptr;
		}
		
		SERIALIZE(ObjState_InitVersion, obj->frame, 0);
		SERIALIZE(ObjState_InitVersion, obj->anim, 0);
		serialization_serializeSectorPtr(stream, ObjState_InitVersion, obj->sector);
		assert(obj && obj->sector && obj->sector->id == obj->sector->index);

		SERIALIZE(ObjState_InitVersion, obj->flags, 0);
		SERIALIZE(ObjState_InitVersion, obj->pitch, 0);
		SERIALIZE(ObjState_InitVersion, obj->yaw, 0);
		SERIALIZE(ObjState_InitVersion, obj->roll, 0);

		// obj->index will be reset once the object is re-added to its sector.
		if (serialization_getMode() == SMODE_READ)
		{
			obj->index = -1;
		}

		SERIALIZE(ObjState_InitVersion, obj->serializeIndex, 0);
	}

	SecObject* objData_getObjectBySerializationId(u32 id)
	{
		SecObject* obj = (SecObject*)TFE_Memory::chunkedArrayGet(s_objData.objectList, id);
		// The object may have been deleted later.
		return (obj && obj->serializeIndex == id) ? obj : nullptr;
	}

	SecObject* objData_getObjectBySerializationId_NoValidation(u32 id)
	{
		return (SecObject*)TFE_Memory::chunkedArrayGet(s_objData.objectList, id);
	}
		
	void objData_serialize(Stream* stream)
	{
		SERIALIZE_VERSION(ObjState_CurVersion);

		u32 writeCount = 0;
		if (serialization_getMode() == SMODE_WRITE)
		{
			ChunkedArray* list = s_objData.objectList;
			if (!list || TFE_Memory::chunkedArraySize(list) < 1)
			{
				// No objects, so just write out 0 for the count.
				SERIALIZE(ObjState_InitVersion, writeCount, 0);
				return;
			}

			// Assign IDs
			const u32 size = TFE_Memory::chunkedArraySize(list);
			for (u32 i = 0; i < size; i++)
			{
				SecObject* obj = (SecObject*)TFE_Memory::chunkedArrayGet(list, i);

				// Skip deleted objects.
				if (!obj->self) { continue; }
				obj->serializeIndex = writeCount;  // So downstream serialization passes have an object ID to use.
				writeCount++;
			}
			SERIALIZE(ObjState_InitVersion, writeCount, 0);

			// Write objects.
			for (u32 i = 0; i < size; i++)
			{
				SecObject* obj = (SecObject*)TFE_Memory::chunkedArrayGet(list, i);

				// Skip deleted objects.
				if (!obj->self) { continue; }
				// Write the object to the stream.
				objData_serializeObject(obj, stream);

				u32 logicCount = allocator_getCount((Allocator*)obj->logic);
				SERIALIZE(ObjState_InitVersion, logicCount, 0);
				if (!logicCount) { continue; }

				allocator_saveIter((Allocator*)obj->logic);
					Logic** logicList = (Logic**)allocator_getHead((Allocator*)obj->logic);
					while (logicList)
					{
						Logic* logic = *logicList;
						if (logic)
						{
							TFE_DarkForces::logic_serialize(logic, obj, stream);
						}
						else
						{
							s32 invalidLogic = -1;
							SERIALIZE(ObjState_InitVersion, invalidLogic, -1);
						}
						logicList = (Logic**)allocator_getNext((Allocator*)obj->logic);
					}
				allocator_restoreIter((Allocator*)obj->logic);
			}
		}
		else if (serialization_getMode() == SMODE_READ)
		{
			SERIALIZE(ObjState_InitVersion, writeCount, 0);
			if (!s_objData.objectList)
			{
				const u32 initChunkCount = (u32)(s32)max((s32)1, (s32)((writeCount + 255) >> 8));
				s_objData.objectList = TFE_Memory::createChunkedArray(sizeof(SecObject), 256, initChunkCount, s_levelRegion);
			}
			else
			{
				TFE_Memory::chunkedArrayClear(s_objData.objectList);
			}

			for (u32 i = 0; i < writeCount; i++)
			{
				SecObject* obj = (SecObject*)TFE_Memory::allocFromChunkedArray(s_objData.objectList);
				objData_serializeObject(obj, stream);

				if (obj->sector)
				{
					sector_addObjectDirect(obj->sector, obj);
				}
				else
				{
					TFE_System::logWrite(LOG_ERROR, "Game Load", "Object [%d] has no sector.", i);
				}

				u32 logicCount;
				SERIALIZE(ObjState_InitVersion, logicCount, 0);
				obj->logic = nullptr;
				obj->projectileLogic = nullptr;
				if (!logicCount) { continue; }

				obj->logic = allocator_create(sizeof(Logic**));
				for (u32 l = 0; l < logicCount; l++)
				{
					Logic* logic = nullptr;
					TFE_DarkForces::logic_serialize(logic, obj, stream);
					if (!logic) { continue; }

					Logic** logicItem = (Logic**)allocator_newItem((Allocator*)obj->logic);
					logic->parent = logicItem;
					logic->obj = obj;
					*logicItem = logic;

					// Handle projectiles.
					if (logic->type == LOGIC_PROJECTILE)
					{
						obj->projectileLogic = logic;
					}
				}
			}

			// Fix-up generator references.
			for (u32 i = 0; i < writeCount; i++)
			{
				SecObject* obj = (SecObject*)TFE_Memory::chunkedArrayGet(s_objData.objectList, i);

				allocator_saveIter((Allocator*)obj->logic);
					Logic** logicPtr = (Logic**)allocator_getHead((Allocator*)obj->logic);
					while (logicPtr)
					{
						Logic* logic = *logicPtr;
						if (logic->type == LOGIC_GENERATOR)
						{
							TFE_DarkForces::generatorLogic_fixup(logic);
						}
						logicPtr = (Logic**)allocator_getNext((Allocator*)obj->logic);
					}
				allocator_restoreIter((Allocator*)obj->logic);
			}
		}
	}
} // namespace TFE_Jedi