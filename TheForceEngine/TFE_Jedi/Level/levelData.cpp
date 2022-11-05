#include <climits>
#include <cstring>

#include "levelData.h"
#include "rsector.h"
#include "rwall.h"
#include "robjData.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Serialization/serialization.h>

// TODO: coupling between Dark Forces and Jedi.
using namespace TFE_DarkForces;
#include <TFE_DarkForces/projectile.h>

namespace TFE_Jedi
{
	const s32 c_levelStateVersion = 1;

	LevelState s_levelState = {};
	LevelInternalState s_levelIntState = {};

	void level_serializeSector(Stream* stream, RSector* sector);
	void level_serializeSafe(Stream* stream, Safe* safe);
	void level_serializeAmbientSound(Stream* stream, AmbientSound* sound);
	void level_serializeWall(Stream* stream, RWall* wall, RSector* sector);
	void level_serializeTextureList(Stream* stream);
	
	void level_deserializeSector(Stream* stream, RSector* sector);
	void level_deserializeSafe(Stream* stream, Safe* safe);
	void level_deserializeAmbientSound(Stream* stream, AmbientSound* sound);
	void level_deserializeWall(Stream* stream, RWall* wall, RSector* sector);
	void level_deserializeTextureList(Stream* stream);

	/////////////////////////////////////////////
	// Implementation
	/////////////////////////////////////////////
	void level_clearData()
	{
		s_levelState = { 0 };
		s_levelIntState = { 0 };

		s_levelState.controlSector = (RSector*)level_alloc(sizeof(RSector));
		sector_clear(s_levelState.controlSector);

		objData_clear();
	}
		
	void level_serialize(Stream* stream)
	{
		SERIALIZE(c_levelStateVersion);

		SERIALIZE(s_levelState.minLayer);
		SERIALIZE(s_levelState.maxLayer);
		SERIALIZE(s_levelState.secretCount);
		SERIALIZE(s_levelState.sectorCount);
		SERIALIZE(s_levelState.parallax0);
		SERIALIZE(s_levelState.parallax1);

		// This is needed because level textures are pointers to the list itself, rather than the texture.
		level_serializeTextureList(stream);

		RSector* sector = s_levelState.sectors;
		for (u32 s = 0; s < s_levelState.sectorCount; s++, sector++)
		{
			level_serializeSector(stream, sector);
		}

		serialization_writeSectorPtr(stream, s_levelState.bossSector);
		serialization_writeSectorPtr(stream, s_levelState.mohcSector);
		serialization_writeSectorPtr(stream, s_levelState.completeSector);
		// The control sector is just a dummy sector, no need to save anything.
		// Sound emitter isn't actually used.

		s32 safeCount = allocator_getCount(s_levelState.safeLoc);
		SERIALIZE(safeCount);
		Safe* safe = (Safe*)allocator_getHead(s_levelState.safeLoc);
		while (safe)
		{
			level_serializeSafe(stream, safe);
			safe = (Safe*)allocator_getNext(s_levelState.safeLoc);
		}

		s32 ambientSoundCount = allocator_getCount(s_levelState.ambientSounds);
		SERIALIZE(ambientSoundCount);
		AmbientSound* sound = (AmbientSound*)allocator_getHead(s_levelState.ambientSounds);
		while (sound)
		{
			level_serializeAmbientSound(stream, sound);
			sound = (AmbientSound*)allocator_getNext(s_levelState.ambientSounds);
		}

		// Serialize objects.
		objData_serialize(stream);
	}

