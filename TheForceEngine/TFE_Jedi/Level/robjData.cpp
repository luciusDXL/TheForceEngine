#include "robjData.h"
#include "robject.h"
#include "level.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <TFE_DarkForces/logic.h>
#include <TFE_Memory/chunkedArray.h>
#include <TFE_System/system.h>

namespace TFE_Jedi
{
	const u32 c_objStateVersion = 1;

	// Serialization - Keep a global list of objects...
	struct SectorObjectData
	{
		ChunkedArray* objectList = nullptr;
		SecObject* playerEye = nullptr;
		SecObject* player = nullptr;
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
		
	SecObject* objData_getPlayerEye()
	{
		return s_objData.playerEye;
	}

	SecObject* objData_getPlayer()
	{
		return s_objData.player;
	}

	void objData_serializeObject(SecObject* obj, Stream* stream)
	{
		SERIALIZE(obj->type);
		SERIALIZE(obj->entityFlags);

		// TODO: Handle obj->projectileLogic.

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

		// TODO: Handle obj->logic

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

		// TODO: Handle obj->projectileLogic.
		obj->projectileLogic = nullptr;

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

		// TODO: Handle obj->logic
		obj->logic = nullptr;

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
		size_t writeCountLoc = stream->getLoc();
		SERIALIZE(writeCount);

		ChunkedArray* list = s_objData.objectList;
		if (!list || TFE_Memory::chunkedArraySize(list) < 1)
		{
			// No objects, so just write out 0 for the count.
			return;
		}

		const u32 size = TFE_Memory::chunkedArraySize(list);
		SecObject* obj = (SecObject*)TFE_Memory::chunkedArrayGet(list, 0);
		for (u32 i = 0; i < size; i++, obj++)
		{
			// Skip deleted objects.
			if (!obj->self) { continue; }
			// Write the object to the stream.
			obj->serializeIndex = writeCount;  // So downstream serialization passes have an object ID to use.
			objData_serializeObject(obj, stream);
			writeCount++;
		}
		// Fix-up the write count.
		size_t endLoc = stream->getLoc();
		stream->seek((u32)writeCountLoc);
		SERIALIZE(writeCount);
		stream->seek((u32)endLoc);
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
		s_objData.player = nullptr;
		s_objData.playerEye = nullptr;

		for (u32 i = 0; i < count; i++)
		{
			SecObject* obj = (SecObject*)TFE_Memory::allocFromChunkedArray(s_objData.objectList);
			objData_deserializeObject(obj, stream);

			if (obj->sector)
			{
				sector_addObjectDirect(obj->sector, obj);
				// Grab the player info now so we don't have to loop through this when deserializing player data.
				// Technically there should be a separation between the object and game data - but the flags are
				// here anyway...
				if (obj->flags & OBJ_FLAG_EYE)
				{
					s_objData.playerEye = obj;
				}
				else if (obj->entityFlags & ETFLAG_PLAYER)
				{
					s_objData.player = obj;
				}
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "Game Load", "Object [%d] has no sector.", i);
			}
		}
	}
} // namespace TFE_Jedi