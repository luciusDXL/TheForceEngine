#pragma once
#include <DXL2_System/types.h>
#include <DXL2_FileSystem/paths.h>

// Source data type. The sound data will be converted to float time during mixing.
enum SoundDataType
{
	SOUND_DATA_8BIT = 0,
	SOUND_DATA_16BIT,
	SOUND_DATA_FLOAT,
};

// Optional sound buffer flags, not used by the audio system directly but may be set by the assets - 
// VOC files have loops points embedded for example.
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

struct SoundSource;
// Type of sound effect, used for sources.
// This determines how source simulation is handled.
enum SoundType
{
	SOUND_2D = 0,	// Mono 2D sound effect.
	SOUND_3D,		// 3D positional sound effect.
};

#define MONO_SEPERATION 0.5f

namespace DXL2_Audio
{
	bool init();
	void shutdown();
	void stopAllSounds();

	// Update position audio and other audio effects.
	void update(const Vec3f* listenerPos, const Vec3f* listenerDir);

	// One shot, play and forget. Only do this if the client needs no control until stopAllSounds() is called.
	// Note that looping one shots are valid though may generate too many sound sources if not used carefully.
	bool playOneShot(SoundType type, f32 volume, f32 stereoSeperation, const SoundBuffer* buffer, bool looping, const Vec3f* pos = nullptr, bool copyPosition = false);

	// Sound source that the client holds onto.
	SoundSource* createSoundSource(SoundType type, f32 volume, f32 stereoSeperation, const SoundBuffer* buffer, const Vec3f* pos = nullptr);
	void playSource(SoundSource* source, bool looping = false);
	void stopSource(SoundSource* source);
	void freeSource(SoundSource* source);
	void setSourceVolume(SoundSource* source, f32 volume);
	void setSourceStereoSeperation(SoundSource* source, f32 stereoSeperation);
	// This will restart the sound and change the buffer.
	void setSourceBuffer(SoundSource* source, const SoundBuffer* buffer);

	bool isSourcePlaying(SoundSource* source);
	f32  getSourceVolume(SoundSource* source);
}