	void level_deserialize(Stream* stream)
	{
		level_clearData();

		s32 version;
		DESERIALIZE(version);
		DESERIALIZE(s_levelState.minLayer);
		DESERIALIZE(s_levelState.maxLayer);
		DESERIALIZE(s_levelState.secretCount);
		DESERIALIZE(s_levelState.sectorCount);
		DESERIALIZE(s_levelState.parallax0);
		DESERIALIZE(s_levelState.parallax1);

		// This is needed because level textures are pointers to the list itself, rather than the texture.
		level_deserializeTextureList(stream);

		s_levelState.sectors = (RSector*)level_alloc(sizeof(RSector) * s_levelState.sectorCount);
		RSector* sector = s_levelState.sectors;
		for (u32 s = 0; s < s_levelState.sectorCount; s++, sector++)
		{
			level_deserializeSector(stream, sector);
		}

		s_levelState.bossSector = serialization_readSectorPtr(stream);
		s_levelState.mohcSector = serialization_readSectorPtr(stream);
		s_levelState.completeSector = serialization_readSectorPtr(stream);
		// The control sector is just a dummy sector, no need to save anything.
		// Sound emitter isn't actually used.

		s32 safeCount;
		DESERIALIZE(safeCount);
		s_levelState.safeLoc = nullptr;
		if (safeCount)
		{
			s_levelState.safeLoc = allocator_create(sizeof(Safe));
			for (s32 s = 0; s < safeCount; s++)
			{
				Safe* safe = (Safe*)allocator_newItem(s_levelState.safeLoc);
				level_deserializeSafe(stream, safe);
			}
		}

		s32 ambientSoundCount;
		DESERIALIZE(ambientSoundCount);
		s_levelState.ambientSounds = nullptr;
		if (ambientSoundCount)
		{
			s_levelState.ambientSounds = allocator_create(sizeof(AmbientSound));
			for (s32 s = 0; s < ambientSoundCount; s++)
			{
				AmbientSound* sound = (AmbientSound*)allocator_newItem(s_levelState.ambientSounds);
				level_deserializeAmbientSound(stream, sound);
			}
		}

		// Objects
		objData_deserialize(stream);
	}
		
	/////////////////////////////////////////////
	// Internal - Serialize
	/////////////////////////////////////////////
	void level_serializeTextureList(Stream* stream)
	{
		SERIALIZE(s_levelState.textureCount);
		for (s32 i = 0; i < s_levelState.textureCount; i++)
		{
			serialize_writeTexturePtr(stream, s_levelState.textures[i]);
		}
	}

	void level_deserializeTextureList(Stream* stream)
	{
		DESERIALIZE(s_levelState.textureCount);
		s_levelState.textures = (TextureData**)level_alloc(s_levelState.textureCount * sizeof(TextureData**));
		for (s32 i = 0; i < s_levelState.textureCount; i++)
		{
			s_levelState.textures[i] = serialize_readTexturePtr(stream);
		}
	}

	void level_writeTexturePointer(Stream* stream, TextureData** texData)
	{
		s32 index = -1;
		if (!texData)
		{
			SERIALIZE(index);
			return;
		}

		size_t offset = size_t(texData - s_levelState.textures) / sizeof(TextureData**);
		index = s32(offset);
		SERIALIZE(index);
	}

	TextureData** level_readTexturePointer(Stream* stream)
	{
		s32 index;
		DESERIALIZE(index);
		if (index < 0) { return nullptr; }

		return &s_levelState.textures[index];
	}

	void level_serializeSector(Stream* stream, RSector* sector)
	{
		SERIALIZE(sector->id);
		SERIALIZE(sector->index);
		SERIALIZE(sector->prevDrawFrame);
		SERIALIZE(sector->prevDrawFrame2);

		SERIALIZE(sector->vertexCount);
		SERIALIZE_BUF(sector->verticesWS, sizeof(vec2_fixed) * sector->vertexCount);
		// view space vertices don't need to be serialized.

		SERIALIZE(sector->wallCount);
		RWall* wall = sector->walls;
		for (s32 w = 0; w < sector->wallCount; w++, wall++)
		{
			level_serializeWall(stream, wall, sector);
		}

		SERIALIZE(sector->floorHeight);
		SERIALIZE(sector->ceilingHeight);
		SERIALIZE(sector->secHeight);
		SERIALIZE(sector->ambient);

		SERIALIZE(sector->colFloorHeight);
		SERIALIZE(sector->colCeilHeight);
		SERIALIZE(sector->colSecHeight);
		SERIALIZE(sector->colMinHeight);
		// infLink will be set when the INF system is serialized.

		level_writeTexturePointer(stream, sector->floorTex);
		level_writeTexturePointer(stream, sector->ceilTex);
		SERIALIZE(sector->floorOffset);
		SERIALIZE(sector->ceilOffset);

		// Objects are serialized seperately.

		SERIALIZE(sector->collisionFrame);
		SERIALIZE(sector->startWall);
		SERIALIZE(sector->drawWallCnt);
		SERIALIZE(sector->flags1);
		SERIALIZE(sector->flags2);
		SERIALIZE(sector->flags3);
		SERIALIZE(sector->layer);
		SERIALIZE(sector->boundsMin);
		SERIALIZE(sector->boundsMax);

		// dirty flags will be set on deserialization.
	}
		
