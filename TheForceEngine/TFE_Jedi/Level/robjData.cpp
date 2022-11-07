#include "robjData.h"
#include "robject.h"
#include "level.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <TFE_DarkForces/logic.h>
#include <TFE_Memory/chunkedArray.h>
#include <TFE_System/system.h>

// Required for serialization.
namespace TFE_DarkForces
{
	extern void logic_serialize(Logic* logic, Stream* stream);
	extern Logic* logic_deserialize(Logic** parent, Stream* stream);
}

namespace TFE_Jedi
{
	const u32 c_objStateVersion = 1;

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
		SERIALIZE(obj->type);
		SERIALIZE(obj->entityFlags);
		SERIALIZE(obj->posWS);
		// obj->posVS is derived at runtime.

		SERIALIZE(obj->worldWidth);
		SERIALIZE(obj->worldHeight);
		if (obj->type == OBJ_TYPE_3D)
		{
			SERIALIZE_BUF(obj->transform, TFE_ARRAYSIZE(obj->transform) * sizeof(fixed16_16));
		}

		// Handle object render data
		if (obj->type == OBJ_TYPE_SPRITE)
		{
			serialize_writeWaxPtr(stream, obj->wax);
		}
		else if (obj->type == OBJ_TYPE_3D)
		{
			serialize_write3doPtr(stream, obj->model);
		}
		else if (obj->type == OBJ_TYPE_FRAME)
		{
			serialize_writeFramePtr(stream, obj->fme);
		}
		
		SERIALIZE(obj->frame);
		SERIALIZE(obj->anim);
		serialization_writeSectorPtr(stream, obj->sector);

		SERIALIZE(obj->flags);
		SERIALIZE(obj->pitch);
		SERIALIZE(obj->yaw);
		SERIALIZE(obj->roll);
		// obj->index will be reset once the object is re-added to its sector.

		SERIALIZE(obj->serializeIndex);
	}

	void objData_deserializeObject(SecObject* obj, Stream* stream)
	{
		obj->self = obj;
		DESERIALIZE(obj->type);
		DESERIALIZE(obj->entityFlags);
		DESERIALIZE(obj->posWS);
		// obj->posVS is derived at runtime.

		DESERIALIZE(obj->worldWidth);
		DESERIALIZE(obj->worldHeight);
		if (obj->type == OBJ_TYPE_3D)
		{
			DESERIALIZE_BUF(obj->transform, TFE_ARRAYSIZE(obj->transform) * sizeof(fixed16_16));
		}
		else
		{
			memset(obj->transform, 0, TFE_ARRAYSIZE(obj->transform) * sizeof(fixed16_16));
		}

		// Handle object render data
		obj->ptr = nullptr;
		if (obj->type == OBJ_TYPE_SPRITE)
		{
			obj->wax = serialize_readWaxPtr(stream);
		}
		else if (obj->type == OBJ_TYPE_3D)
		{
			obj->model = serialize_read3doPtr(stream);
		}
		else if (obj->type == OBJ_TYPE_FRAME)
		{
			obj->fme = serialize_readFramePtr(stream);
		}

		DESERIALIZE(obj->frame);
		DESERIALIZE(obj->anim);
		obj->sector = serialization_readSectorPtr(stream);

		DESERIALIZE(obj->flags);
		DESERIALIZE(obj->pitch);
		DESERIALIZE(obj->yaw);
		DESERIALIZE(obj->roll);
		// obj->index will be reset once the object is re-added to its sector.
		obj->index = -1;

		DESERIALIZE(obj->serializeIndex);
	}

	SecObject* objData_getObjectBySerializationId(u32 id)
	{
		SecObject* obj = (SecObject*)TFE_Memory::chunkedArrayGet(s_objData.objectList, id);
		assert(obj->serializeIndex == id);
		return (obj->serializeIndex == id) ? obj : nullptr;
	}
		
	void objData_serialize(Stream* stream)
	{
		// Data version.
		SERIALIZE(c_objStateVersion);

		u32 writeCount = 0;
		ChunkedArray* list = s_objData.objectList;
		if (!list || TFE_Memory::chunkedArraySize(list) < 1)
		{
			// No objects, so just write out 0 for the count.
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

		// Write objects.
		SERIALIZE(writeCount);
		for (u32 i = 0; i < size; i++)
		{
			SecObject* obj = (SecObject*)TFE_Memory::chunkedArrayGet(list, i);

			// Skip deleted objects.
			if (!obj->self) { continue; }
			// Write the object to the stream.
			objData_serializeObject(obj, stream);

			u32 logicCount = allocator_getCount((Allocator*)obj->logic);
			SERIALIZE(logicCount);
			if (!logicCount) { continue; }

			Logic** logicList = (Logic**)allocator_getHead((Allocator*)obj->logic);
			while (logicList)
			{
				Logic* logic = *logicList;
				if (logic)
				{
					TFE_DarkForces::logic_serialize(logic, stream);
				}
				else
				{
					s32 invalidLogic = -1;
					SERIALIZE(invalidLogic);
				}
				logicList = (Logic**)allocator_getNext((Allocator*)obj->logic);
			};
		}
	}

	void objData_deserialize(Stream* stream)
	{
		u32 version, count;
		DESERIALIZE(version);
		DESERIALIZE(count);

		if (!s_objData.objectList)
		{
			const u32 initChunkCount = max(1, (count + 255) >> 8);
			s_objData.objectList = TFE_Memory::createChunkedArray(sizeof(SecObject), 256, initChunkCount, s_levelRegion);
		}
		else
		{
			TFE_Memory::chunkedArrayClear(s_objData.objectList);
		}

		for (u32 i = 0; i < count; i++)
		{
			SecObject* obj = (SecObject*)TFE_Memory::allocFromChunkedArray(s_objData.objectList);
			objData_deserializeObject(obj, stream);

			if (obj->sector)
			{
				sector_addObjectDirect(obj->sector, obj);
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "Game Load", "Object [%d] has no sector.", i);
			}

			u32 logicCount;
			DESERIALIZE(logicCount);
			obj->logic = nullptr;
			if (!logicCount) { continue; }

			obj->logic = allocator_create(sizeof(Logic**));
			for (u32 i = 0; i < logicCount; i++)
			{
				Logic** logicItem = (Logic**)allocator_newItem((Allocator*)obj->logic);
				Logic* logic = TFE_DarkForces::logic_deserialize(logicItem, stream);
				*logicItem = logic;

				obj->projectileLogic = nullptr;
				if (logic->type == LOGIC_PROJECTILE)
				{
					obj->projectileLogic = logic;
				}
			}
		}
	}
} // namespace TFE_Jedi