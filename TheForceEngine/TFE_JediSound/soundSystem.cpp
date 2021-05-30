#include "soundSystem.h"

namespace TFE_JediSound
{
	void playSound2D(s32 soundId)
	{
		// STUB
	}

	void playSound3D_oneshot(s32 soundId, TFE_CoreMath::vec3_fixed pos)
	{
		// STUB
	}

	s32 playSound3D_looping(s32 sourceId, s32 soundId, TFE_CoreMath::vec3_fixed pos)
	{
		// STUB
		return 0;
	}

	void stopSound(s32 sourceId)
	{
	}

	s32 sound_Load(const char* sound)
	{
		return 0;
	}

	void setSoundEffectVolume(s32 soundId, s32 volume)
	{
	}
}  // TFE_JediSound