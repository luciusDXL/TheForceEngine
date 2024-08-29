#pragma once
//////////////////////////////////////////////////////////////////////
// Serialization
// This was added for TFE and is not directly based on
// reverse-engineered code. However, it was built to handle JEDI and
// Dark Forces types.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/physfswrapper.h>
#include <TFE_DarkForces/sound.h>
#include <TFE_Jedi/Level/level.h>

struct AnimatedTexture;

namespace TFE_Jedi
{
	enum SaveVersion : u32
	{
		SaveVersionInit = 1,
		SaveVersionCur = SaveVersionInit,
	};

	enum SerializationMode
	{
		SMODE_WRITE = 0,
		SMODE_READ,
		SMODE_UNKNOWN,
	};

	extern u32 s_sVersion;
	extern SerializationMode s_sMode;

	// This will generate an signed 32-bit index from a pointer, given a base pointer (start of an array, for example) and size of each element.
	// Note this will produce invalid results if index > INT_MAX (~2 billion).
	#define PTR_INDEX_S32(ptr, base, stride)   s32((std::ptrdiff_t(ptr) - std::ptrdiff_t(base)) / stride)
	#define INDEX_PTR_S32(index, base, stride) ((u8*)base + index * stride)

	#define SERIALIZE_VERSION(curVer) \
        { \
			u32 ver = curVer; \
			if (s_sMode == SMODE_WRITE) { stream->write(&ver, sizeof(ver)); } \
			else if (s_sMode == SMODE_READ) { stream->read(&ver, sizeof(ver)); } \
			serialization_setVersion(ver); \
		}

	#define SERIALIZE(v, x, def) \
		if (s_sMode == SMODE_WRITE && s_sVersion >= v) { stream->write(&x, sizeof(x)); } \
		else if (s_sMode == SMODE_READ) \
		{ \
			if (s_sVersion >= v) { stream->read(&x, sizeof(x)); } \
			else { x = def; } \
		}
	#define SERIALIZE_BUF(v, x, s) if (s_sMode == SMODE_WRITE && s_sVersion >= v) { stream->write(x, s); } \
		else if (s_sMode == SMODE_READ) \
		{ \
			if (s_sVersion >= v) { stream->read(x, s); } \
			else { memset(x, 0, s); } \
		}

	// Discard values that were previously added. This might mean skipping over data in the stream and ignoring it.
	// This is done by advancing the stream by the size of the type or buffer.
	// v0 = version added, v1 = version removed.
	#define SERIALIZE_DISCARD(v0, v1, type)  if (s_sVersion >= v0 && s_sVersion < v1 && s_sMode == SMODE_READ) { stream->seek(nullptr, sizeof(type)); }
	#define SERIALIZE_BUF_DISCARD(v0, v1, s) if (s_sVersion >= v0 && s_sVersion < v1 && s_sMode == SMODE_READ) { stream->seek(nullptr, s); }
		
	inline void serialization_setVersion(u32 version) { s_sVersion = version; }
	inline void serialization_setMode(SerializationMode mode) { s_sMode = mode; }
	inline SerializationMode serialization_getMode() { return s_sMode; }
		
	void serialization_serializeDfSound(vpFile* stream, u32 version, SoundSourceId* id);
	void serialization_serializeSectorPtr(vpFile* stream, u32 version, RSector*& sector);
	void serialization_serializeAnimatedTexturePtr(vpFile* stream, u32 version, AnimatedTexture*& animTex);
	void serialization_serializeTexturePtr(vpFile* stream, u32 version, TextureData*& texture);
	void serialization_serialize3doPtr(vpFile* stream, u32 version, JediModel*& model);
	void serialization_serializeWaxPtr(vpFile* stream, u32 version, JediWax*& wax);
	void serialization_serializeFramePtr(vpFile* stream, u32 version, JediFrame*& frame);
}