	void level_serializeSafe(Stream* stream, Safe* safe)
	{
		serialization_writeSectorPtr(stream, safe->sector);
		SERIALIZE(safe->x);
		SERIALIZE(safe->z);
		SERIALIZE(safe->yaw);
		SERIALIZE(safe->pad);	// for padding.
	}

	void level_serializeAmbientSound(Stream* stream, AmbientSound* sound)
	{
		serialization_writeDfSound(stream, sound->soundId);
		// instanceId will be cleared and reset.
		SERIALIZE(sound->pos);
	}
		
	void level_serializeWall(Stream* stream, RWall* wall, RSector* sector)
	{
		SERIALIZE(wall->id);
		SERIALIZE(wall->seen);
		SERIALIZE(wall->visible);
		serialization_writeSectorPtr(stream, wall->sector);
		serialization_writeSectorPtr(stream, wall->nextSector);
		// mirrorWall will be computed from mirror.
		SERIALIZE(wall->drawFlags);
		SERIALIZE(wall->mirror);
		
		// worldspace vertices.
		const s32 i0 = PTR_INDEX_S32(wall->w0, sector->verticesWS, sizeof(vec2_fixed));
		const s32 i1 = PTR_INDEX_S32(wall->w1, sector->verticesWS, sizeof(vec2_fixed));
		SERIALIZE(i0);
		SERIALIZE(i1);
		// viewspace vertices need not be saved.

		level_writeTexturePointer(stream, wall->topTex);
		level_writeTexturePointer(stream, wall->midTex);
		level_writeTexturePointer(stream, wall->botTex);
		level_writeTexturePointer(stream, wall->signTex);

		SERIALIZE(wall->texelLength);
		SERIALIZE(wall->topTexelHeight);
		SERIALIZE(wall->midTexelHeight);
		SERIALIZE(wall->botTexelHeight);

		SERIALIZE(wall->topOffset);
		SERIALIZE(wall->midOffset);
		SERIALIZE(wall->botOffset);
		SERIALIZE(wall->signOffset);

		SERIALIZE(wall->wallDir);
		SERIALIZE(wall->length);
		SERIALIZE(wall->collisionFrame);
		SERIALIZE(wall->drawFrame);

		// infLink will be filled in when serializing the INF system.

		SERIALIZE(wall->flags1);
		SERIALIZE(wall->flags2);
		SERIALIZE(wall->flags3);

		SERIALIZE(wall->worldPos0);
		SERIALIZE(wall->wallLight);
		SERIALIZE(wall->angle);
	}

