#pragma once
#include <DXL2_System/types.h>
#include <DXL2_FileSystem/paths.h>

enum SoundDataType
{
	SOUND_DATA_8BIT = 0,
	SOUND_DATA_16BIT,
	SOUND_DATA_FLOAT,
};

// Optional flags
enum SoundBufferFlags
{
	SBUFFER_FLAG_NONE = 0,
	SBUFFER_FLAG_LOOPING = (1 << 0),
};

struct SoundBuffer
{
	SoundDataType type;
	u32 flags;
	u32 size;
	u32 sampleRate;
	u32 loopStart;
	u32 loopEnd;

	u8* data;
};

enum SoundType
{
	SOUND_2D = 0,
	SOUND_3D,
};

struct SoundSource;

namespace DXL2_Audio
{
	bool init();
	void shutdown();
	void stopAllSounds();

	// For positional audio, a listener is required.
	void setListenerPosition(const Vec3f* pos);

	// One shot, play and forget. Only do this if the client needs no control until stopAllSounds() is called.
	// Note that looping one shots are valid.
	bool playOneShot(SoundType type, f32 volume, const SoundBuffer* buffer, bool looping, const Vec3f* pos = nullptr);

	// Sound source that the client holds onto.
	SoundSource* createSoundSource(SoundType type, f32 volume, const SoundBuffer* buffer, const Vec3f* pos = nullptr);
	void playSource(SoundSource* source, bool looping = false);
	void stopSource(SoundSource* source);
	void freeSource(SoundSource* source);
	void setSourceVolume(SoundSource* source, f32 volume);
	// This will restart the sound and change the buffer.
	void setSourceBuffer(SoundSource* source, const SoundBuffer* buffer);

	bool isSourcePlaying(SoundSource* source);
	f32  getSourceVolume(SoundSource* source);
}
