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

namespace TFE_Jedi
{
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
}