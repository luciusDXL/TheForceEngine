#include "soundSystem.h"

namespace TFE_Jedi
{
	SoundEffectID playSound2D(SoundSourceID soundId)
	{
		// STUB
		return SoundEffectID(0);
	}

	void playSound3D_oneshot(SoundSourceID soundId, vec3_fixed pos)
	{
		// STUB
	}

	SoundEffectID playSound3D_looping(SoundSourceID sourceId, SoundEffectID soundId, vec3_fixed pos)
	{
		// STUB
		return SoundEffectID(0);
	}

	void stopSound(SoundEffectID sourceId)
	{
		// STUB
	}

	SoundSourceID sound_Load(const char* sound)
	{
		// STUB
		return SoundSourceID(0);
	}

	void sound_pitchShift(SoundEffectID soundId, s32 shift)
	{
		// STUB
	}

	void setSoundSourceVolume(SoundSourceID soundId, s32 volume)
	{
		// STUB
	}
}  // TFE_JediSound