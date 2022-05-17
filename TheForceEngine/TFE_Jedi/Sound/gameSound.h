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

enum GameSoundConst
{
	GS_NAME_LEN   = 8,      // Sound name length
	GS_MAX_SOUNDS = 48,		// Maximum number of sounds
};

enum GameSoundFlags
{
	GS_SOUND_FREE       = 0x0001,		// Free the sound after user function
	GS_SOUND_DISCARD    = 0x0002,		// Sound data is discardable
	GS_SOUND_KEEP_USER  = 0x0004,		// Don't NULL user function
	GS_SOUND_KEEP       = 0x1000,		// Keep the sound for one free
	GS_SOUND_KEEPABLE   = 0x2000,		// Keep the sound
	GS_SOUND_USER_FLAG1 = 0x4000,		// User flag 1
	GS_SOUND_USER_FLAG2 = 0x8000,		// User flag 2
};

// In practice, only digitalSound is used but I'm keeping the code the same for now.
enum GameSoundType
{
	noSound = 0,
	gmidiSound,
	digitalSound
};

// Structure padding matters due to the hacky way iMuse gets at the sound data,
// so I promoted 16-bit members to 32-bit to simplify.
struct GameSound
{
	u32 resType;
	char name[GS_NAME_LEN];

	GameSound* next;
	GameSoundType type;
	s32 id;

	s32 flags;
	s32 volume;

	s32 var1;
	s32 var2;
	u8* varptr;
	u8* varhdl;

	u8* data;
};

namespace TFE_Jedi
{
	// System
	void gameSoundInit();
	void gameSoundDestroy();
	GameSound* gameSoundGetList();
	GameSound* gameSoundAlloc(u8* data);
	GameSound* gameSoundLoad(const char* name, GameSoundType soundType);
	void gameSoundFree(GameSound* sound);
	void gameSoundFreePlaying(GameSound* sound, JBool stop);
	void freeSoundList(GameSound* sound);

	// Game Sound <-> iMuse interface.
	void startSfx(GameSound* sound);
	void startSpeech(GameSound* sound);
	void stopSound(GameSound* sound);
	void setSoundVolume(GameSound* sound, s32 volume);
	void setSoundFade(GameSound* sound, s32 volume, s32 time);
	void setSoundPan(GameSound* sound, s32 pan);
	void setSoundPanFade(GameSound* sound, s32 pan, s32 time);

	// Game Sound only.
	void copySoundData(GameSound* dstSound, GameSound* srcSound);
	GameSound* findSoundType(const char* name, u32 type);
	void  discardSoundData(GameSound* sound);
	void  nonDiscardSoundData(GameSound* sound);
	JBool isDiscardSoundData(GameSound* sound);
	void  setSoundKeep(GameSound* sound);
	void  clearSoundKeep(GameSound* sound);
	JBool isSoundKeep(GameSound* sound);
	void  setSoundKeepable(GameSound* sound);
	void  clearSoundKeepable(GameSound* sound);
	JBool isSoundKeepable(GameSound* sound);
	void  setSoundName(GameSound* sound, u32 type, const char* name);

	u8* readVocFileData(const char* name, u32* size = nullptr);
}  // namespace TFE_Jedi