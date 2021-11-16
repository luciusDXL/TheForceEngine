#pragma once
//////////////////////////////////////////////////////////////////////
// Jedi Sound System
// Derived from the DOS Dark Forces code.
// This system uses the TFE Framework low level sound system to
// actually manage sounds, but this system is used as a front end to
// ensure the behavior matches the original game.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/core_math.h>

// Sound Effect ID
typedef u32 SoundEffectID;
// Sound Source Data ID
typedef s32 SoundSourceID;
// Null or invalid sound ID
#define NULL_SOUND 0

namespace TFE_Jedi
{
	void sound_stopAll();
	void sound_freeAll();

	// Play a one-shot 2D sound effect from a Sound Source.
	SoundEffectID playSound2D(SoundSourceID soundId);
	SoundEffectID playSound2D_looping(SoundSourceID soundId);
	// Play a one-shot 3D sound effect from a Sound Source at 'pos'.
	SoundEffectID playSound3D_oneshot(SoundSourceID soundId, vec3_fixed pos);
	// Play a looping 3D sound effect from a Sound Source (sourceID) at 'pos'.
	// Pass in the previous sound effect ID to keep using it and it returns the new sound effect ID (if it changes).
	// Note this function should be called whenever 'pos' needs to be updated if the sound is moving.
	SoundEffectID playSound3D_looping(SoundSourceID sourceId, SoundEffectID soundId, vec3_fixed pos);

	// Stop a sound effect that is currently playing.
	void stopSound(SoundEffectID sourceId);

	// Load a sound source from disk.
	SoundSourceID sound_Load(const char* sound);
	// Change the sound source base volume, range: [0, 127]
	void setSoundSourceVolume(SoundSourceID soundId, s32 volume);

	void sound_pitchShift(SoundEffectID soundId, s32 shift);
}  // namespace TFE_Jedi