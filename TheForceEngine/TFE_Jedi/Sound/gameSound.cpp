#include <cstring>

#include "gameSound.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/IMuse/imuse.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>

namespace TFE_Jedi
{
	#define DEFAULT_PRIORITY 64
	static GameSound* s_soundList = nullptr;

	void purgeAllSounds();
	void freeSoundList(GameSound* sound);
	void initSound(GameSound* sound);
	u8*  readFileData(const char* name);

	/////////////////////////////////////////////////////////
	// System
	/////////////////////////////////////////////////////////
	void gameSoundInit()
	{
		s_soundList = nullptr;
	}

	void gameSoundDestroy()
	{
		purgeAllSounds();
	}

	GameSound* gameSoundGetList()
	{
		return s_soundList;
	}

	GameSound* gameSoundAlloc(u8* data)
	{
		GameSound* sound = (GameSound*)game_alloc(sizeof(GameSound));
		if (sound)
		{
			initSound(sound);
			sound->data = data;

			// Add the sound to the head of the list.
			if (s_soundList)
			{
				sound->next = s_soundList;
			}
			s_soundList = sound;
		}
		return sound;
	}
		
	GameSound* gameSoundLoad(const char* name, GameSoundType soundType)
	{
		// Read the data from the file.
		u8* data = readFileData(name);

		// Then allocate the sound itself.
		GameSound* sound = gameSoundAlloc(data);
		setSoundName(sound, soundType, name);
		discardSoundData(sound);
		sound->type = soundType;

		return sound;
	}

	void gameSoundFree(GameSound* sound)
	{
		gameSoundFreePlaying(sound, JTRUE);
	}

	void gameSoundFreePlaying(GameSound* sound, JBool stop)
	{
		if (isSoundKeep(sound) || isSoundKeepable(sound))
		{
			clearSoundKeep(sound);
		}
		else
		{
			GameSound* curSound = s_soundList;
			GameSound* lastSound = nullptr;

			while (curSound && curSound != sound)
			{
				lastSound = curSound;
				curSound = curSound->next;
			}

			if (curSound == sound)
			{
				if (lastSound)
				{
					lastSound->next = sound->next;
				}
				else
				{
					s_soundList = sound->next;
				}
			}

			if (stop)
			{
				stopSound(sound);
			}

			if (isDiscardSoundData(sound) && sound->data)
			{
				game_free(sound->data);
				sound->data = nullptr;
			}
			if (sound->varptr)
			{
				game_free(sound->varptr);
				sound->varptr = nullptr;
			}
			if (sound->varhdl)
			{
				game_free(sound->varhdl);
				sound->varhdl = nullptr;
			}
			game_free(sound);
		}
	}

	/////////////////////////////////////////////////////////
	// Game Sound <-> iMuse interface.
	/////////////////////////////////////////////////////////
	void startSfx(GameSound* sound)
	{
		ImStartSfx((ImSoundId)sound, DEFAULT_PRIORITY);
	}

	void startSpeech(GameSound* sound)
	{
		ImStartVoice((ImSoundId)sound, DEFAULT_PRIORITY);
	}

	void stopSound(GameSound* sound)
	{
		ImStopSound((ImSoundId)sound);
	}

	void setSoundVolume(GameSound* sound, s32 volume)
	{
		ImSetParam((ImSoundId)sound, soundVol, volume);
	}

	void setSoundFade(GameSound* sound, s32 volume, s32 time)
	{
		ImFadeParam((ImSoundId)sound, soundVol, volume, time);
	}

	void setSoundPan(GameSound* sound, s32 pan)
	{
		ImSetParam((ImSoundId)sound, soundPan, pan);
	}

	void setSoundPanFade(GameSound* sound, s32 pan, s32 time)
	{
		ImFadeParam((ImSoundId)sound, soundPan, pan, time);
	}

