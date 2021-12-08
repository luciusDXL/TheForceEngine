#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru Sound - the Landru system manages sounds differently than
// the main game.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/vocAsset.h>
#include <TFE_Audio/audioSystem.h>
#include "lsystem.h"

enum LSoundType
{
	noSound = 0,
	gmidiSound,
	digitalSound,
};

struct LSound;
typedef void(*LSoundCallback)(LSound*, u32);

struct LSound
{
	u32 resType;
	char name[16];

	LSound* next;
	LSoundType type;
	s16 id;

	s16 flags;
	s16 volume;
	s16 pan;
	s16 var1;
	s16 var2;
	u8* varPtr;
	u8* varPtr2;

	LSoundCallback callback;
	u8* data;

	// Internal, this is in iMuse in the original game.
	SoundBuffer  soundBuffer;
	SoundSource* soundSource;
};

namespace TFE_DarkForces
{
	void lsound_init();
	void lsound_destroy();

	LSound* lsound_loadMusic(const char* name);
	LSound* lsound_loadDigital(const char* name);
	LSound* lsound_loadDigitalVoice(const char* name);

	void lsound_startSpeech(LSound* sound);
	void lsound_startSfx(LSound* sound);
	void lsound_stop(LSound* sound);
	void lsound_setVolume(LSound* sound, s16 volume);
	void lsound_setFade(LSound* sound, s16 volume, s16 time);
	void lsound_setPan(LSound* sound, s16 pan);
	void lsound_setPanFade(LSound* sound, s16 pan, s16 time);
	void lsound_setKeep(LSound* sound);

	LSound* lsound_getList();
	void lsound_freeSounds(LSound* sound);

	void lsound_update();
}  // namespace TFE_DarkForces