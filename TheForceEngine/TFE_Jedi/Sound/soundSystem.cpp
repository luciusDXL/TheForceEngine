#include "soundSystem.h"

namespace TFE_JediSound
{
	void playSound2D(SoundSourceID soundId)
	{
		// STUB
	}

	void playSound3D_oneshot(SoundSourceID soundId, TFE_CoreMath::vec3_fixed pos)
	{
		// STUB
	}

	SoundEffectID playSound3D_looping(SoundSourceID sourceId, SoundEffectID soundId, TFE_CoreMath::vec3_fixed pos)
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

	void setSoundSourceVolume(SoundSourceID soundId, s32 volume)
	{
		// STUB
	}
}  // TFE_JediSound