	/////////////////////////////////////////////
	// Internal - Deserialize
	/////////////////////////////////////////////
	void level_deserializeSector(Stream* stream, RSector* sector)
	{
		DESERIALIZE(sector->id);
		DESERIALIZE(sector->index);
		DESERIALIZE(sector->prevDrawFrame);
		DESERIALIZE(sector->prevDrawFrame2);

		DESERIALIZE(sector->vertexCount);
		const size_t vtxSize = sector->vertexCount * sizeof(vec2_fixed);
		sector->verticesWS = (vec2_fixed*)level_alloc(vtxSize);
		sector->verticesVS = (vec2_fixed*)level_alloc(vtxSize);

		DESERIALIZE_BUF(sector->verticesWS, sizeof(vec2_fixed) * sector->vertexCount);
		// view space vertices don't need to be deserialized.

		DESERIALIZE(sector->wallCount);
		sector->walls = (RWall*)level_alloc(sector->wallCount * sizeof(RWall));
		RWall* wall = sector->walls;
		for (s32 w = 0; w < sector->wallCount; w++, wall++)
		{
			level_deserializeWall(stream, wall, sector);
		}

		DESERIALIZE(sector->floorHeight);
		DESERIALIZE(sector->ceilingHeight);
		DESERIALIZE(sector->secHeight);
		DESERIALIZE(sector->ambient);

		DESERIALIZE(sector->colFloorHeight);
		DESERIALIZE(sector->colCeilHeight);
		DESERIALIZE(sector->colSecHeight);
		DESERIALIZE(sector->colMinHeight);
		// infLink will be set when the INF system is serialized.
		sector->infLink = nullptr;

		sector->floorTex = level_readTexturePointer(stream);
		sector->ceilTex  = level_readTexturePointer(stream);
		DESERIALIZE(sector->floorOffset);
		DESERIALIZE(sector->ceilOffset);

		// Objects are handled seperately and added to the sector later, so just initialize the object data.
		sector->objectCount = 0;
		sector->objectCapacity = 0;
		sector->objectList = nullptr;

		DESERIALIZE(sector->collisionFrame);
		DESERIALIZE(sector->startWall);
		DESERIALIZE(sector->drawWallCnt);
		DESERIALIZE(sector->flags1);
		DESERIALIZE(sector->flags2);
		DESERIALIZE(sector->flags3);
		DESERIALIZE(sector->layer);
		DESERIALIZE(sector->boundsMin);
		DESERIALIZE(sector->boundsMax);

		// everything is dirty...
		sector->dirtyFlags = SDF_ALL;
	}

	void level_deserializeSafe(Stream* stream, Safe* safe)
	{
		safe->sector = serialization_readSectorPtr(stream);
		DESERIALIZE(safe->x);
		DESERIALIZE(safe->z);
		DESERIALIZE(safe->yaw);
		DESERIALIZE(safe->pad);	// for padding.
	}

	void level_deserializeAmbientSound(Stream* stream, AmbientSound* sound)
	{
		serialization_readDfSound(stream, &sound->soundId);
		// instanceId will be cleared and reset.
		DESERIALIZE(sound->pos);
	}

	void level_deserializeWall(Stream* stream, RWall* wall, RSector* sector)
	{
		DESERIALIZE(wall->id);
		DESERIALIZE(wall->seen);
		DESERIALIZE(wall->visible);
		wall->sector = serialization_readSectorPtr(stream);
		wall->nextSector = serialization_readSectorPtr(stream);
		DESERIALIZE(wall->drawFlags);
		DESERIALIZE(wall->mirror);

		// compute mirrorWall from mirror.
		wall->mirrorWall = nullptr;
		RSector* nextSector = wall->nextSector;
		if (nextSector && wall->mirror >= 0)
		{
			RWall* mirror = &nextSector->walls[wall->mirror];
			wall->mirrorWall = mirror;
		}

		s32 i0, i1;
		DESERIALIZE(i0);
		DESERIALIZE(i1);
		wall->w0 = &sector->verticesWS[i0];
		wall->w1 = &sector->verticesWS[i1];
		wall->v0 = &sector->verticesVS[i0];
		wall->v1 = &sector->verticesVS[i1];

		wall->topTex  = level_readTexturePointer(stream);
		wall->midTex  = level_readTexturePointer(stream);
		wall->botTex  = level_readTexturePointer(stream);
		wall->signTex = level_readTexturePointer(stream);

		DESERIALIZE(wall->texelLength);
		DESERIALIZE(wall->topTexelHeight);
		DESERIALIZE(wall->midTexelHeight);
		DESERIALIZE(wall->botTexelHeight);

		DESERIALIZE(wall->topOffset);
		DESERIALIZE(wall->midOffset);
		DESERIALIZE(wall->botOffset);
		DESERIALIZE(wall->signOffset);

		DESERIALIZE(wall->wallDir);
		DESERIALIZE(wall->length);
		DESERIALIZE(wall->collisionFrame);
		DESERIALIZE(wall->drawFrame);

		// infLink will be filled in when serializing the INF system.
		wall->infLink = nullptr;

		DESERIALIZE(wall->flags1);
		DESERIALIZE(wall->flags2);
		DESERIALIZE(wall->flags3);

		DESERIALIZE(wall->worldPos0);
		DESERIALIZE(wall->wallLight);
		DESERIALIZE(wall->angle);
	}
}
