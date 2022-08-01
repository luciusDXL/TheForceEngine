#include <cstring>

#include "serialization.h"
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
		return sectorIndex < 0 ? nullptr : &s_sectors[sectorIndex];
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
}