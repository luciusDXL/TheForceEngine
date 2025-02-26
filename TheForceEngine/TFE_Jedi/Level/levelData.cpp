#include <climits>
#include <cstring>

#include "levelData.h"
#include "rsector.h"
#include "rwall.h"
#include "robjData.h"
#include <TFE_Game/igame.h>
#include <TFE_System/system.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_Jedi/Serialization/serialization.h>

// TODO: coupling between Dark Forces and Jedi.
using namespace TFE_DarkForces;
#include <TFE_DarkForces/projectile.h>

namespace TFE_DarkForces
{
	extern s32 s_secretsFound;
	extern s32 s_secretsPercent;
}

namespace TFE_Jedi
{
	enum LevelStateVersion : u32
	{
		LevelState_InitVersion = 1,
		LevelState_CurVersion = LevelState_InitVersion,
	};
	enum LevelTextureType : u32
	{
		LEVTEX_TYPE_TEX = 1,
		LEVTEX_TYPE_ANM = 2,
	};

	LevelState s_levelState = {};
	LevelInternalState s_levelIntState = {};
	
	void level_serializeSector(Stream* stream, RSector* sector);
	void level_serializeSafe(Stream* stream, Safe* safe);
	void level_serializeAmbientSound(Stream* stream, AmbientSound* sound);
	void level_serializeWall(Stream* stream, RWall* wall, RSector* sector);
	void level_serializeTextureList(Stream* stream);

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

