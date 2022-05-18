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

enum LSoundConst
{
	LS_NAME_LEN   = 8,      // Sound name length
	LS_MAX_SOUNDS = 48,		// Maximum number of sounds
};

enum LSoundFlags
{
	LS_SOUND_FREE       = 0x0001,		// Free the sound after user function
	LS_SOUND_DISCARD    = 0x0002,		// Sound data is discardable
	LS_SOUND_KEEP_USER  = 0x0004,		// Don't NULL user function
	LS_SOUND_KEEP       = 0x1000,		// Keep the sound for one free
	LS_SOUND_KEEPABLE   = 0x2000,		// Keep the sound
	LS_SOUND_USER_FLAG1 = 0x4000,		// User flag 1
	LS_SOUND_USER_FLAG2 = 0x8000,		// User flag 2
};

// In practice, only digitalSound is used but I'm keeping the code the same for now.
enum LSoundType
{
	noSound = 0,
	gmidiSound,
	digitalSound
};

// Structure padding matters due to the hacky way iMuse gets at the sound data,
// so I promoted 16-bit members to 32-bit to simplify.
struct LSound
{
	u32 resType;
	char name[LS_NAME_LEN];

	LSound* next;
	LSoundType type;
	s32 id;

	s32 flags;
	s32 volume;

	s32 var1;
	s32 var2;
	u8* varptr;
	u8* varhdl;

	u8* data;
};

namespace TFE_DarkForces
{
	// System
	void lSoundInit();
	void lSoundDestroy();
	LSound* lSoundGetList();
	LSound* lSoundAlloc(u8* data);
	LSound* lSoundLoad(const char* name, LSoundType soundType);
	void lSoundFree(LSound* sound);
	void lSoundFreePlaying(LSound* sound, JBool stop);
	void freeSoundList(LSound* sound);

	// Landru Sound <-> iMuse interface.
	void startSfx(LSound* sound);
	void startSpeech(LSound* sound);
	void stopSound(LSound* sound);
	void setSoundVolume(LSound* sound, s32 volume);
	void setSoundFade(LSound* sound, s32 volume, s32 time);
	void setSoundPan(LSound* sound, s32 pan);
	void setSoundPanFade(LSound* sound, s32 pan, s32 time);

	// Landru Sound only.
	void copySoundData(LSound* dstSound, LSound* srcSound);
	LSound* findSoundType(const char* name, u32 type);
	void  discardSoundData(LSound* sound);
	void  nonDiscardSoundData(LSound* sound);
	JBool isDiscardSoundData(LSound* sound);
	void  setSoundKeep(LSound* sound);
	void  clearSoundKeep(LSound* sound);
	JBool isSoundKeep(LSound* sound);
	void  setSoundKeepable(LSound* sound);
	void  clearSoundKeepable(LSound* sound);
	JBool isSoundKeepable(LSound* sound);
	void  setSoundName(LSound* sound, u32 type, const char* name);

	u8* readVocFileData(const char* name, u32* size = nullptr);
}  // namespace TFE_Jedi