	/////////////////////////////////////////////////////////
	// Game Sound General Interface
	/////////////////////////////////////////////////////////
	void copySoundData(GameSound* dstSound, GameSound* srcSound)
	{
		if (isDiscardSoundData(dstSound) && dstSound->data)
		{
			game_free(dstSound->data);
		}
		nonDiscardSoundData(dstSound);
		setSoundName(dstSound, srcSound->resType, srcSound->name);
		dstSound->data = srcSound->data;
		dstSound->id   = srcSound->id;
		dstSound->type = srcSound->type;
	}
		
	GameSound* findSoundType(const char* name, u32 type)
	{
		GameSound* sound = s_soundList;
		s32 found = 0;
		while (sound && !found)
		{
			if (type == sound->resType)
			{
				found = 1;
				s32 i = 0;
				for (; i < GS_NAME_LEN && found && name[i]; i++)
				{
					found = (sound->name[i] == tolower(name[i]));
				}
				if (found && i < GS_NAME_LEN && sound->name[i]) { found = 0; }
			}

			if (!found)
			{
				sound = sound->next;
			}
		}
		return sound;
	}

	void discardSoundData(GameSound* sound)
	{
		sound->flags |= GS_SOUND_DISCARD;
	}

	void nonDiscardSoundData(GameSound* sound)
	{
		sound->flags &= ~GS_SOUND_DISCARD;
	}

	JBool isDiscardSoundData(GameSound* sound)
	{
		return sound->flags & GS_SOUND_DISCARD;
	}

	void setSoundKeep(GameSound* sound)
	{
		sound->flags |= GS_SOUND_KEEP;
	}

	void clearSoundKeep(GameSound* sound)
	{
		sound->flags &= ~GS_SOUND_KEEP;
	}

	JBool isSoundKeep(GameSound* sound)
	{
		return sound->flags & GS_SOUND_KEEP;
	}

	void setSoundKeepable(GameSound* sound)
	{
		sound->flags |= GS_SOUND_KEEPABLE;
	}

	void clearSoundKeepable(GameSound* sound)
	{
		sound->flags &= ~GS_SOUND_KEEPABLE;
	}

	JBool isSoundKeepable(GameSound* sound)
	{
		return sound->flags & GS_SOUND_KEEPABLE;
	}

	void setSoundName(GameSound* sound, u32 type, const char* name)
	{
		sound->resType = type;

		s32 i = 0;
		for (; i < GS_NAME_LEN && name[i]; i++)
		{
			sound->name[i] = tolower(name[i]);
		}
		while (i < GS_NAME_LEN)
		{
			sound->name[i] = 0;
			i++;
		}
	}

	void freeSoundList(GameSound* sound)
	{
		while (sound)
		{
			GameSound* next = sound->next;
			gameSoundFree(sound);
			sound = next;
		}
	}

	/////////////////////////////////////////////////////////
	// System Internal
	/////////////////////////////////////////////////////////
	void purgeAllSounds()
	{
		GameSound* sound = s_soundList;

		while (sound)
		{
			clearSoundKeep(sound);
			clearSoundKeepable(sound);
			sound = sound->next;
		}

		freeSoundList(s_soundList);
		s_soundList = nullptr;
	}

	void initSound(GameSound* sound)
	{
		memset(sound, 0, sizeof(GameSound));
		sound->volume = 128;
	}

	u8* readFileData(const char* name)
	{
		FilePath path;
		char fileName[TFE_MAX_PATH];
		sprintf(fileName, "%s.VOC", name);
		if (!TFE_Paths::getFilePath(fileName, &path))
		{
			sprintf(fileName, "%s.VOIC", name);
			if (!TFE_Paths::getFilePath(fileName, &path))
			{
				return nullptr;
			}
		}
		FileStream file;
		if (!file.open(&path, FileStream::MODE_READ))
		{
			return nullptr;
		}
		u32 size = (u32)file.getSize();
		u8* data = (u8*)game_alloc(size);
		if (!data)
		{
			return nullptr;
		}
		file.readBuffer(data, size);
		file.close();

		return data;
	}
}  // TFE_JediSound