#include <cstring>

#include "lsound.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/IMuse/imuse.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	#define DEFAULT_PRIORITY 64
	static LSound* s_soundList = nullptr;

	void purgeAllSounds();
	void freeSoundList(LSound* sound);
	void initSound(LSound* sound);

	/////////////////////////////////////////////////////////
	// System
	/////////////////////////////////////////////////////////
	void lSoundInit()
	{
		s_soundList = nullptr;
	}

	void lSoundDestroy()
	{
		purgeAllSounds();
	}

	LSound* lSoundGetList()
	{
		return s_soundList;
	}

	LSound* lSoundAlloc(u8* data)
	{
		LSound* sound = (LSound*)game_alloc(sizeof(LSound));
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
		
	LSound* lSoundLoad(const char* name, LSoundType soundType)
	{
		// Read the data from the file.
		u8* data = readVocFileData(name);

		// Then allocate the sound itself.
		LSound* sound = lSoundAlloc(data);
		setSoundName(sound, soundType, name);
		discardSoundData(sound);
		sound->type = soundType;

		return sound;
	}

	void lSoundFree(LSound* sound)
	{
		if (!sound) { return; }
		lSoundFreePlaying(sound, JTRUE);
	}

	void lSoundFreePlaying(LSound* sound, JBool stop)
	{
		if (isSoundKeep(sound) || isSoundKeepable(sound))
		{
			clearSoundKeep(sound);
		}
		else
		{
			LSound* curSound = s_soundList;
			LSound* lastSound = nullptr;

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
	void startSfx(LSound* sound)
	{
		ImStartSfx((ImSoundId)sound, DEFAULT_PRIORITY);
	}

	void startSpeech(LSound* sound)
	{
		ImStartVoice((ImSoundId)sound, DEFAULT_PRIORITY);
	}

	void stopSound(LSound* sound)
	{
		ImStopSound((ImSoundId)sound);
	}

	void setSoundVolume(LSound* sound, s32 volume)
	{
		ImSetParam((ImSoundId)sound, soundVol, volume);
	}

	void setSoundFade(LSound* sound, s32 volume, s32 time)
	{
		ImFadeParam((ImSoundId)sound, soundVol, volume, time);
	}

	void setSoundPan(LSound* sound, s32 pan)
	{
		ImSetParam((ImSoundId)sound, soundPan, pan);
	}

	void setSoundPanFade(LSound* sound, s32 pan, s32 time)
	{
		ImFadeParam((ImSoundId)sound, soundPan, pan, time);
	}

	/////////////////////////////////////////////////////////
	// Game Sound General Interface
	/////////////////////////////////////////////////////////
	void copySoundData(LSound* dstSound, LSound* srcSound)
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
		
	LSound* findSoundType(const char* name, u32 type)
	{
		LSound* sound = s_soundList;
		s32 found = 0;
		while (sound && !found)
		{
			if (type == sound->resType)
			{
				found = 1;
				s32 i = 0;
				for (; i < LS_NAME_LEN && found && name[i]; i++)
				{
					found = (sound->name[i] == tolower(name[i]));
				}
				if (found && i < LS_NAME_LEN && sound->name[i]) { found = 0; }
			}

			if (!found)
			{
				sound = sound->next;
			}
		}
		return sound;
	}

	void discardSoundData(LSound* sound)
	{
		sound->flags |= LS_SOUND_DISCARD;
	}

	void nonDiscardSoundData(LSound* sound)
	{
		sound->flags &= ~LS_SOUND_DISCARD;
	}

	JBool isDiscardSoundData(LSound* sound)
	{
		return sound->flags & LS_SOUND_DISCARD;
	}

	void setSoundKeep(LSound* sound)
	{
		sound->flags |= LS_SOUND_KEEP;
	}

	void clearSoundKeep(LSound* sound)
	{
		sound->flags &= ~LS_SOUND_KEEP;
	}

	JBool isSoundKeep(LSound* sound)
	{
		return sound->flags & LS_SOUND_KEEP;
	}

	void setSoundKeepable(LSound* sound)
	{
		sound->flags |= LS_SOUND_KEEPABLE;
	}

	void clearSoundKeepable(LSound* sound)
	{
		sound->flags &= ~LS_SOUND_KEEPABLE;
	}

	JBool isSoundKeepable(LSound* sound)
	{
		return sound->flags & LS_SOUND_KEEPABLE;
	}

	void setSoundName(LSound* sound, u32 type, const char* name)
	{
		sound->resType = type;

		s32 i = 0;
		for (; i < LS_NAME_LEN && name[i]; i++)
		{
			sound->name[i] = tolower(name[i]);
		}
		while (i < LS_NAME_LEN)
		{
			sound->name[i] = 0;
			i++;
		}
	}

	void freeSoundList(LSound* sound)
	{
		while (sound)
		{
			LSound* next = sound->next;
			lSoundFree(sound);
			sound = next;
		}
	}

	u8* readVocFileData(const char* name, u32* sizeOut)
	{
		FilePath path;
		if (strstr(name, ".voc") || strstr(name, ".VOC"))
		{
			if (!TFE_Paths::getFilePath(name, &path))
			{
				return nullptr;
			}
		}
		else
		{
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
		}
		FileStream file;
		if (!file.open(&path, Stream::MODE_READ))
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

		if (sizeOut)
		{
			*sizeOut = size;
		}

		return data;
	}

	/////////////////////////////////////////////////////////
	// System Internal
	/////////////////////////////////////////////////////////
	void purgeAllSounds()
	{
		LSound* sound = s_soundList;

		while (sound)
		{
			clearSoundKeep(sound);
			clearSoundKeepable(sound);
			sound = sound->next;
		}

		freeSoundList(s_soundList);
		s_soundList = nullptr;
	}

	void initSound(LSound* sound)
	{
		memset(sound, 0, sizeof(LSound));
		sound->volume = 128;
	}
}  // TFE_JediSound