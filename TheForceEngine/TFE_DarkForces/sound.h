#pragma once
//////////////////////////////////////////////////////////////////////
// Jedi Sound System
// Derived from the DOS Dark Forces code.
// This system uses the TFE Framework low level sound system to
// actually manage sounds, but this system is used as a front end to
// ensure the behavior matches the original game.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_Jedi/Math/core_math.h>

using namespace TFE_Jedi;

typedef s64 SoundSourceId;
typedef s64 SoundEffectId;
// Null or invalid sound ID
#define NULL_SOUND 0

enum SoundPriority
{
	SOUND_PRIORITY_LOW0 = 1,
	SOUND_PRIORITY_LOW1 = 4,
	SOUND_PRIORITY_LOW2 = 8,
	SOUND_PRIORITY_LOW3 = 12,
	SOUND_PRIORITY_LOW4 = 16,
	SOUND_PRIORITY_LOW5 = 20,

	SOUND_PRIORITY_MED0 = 64,
	SOUND_PRIORITY_MED1 = 68,
	SOUND_PRIORITY_MED2 = 72,
	SOUND_PRIORITY_MED3 = 76,
	SOUND_PRIORITY_MED4 = 80,
	SOUND_PRIORITY_MED5 = 84,

	SOUND_PRIORITY_HIGH0 = 107,
	SOUND_PRIORITY_HIGH1 = 111,
	SOUND_PRIORITY_HIGH2 = 115,
	SOUND_PRIORITY_HIGH3 = 119,
	SOUND_PRIORITY_HIGH4 = 123,
	SOUND_PRIORITY_HIGH5 = 127,
};

namespace TFE_DarkForces
{
	void sound_open(MemoryRegion* memRegion);
	void sound_close();

	void sound_levelStart();
	void sound_levelStop();

	SoundEffectId sound_play(SoundSourceId sourceId);
	SoundEffectId sound_playPriority(SoundSourceId id, s32 priority);
	SoundEffectId sound_playCued(SoundSourceId id, vec3_fixed pos);
	SoundEffectId sound_maintain(SoundEffectId idInstance, SoundSourceId idSound, vec3_fixed pos);
	void sound_adjustCued(SoundEffectId id, vec3_fixed pos);
	// Stop a sound effect that is currently playing.
	void sound_stop(SoundEffectId sourceId);
	void sound_stopAll();

	// Load a sound source from disk.
	SoundSourceId sound_load(const char* sound, u32 priority = SOUND_PRIORITY_MED0);
	void sound_free(SoundSourceId id);
	// Change the sound source base volume, range: [0, 127]
	void sound_setBaseVolume(SoundSourceId soundId, s32 volume);
	// Change the sound effect volume.
	void sound_setVolume(SoundEffectId soundId, s32 volume);
	// Change the sound effect pan.
	void sound_setPan(SoundEffectId soundId, s32 pan);
	
	void sound_pitchShift(SoundEffectId soundId, s32 shift);

	// Get the sound data index from the id.
	s32 sound_getIndexFromId(SoundSourceId id);
	SoundSourceId sound_getSoundFromIndex(s32 index, bool refCount = true);
}  // namespace TFE_Jedi