#pragma once
//////////////////////////////////////////////////////////////////////
// Jedi Sound System
// Derived from the DOS Dark Forces code.
// This system uses the TFE Framework low level sound system to
// actually manage sounds, but this system is used as a front end to
// ensure the behavior matches the original game.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Level/core_math.h>

namespace TFE_JediSound
{
	void playSound2D(s32 soundId);
	void playSound3D_oneshot(s32 soundId, TFE_CoreMath::vec3_fixed pos);
	s32  playSound3D_looping(s32 sourceId, s32 soundId, TFE_CoreMath::vec3_fixed pos);
	void stopSound(s32 sourceId);
	s32  sound_Load(const char* sound);
	void setSoundEffectVolume(s32 soundId, s32 volume);
}  // namespace TFE_DarkForces