	void level_serializeFixupMirrors()
	{
		RSector* sector = s_levelState.sectors;
		for (u32 s = 0; s < s_levelState.sectorCount; s++, sector++)
		{
			s32 wallCount = sector->wallCount;
			RWall* wall = sector->walls;
			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				if (wall->nextSector && wall->mirror >= 0)
				{
					wall->mirrorWall = &wall->nextSector->walls[wall->mirror];
				}
			}
		}
	}

	void level_serializePalette(Stream* stream)
	{
		u8 length = 0;
		if (serialization_getMode() == SMODE_WRITE)
		{
			length = (u8)strlen(s_levelState.levelPaletteName);
		}
		SERIALIZE(SaveVersionInit, length, 0);
		SERIALIZE_BUF(SaveVersionInit, s_levelState.levelPaletteName, length);
		s_levelState.levelPaletteName[length] = 0;

		if (serialization_getMode() == SMODE_READ)
		{
			level_loadPalette();
		}
	}

	void level_updateSecretPercent()
	{
		if (s_levelState.secretCount)
		{
			// 100.0 * found / count
			fixed16_16 percentage = mul16(FIXED(100), div16(intToFixed16(s_secretsFound), intToFixed16(s_levelState.secretCount)));
			s_secretsPercent = floor16(percentage);
		}
		else
		{
			s_secretsPercent = 100;
		}
		s_secretsPercent = max(0, min(100, s_secretsPercent));
	}
		
	void level_serialize(Stream* stream)
	{
		bool debug = serialization_getMode() == SMODE_WRITE;

		if (serialization_getMode() == SMODE_READ)
		{
			level_clearData();
		}
		SERIALIZE_VERSION(LevelState_CurVersion);
		
		SERIALIZE(LevelState_InitVersion, s_levelState.minLayer, 0);
		SERIALIZE(LevelState_InitVersion, s_levelState.maxLayer, 0);
		SERIALIZE(LevelState_InitVersion, s_levelState.secretCount, 0);
		SERIALIZE(LevelState_InitVersion, s_levelState.sectorCount, 0);
		SERIALIZE(LevelState_InitVersion, s_levelState.parallax0, 0);
		SERIALIZE(LevelState_InitVersion, s_levelState.parallax1, 0);
		SERIALIZE(LevelState_InitVersion, TFE_DarkForces::s_secretsFound, 0);

		SERIALIZE_BUF(LevelState_InitVersion, s_levelState.complete, sizeof(JBool) * 2 * NUM_COMPLETE);
		SERIALIZE_BUF(LevelState_InitVersion, s_levelState.completeNum, sizeof(s32) * 2 * NUM_COMPLETE);

		/////////////////////////////////////
		// Serialize asset names
		/////////////////////////////////////
		level_serializePalette(stream);
		bitmap_serializeLevelTextures(stream);
		TFE_Sprite_Jedi::sprite_serializeSpritesAndFrames(stream);
		TFE_Model_Jedi::serializeModels(stream);

		// This is needed because level textures are pointers to the list itself, rather than the texture.
		level_serializeTextureList(stream);
				
		if (serialization_getMode() == SMODE_READ)
		{
			s_levelState.sectors = (RSector*)level_alloc(sizeof(RSector) * s_levelState.sectorCount);
			s_levelState.controlSector->id = s_levelState.sectorCount;
			s_levelState.controlSector->index = s_levelState.controlSector->id;

			level_updateSecretPercent();
		}
		RSector* sector = s_levelState.sectors;
		for (u32 s = 0; s < s_levelState.sectorCount; s++, sector++)
		{
			level_serializeSector(stream, sector);
		}

		serialization_serializeSectorPtr(stream, LevelState_InitVersion, s_levelState.bossSector);
		serialization_serializeSectorPtr(stream, LevelState_InitVersion, s_levelState.mohcSector);
		serialization_serializeSectorPtr(stream, LevelState_InitVersion, s_levelState.completeSector);

		// The control sector is just a dummy sector, no need to save anything.
		// Sound emitter isn't actually used.

		if (serialization_getMode() == SMODE_WRITE)
		{
			allocator_saveIter(s_levelState.safeLoc);
				s32 safeCount = allocator_getCount(s_levelState.safeLoc);
				SERIALIZE(LevelState_InitVersion, safeCount, 0);
				Safe* safe = (Safe*)allocator_getHead(s_levelState.safeLoc);
				while (safe)
				{
					level_serializeSafe(stream, safe);
					safe = (Safe*)allocator_getNext(s_levelState.safeLoc);
				}
			allocator_restoreIter(s_levelState.safeLoc);

			allocator_saveIter(s_levelState.ambientSounds);
				s32 ambientSoundCount = allocator_getCount(s_levelState.ambientSounds);
				SERIALIZE(LevelState_InitVersion, ambientSoundCount, 0);
				AmbientSound* sound = (AmbientSound*)allocator_getHead(s_levelState.ambientSounds);
				while (sound)
				{
					level_serializeAmbientSound(stream, sound);
					sound = (AmbientSound*)allocator_getNext(s_levelState.ambientSounds);
				}
			allocator_restoreIter(s_levelState.ambientSounds);
		}
		else
		{
			s32 safeCount;
			SERIALIZE(LevelState_InitVersion, safeCount, 0);
			s_levelState.safeLoc = nullptr;
			if (safeCount)
			{
				s_levelState.safeLoc = allocator_create(sizeof(Safe));
				for (s32 s = 0; s < safeCount; s++)
				{
					Safe* safe = (Safe*)allocator_newItem(s_levelState.safeLoc);
					if (!safe)
						return;
					level_serializeSafe(stream, safe);
				}
			}

			s32 ambientSoundCount;
			SERIALIZE(LevelState_InitVersion, ambientSoundCount, 0);
			s_levelState.ambientSounds = nullptr;
			if (ambientSoundCount)
			{
				s_levelState.ambientSounds = allocator_create(sizeof(AmbientSound));
				for (s32 s = 0; s < ambientSoundCount; s++)
				{
					AmbientSound* sound = (AmbientSound*)allocator_newItem(s_levelState.ambientSounds);
					if (!sound)
						return;
					level_serializeAmbientSound(stream, sound);
				}
				if (!s_levelIntState.ambientSoundTask)
				{
					s_levelIntState.ambientSoundTask = createSubTask("AmbientSound", ambientSoundTaskFunc);
				}
			}

			level_serializeFixupMirrors();
		}

		// Serialize objects.
		objData_serialize(stream);
	}
		
	/////////////////////////////////////////////
	// Internal - Serialize
	/////////////////////////////////////////////
	void level_serializeTextureList(Stream* stream)
	{
		const bool read = serialization_getMode() == SMODE_READ;

		SERIALIZE(LevelState_InitVersion, s_levelState.textureCount, 0);
		if (read)
		{
			s_levelState.textures = (TextureData**)level_alloc(2 * s_levelState.textureCount * sizeof(TextureData**));
		}
		TextureData** textures = s_levelState.textures;
		TextureData** texBase = textures + s_levelState.textureCount;
		for (s32 i = 0; i < s_levelState.textureCount; i++)
		{
			serialization_serializeTexturePtr(stream, LevelState_InitVersion, texBase[i]);
			// texBase[] remains unmodified for serialization, whereas textures[] can be modified with animated textures.
			if (read)
			{
				if (texBase[i]) { texBase[i]->flags |= ENABLE_MIP_MAPS; }
				textures[i] = texBase[i];
			}
			if (read && s_levelState.textures[i] && s_levelState.textures[i]->uvWidth == BM_ANIMATED_TEXTURE)
			{
				bitmap_setupAnimatedTexture(&s_levelState.textures[i], i);
			}
		}
	}
		
	void level_serializeTexturePointer(Stream* stream, TextureData**& texData)
	{
		u32 texIndex = 0u;
		if (serialization_getMode() == SMODE_WRITE && texData && *texData)
		{
			if ((*texData)->animIndex >= 0)
			{
				const u8 frameIndex = (*texData)->frameIdx < 0 ? 0xff : (u8)(*texData)->frameIdx;
				texIndex = LEVTEX_TYPE_ANM | (frameIndex << 4) | ((u32)(*texData)->animIndex << 12u);
			}
			else
			{
				std::ptrdiff_t offset = (std::ptrdiff_t(texData) - std::ptrdiff_t(s_levelState.textures)) / (std::ptrdiff_t)sizeof(TextureData**);
				if (offset >= 0 && offset < s_levelState.textureCount)
				{
					texIndex = LEVTEX_TYPE_TEX | ((u32)offset << 12u);
				}
				assert(texIndex && s_levelState.textures[offset] == *texData);
			}
		}
		SERIALIZE(LevelState_InitVersion, texIndex, 0u);

		if (serialization_getMode() == SMODE_READ)
		{
			s32 index = texIndex >> 12;
			s32 frameIdx = (texIndex >> 4) & 255;
			if (frameIdx == 0xff) { frameIdx = -1; }

			assert(index >= 0 && index < s_levelState.textureCount);
			texData = (texIndex == 0) ? nullptr : &s_levelState.textures[index];
			if (texData && *texData)
			{
				if ((texIndex & 3) == LEVTEX_TYPE_ANM && frameIdx >= 0)
				{
					AnimatedTexture* anim = (AnimatedTexture*)(*texData)->animPtr;
					if (anim)
					{
						(*texData) = anim->frameList[frameIdx];
					}
				}
				(*texData)->animIndex = ((texIndex & 3) == LEVTEX_TYPE_ANM) ? index : -1;
				(*texData)->frameIdx = frameIdx;
			}
		}
	}

	void level_serializeSector(Stream* stream, RSector* sector)
	{
		if (serialization_getMode() == SMODE_READ)
		{
			sector->self = sector;
		}
		SERIALIZE(LevelState_InitVersion, sector->id, 0);
		SERIALIZE(LevelState_InitVersion, sector->index, 0);
		SERIALIZE(LevelState_InitVersion, sector->prevDrawFrame, 0);
		SERIALIZE(LevelState_InitVersion, sector->prevDrawFrame2, 0);

		SERIALIZE(LevelState_InitVersion, sector->vertexCount, 0);
		const size_t vtxSize = sector->vertexCount * sizeof(vec2_fixed);
		if (serialization_getMode() == SMODE_READ)
		{
			sector->verticesWS = (vec2_fixed*)level_alloc(vtxSize);
			sector->verticesVS = (vec2_fixed*)level_alloc(vtxSize);
		}
		SERIALIZE_BUF(LevelState_InitVersion, sector->verticesWS, u32(vtxSize));
		// view space vertices don't need to be serialized.

		SERIALIZE(LevelState_InitVersion, sector->wallCount, 0);
		if (serialization_getMode() == SMODE_READ)
		{
			sector->walls = (RWall*)level_alloc(sector->wallCount * sizeof(RWall));
		}
		RWall* wall = sector->walls;
		for (s32 w = 0; w < sector->wallCount; w++, wall++)
		{
			level_serializeWall(stream, wall, sector);
		}

		SERIALIZE(LevelState_InitVersion, sector->floorHeight, 0);
		SERIALIZE(LevelState_InitVersion, sector->ceilingHeight, 0);
		SERIALIZE(LevelState_InitVersion, sector->secHeight, 0);
		SERIALIZE(LevelState_InitVersion, sector->ambient, 0);

		SERIALIZE(LevelState_InitVersion, sector->colFloorHeight, 0);
		SERIALIZE(LevelState_InitVersion, sector->colCeilHeight, 0);
		SERIALIZE(LevelState_InitVersion, sector->colSecHeight, 0);
		SERIALIZE(LevelState_InitVersion, sector->colSecCeilHeight, 0);

		// infLink will be set when the INF system is serialized.
		if (serialization_getMode() == SMODE_READ)
		{
			sector->infLink = nullptr;
		}

		level_serializeTexturePointer(stream, sector->floorTex);
		level_serializeTexturePointer(stream, sector->ceilTex);

		const vec2_fixed def = { 0,0 };
		SERIALIZE(LevelState_InitVersion, sector->floorOffset, def);
		SERIALIZE(LevelState_InitVersion, sector->ceilOffset, def);

		// Objects are handled seperately and added to the sector later, so just initialize the object data.
		if (serialization_getMode() == SMODE_READ)
		{
			sector->objectCount = 0;
			sector->objectCapacity = 0;
			sector->objectList = nullptr;
			sector->searchKey = 0;
		}

		SERIALIZE(LevelState_InitVersion, sector->collisionFrame, 0);
		SERIALIZE(LevelState_InitVersion, sector->startWall, 0);
		SERIALIZE(LevelState_InitVersion, sector->drawWallCnt, 0);
		SERIALIZE(LevelState_InitVersion, sector->flags1, 0);
		SERIALIZE(LevelState_InitVersion, sector->flags2, 0);
		SERIALIZE(LevelState_InitVersion, sector->flags3, 0);
		SERIALIZE(LevelState_InitVersion, sector->layer, 0);
		SERIALIZE(LevelState_InitVersion, sector->boundsMin, def);
		SERIALIZE(LevelState_InitVersion, sector->boundsMax, def);

		// dirty flags will be set on deserialization.
		if (serialization_getMode() == SMODE_READ)
		{
			sector->dirtyFlags = SDF_ALL;
		}
	}
		
	void level_serializeSafe(Stream* stream, Safe* safe)
	{
		serialization_serializeSectorPtr(stream, LevelState_InitVersion, safe->sector);
		SERIALIZE(LevelState_InitVersion, safe->x, 0);
		SERIALIZE(LevelState_InitVersion, safe->z, 0);
		SERIALIZE(LevelState_InitVersion, safe->yaw, 0);
		SERIALIZE(LevelState_InitVersion, safe->pad, 0);	// for padding.
	}

	void level_serializeAmbientSound(Stream* stream, AmbientSound* sound)
	{
		serialization_serializeDfSound(stream, LevelState_InitVersion, &sound->soundId);
		if (serialization_getMode() == SMODE_READ)
		{
			sound->instanceId = 0;
		}
		const vec3_fixed def = { 0,0,0 };
		SERIALIZE(LevelState_InitVersion, sound->pos, def);
	}
				
	void level_serializeWall(Stream* stream, RWall* wall, RSector* sector)
	{
		SERIALIZE(LevelState_InitVersion, wall->id, 0);
		SERIALIZE(LevelState_InitVersion, wall->seen, 0);
		SERIALIZE(LevelState_InitVersion, wall->visible, 0);

		serialization_serializeSectorPtr(stream, LevelState_InitVersion, wall->sector);
		serialization_serializeSectorPtr(stream, LevelState_InitVersion, wall->nextSector);

		// mirrorWall will be computed from mirror.
		SERIALIZE(LevelState_InitVersion, wall->drawFlags, 0);
		SERIALIZE(LevelState_InitVersion, wall->mirror, -1);
		if (serialization_getMode() == SMODE_READ)
		{
			wall->mirrorWall = nullptr;
			// Fix-up next sector if needed.
			if (wall->mirror < 0 && wall->nextSector)
			{
				TFE_System::logWrite(LOG_WARNING, "LevelData",
					"Wall %d of sector %d has 'nextSector = %d' but no 'mirror' - clearing out the next sector.", wall->id, sector->id, wall->nextSector->id);
				wall->nextSector = nullptr;
			}
		}
		
		// worldspace vertices.
		s32 i0, i1;
		if (serialization_getMode() == SMODE_WRITE)
		{
			i0 = PTR_INDEX_S32(wall->w0, sector->verticesWS, sizeof(vec2_fixed));
			i1 = PTR_INDEX_S32(wall->w1, sector->verticesWS, sizeof(vec2_fixed));
			SERIALIZE(LevelState_InitVersion, i0, 0);
			SERIALIZE(LevelState_InitVersion, i1, 0);
			// viewspace vertices need not be saved.
		}
		else
		{
			SERIALIZE(LevelState_InitVersion, i0, 0);
			SERIALIZE(LevelState_InitVersion, i1, 0);
			wall->w0 = &sector->verticesWS[i0];
			wall->w1 = &sector->verticesWS[i1];
			wall->v0 = &sector->verticesVS[i0];
			wall->v1 = &sector->verticesVS[i1];
		}

		level_serializeTexturePointer(stream, wall->topTex);
		level_serializeTexturePointer(stream, wall->midTex);
		level_serializeTexturePointer(stream, wall->botTex);
		level_serializeTexturePointer(stream, wall->signTex);

		SERIALIZE(LevelState_InitVersion, wall->texelLength, 0);
		SERIALIZE(LevelState_InitVersion, wall->topTexelHeight, 0);
		SERIALIZE(LevelState_InitVersion, wall->midTexelHeight, 0);
		SERIALIZE(LevelState_InitVersion, wall->botTexelHeight, 0);

		const vec2_fixed def = { 0, 0 };
		SERIALIZE(LevelState_InitVersion, wall->topOffset, def);
		SERIALIZE(LevelState_InitVersion, wall->midOffset, def);
		SERIALIZE(LevelState_InitVersion, wall->botOffset, def);
		SERIALIZE(LevelState_InitVersion, wall->signOffset, def);

		SERIALIZE(LevelState_InitVersion, wall->wallDir, def);
		SERIALIZE(LevelState_InitVersion, wall->length, 0);
		SERIALIZE(LevelState_InitVersion, wall->collisionFrame, 0);
		SERIALIZE(LevelState_InitVersion, wall->drawFrame, 0);

		// infLink will be filled in when serializing the INF system.
		if (serialization_getMode() == SMODE_READ)
		{
			wall->infLink = nullptr;
		}

		SERIALIZE(LevelState_InitVersion, wall->flags1, 0);
		SERIALIZE(LevelState_InitVersion, wall->flags2, 0);
		SERIALIZE(LevelState_InitVersion, wall->flags3, 0);

		SERIALIZE(LevelState_InitVersion, wall->worldPos0, def);
		SERIALIZE(LevelState_InitVersion, wall->wallLight, 0);
		SERIALIZE(LevelState_InitVersion, wall->angle, 0);
	}
}
