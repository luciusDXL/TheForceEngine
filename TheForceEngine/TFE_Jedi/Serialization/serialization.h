#pragma once
//////////////////////////////////////////////////////////////////////
// Serialization
// This was added for TFE and is not directly based on
// reverse-engineered code. However, it was built to handle JEDI and
// Dark Forces types.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/stream.h>
#include <TFE_DarkForces/sound.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rtexture.h>

struct AnimatedTexture;

namespace TFE_Jedi
{
	// This will generate an signed 32-bit index from a pointer, given a base pointer (start of an array, for example) and size of each element.
	// Note this will produce invalid results if index > INT_MAX (~2 billion).
	#define PTR_INDEX_S32(ptr, base, stride)   s32((ptrdiff_t(ptr) - ptrdiff_t(base)) / stride)
	#define INDEX_PTR_S32(index, base, stride) ((u8*)base + index * stride)

	#define SERIALIZE(x) stream->writeBuffer(&x, sizeof(x))
	#define SERIALIZE_BUF(x, s) stream->writeBuffer(x, s)

	#define DESERIALIZE(x) stream->readBuffer(&x, sizeof(x))
	#define DESERIALIZE_BUF(x, s) stream->readBuffer(x, s)
		
	void serialization_writeDfSound(Stream* stream, SoundSourceId id);
	void serialization_readDfSound(Stream* stream, SoundSourceId* id);

	void serialization_writeSectorPtr(Stream* stream, RSector* sector);
	RSector* serialization_readSectorPtr(Stream* stream);

	void serialize_writeAnimatedTexturePtr(Stream* stream, AnimatedTexture* animTex);
	AnimatedTexture* serialize_readAnimatedTexturePtr(Stream* stream);

	void serialize_writeTexturePtr(Stream* stream, TextureData* texture);
	TextureData* serialize_readTexturePtr(Stream* stream);

	void serialize_write3doPtr(Stream* stream, JediModel* model);
	JediModel* serialize_read3doPtr(Stream* stream);

	void serialize_writeWaxPtr(Stream* stream, JediWax* wax);
	JediWax* serialize_readWaxPtr(Stream* stream);

	void serialize_writeFramePtr(Stream* stream, JediFrame* frame);
	JediFrame* serialize_readFramePtr(Stream* stream);
}