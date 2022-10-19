#include <cstring>

#include "serialization.h"
#include <TFE_Jedi/Level/levelData.h>
#include <TFE_System/system.h>

using namespace TFE_DarkForces;
using namespace TFE_Memory;

namespace TFE_Jedi
{
	void serialization_writeDfSound(Stream* stream, SoundSourceId id)
	{
		s32 soundDataId = sound_getIndexFromId(id);
		SERIALIZE(soundDataId);
	}

	void serialization_readDfSound(Stream* stream, SoundSourceId* id)
	{
		s32 soundDataId;
		DESERIALIZE(soundDataId);
		*id = (soundDataId < 0) ? NULL_SOUND : sound_getSoundFromIndex(soundDataId, false);
	}
		
	void serialization_writeSectorPtr(Stream* stream, RSector* sector)
	{
		s32 sectorIndex = sector ? sector->index : -1;
		SERIALIZE(sectorIndex);
	}

	RSector* serialization_readSectorPtr(Stream* stream)
	{
		s32 sectorIndex;
		DESERIALIZE(sectorIndex);
		return sectorIndex < 0 ? nullptr : &s_levelState.sectors[sectorIndex];
	}
		
	void serialize_writeAnimatedTexturePtr(Stream* stream, AnimatedTexture* animTex)
	{
		s32 index = -1;
		Allocator* animTexAlloc = bitmap_getAnimTextureAlloc();
		if (animTex && animTexAlloc)
		{
			index = allocator_getIndex(animTexAlloc, animTex);
		}
		SERIALIZE(index);
	}

	AnimatedTexture* serialize_readAnimatedTexturePtr(Stream* stream)
	{
		s32 index;
		DESERIALIZE(index);

		AnimatedTexture* animTex = nullptr;
		Allocator* animTexAlloc = bitmap_getAnimTextureAlloc();
		if (animTexAlloc && index >= 0)
		{
			animTex = (AnimatedTexture*)allocator_getByIndex(animTexAlloc, index);
		}
		return animTex;
	}
	
	void serialize_writeTexturePtr(Stream* stream, TextureData* texture)
	{
		if (!texture)
		{
			s32 id = -1;
			SERIALIZE(id);
			return;
		}

		s32 index;
		AssetPool pool;
		bool foundTex = bitmap_getTextureIndex(texture, &index, &pool);

		s32 id = (foundTex) ? (index | (pool << 16)) : -1;
		SERIALIZE(id);
	}

	TextureData* serialize_readTexturePtr(Stream* stream)
	{
		s32 id;
		DESERIALIZE(id);
		if (id < 0)
		{
			return nullptr;
		}
		return bitmap_getTextureByIndex(id & 0xffff, AssetPool(id >> 16));
	}
		
	void serialize_write3doPtr(Stream* stream, JediModel* model)
	{
		if (!model)
		{
			s32 id = -1;
			SERIALIZE(id);
			return;
		}

		s32 index;
		AssetPool pool;
		bool foundModel = TFE_Model_Jedi::getModelIndex(model, &index, &pool);

		s32 id = (foundModel) ? (index | (pool << 16)) : -1;
		SERIALIZE(id);
	}

	JediModel* serialize_read3doPtr(Stream* stream)
	{
		s32 id;
		DESERIALIZE(id);
		return id < 0 ? nullptr : TFE_Model_Jedi::getModelByIndex(id & 0xffff, AssetPool(id >> 16));
	}
		
	void serialize_writeWaxPtr(Stream* stream, JediWax* wax)
	{
		if (!wax)
		{
			s32 id = -1;
			SERIALIZE(id);
			return;
		}

		s32 index;
		AssetPool pool;
		bool foundWax = TFE_Sprite_Jedi::getWaxIndex(wax, &index, &pool);

		s32 id = (foundWax) ? (index | (pool << 16)) : -1;
		SERIALIZE(id);
	}

	JediWax* serialize_readWaxPtr(Stream* stream)
	{
		s32 id;
		DESERIALIZE(id);
		return id < 0 ? nullptr : TFE_Sprite_Jedi::getWaxByIndex(id & 0xffff, AssetPool(id >> 16));
	}

	void serialize_writeFramePtr(Stream* stream, JediFrame* frame)
	{
		if (!frame)
		{
			s32 id = -1;
			SERIALIZE(id);
			return;
		}

		s32 index;
		AssetPool pool;
		bool foundFrame = TFE_Sprite_Jedi::getFrameIndex(frame, &index, &pool);

		s32 id = (foundFrame) ? (index | (pool << 16)) : -1;
		SERIALIZE(id);
	}

	JediFrame* serialize_readFramePtr(Stream* stream)
	{
		s32 id;
		DESERIALIZE(id);
		return id < 0 ? nullptr : TFE_Sprite_Jedi::getFrameByIndex(id & 0xffff, AssetPool(id >> 16));
	}
}