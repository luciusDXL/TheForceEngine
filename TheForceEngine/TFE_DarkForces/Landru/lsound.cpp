#include "lsound.h"
#include <TFE_DarkForces/Landru/cutscene_film.h>
#include <TFE_System/system.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <assert.h>

namespace TFE_DarkForces
{
	LSound* s_firstSound = nullptr;
	enum SoundCommand
	{
		SOUND_CMD_NULL = 0,
		SOUND_CMD_PAUSE,
		SOUND_CMD_UNPAUSE,
		SOUND_CMD_STARTMUSIC,
		SOUND_CMD_STARTSFX,
		SOUND_CMD_STARTSPEECH,
		SOUND_CMD_STOP,
		SOUND_CMD_VOLUME,
		SOUND_CMD_FADE,
		SOUND_CMD_PAN,
		SOUND_CMD_PAN_FADE,
		SOUND_CMD_COUNT
	};

	enum SoundConstants
	{
		MAX_SOUNDS = 0x0030,
		PRIORITY = 64,
	};

	enum SoundFlags
	{
		SOUND_FREE      = FLAG_BIT(0),
		SOUND_DISCARD   = FLAG_BIT(1),
		SOUND_KEEP_USER = FLAG_BIT(2),

		SOUND_KEEP       = FLAG_BIT(12),
		SOUND_KEEPABLE   = FLAG_BIT(13),
		SOUND_USER_FLAG1 = FLAG_BIT(14),
		SOUND_USER_FLAG2 = FLAG_BIT(15),
	};

	void lsound_actionFunc(s16 state, LSound* sound, s16 var1, s16 var2);
	LSound* lsound_alloc(u8* data, s16 extend);
	void lsound_initSound(LSound* sound);
	LSound* lsound_load(u32 type, const char* name, s16 id);
	void lsound_setName(LSound* sound, u32 resType, const char* name);
	void lsound_freePlaying(LSound* sound, JBool stop);

	void lsound_init()
	{
	}

	void lsound_destroy()
	{
	}

	LSound* lsound_loadDigital(const char* name)
	{
		return lsound_load(CF_TYPE_VOC_SOUND, name, digitalSound);
	}

	LSound* lsound_loadDigitalVoice(const char* name)
	{
		return lsound_load(CF_TYPE_VOC_SOUND, name, digitalSound);
	}

	LSound* lsound_loadMusic(const char* name)
	{
		// TODO
		return nullptr;
	}
		
	void lsound_startSpeech(LSound* sound)
	{
		lsound_actionFunc(SOUND_CMD_STARTSPEECH, sound, 0, 0);
	}

	void lsound_startSfx(LSound* sound)
	{
		lsound_actionFunc(SOUND_CMD_STARTSFX, sound, 0, 0);
	}

	void lsound_stop(LSound* sound)
	{
		lsound_actionFunc(SOUND_CMD_STOP, sound, 0, 0);
	}

	void lsound_setVolume(LSound* sound, s16 volume)
	{
		lsound_actionFunc(SOUND_CMD_VOLUME, sound, volume, 0);
	}

	void lsound_setFade(LSound* sound, s16 volume, s16 time)
	{
		lsound_actionFunc(SOUND_CMD_FADE, sound, volume, time);
	}

	void lsound_setPan(LSound* sound, s16 pan)
	{
		lsound_actionFunc(SOUND_CMD_PAN, sound, pan, 0);
	}

	void lsound_setPanFade(LSound* sound, s16 pan, s16 time)
	{
		lsound_actionFunc(SOUND_CMD_PAN_FADE, sound, pan, time);
	}

	void lsound_setKeep(LSound* sound)
	{
		sound->flags |= SOUND_KEEP;
	}

	LSound* lsound_load(u32 type, const char* name, s16 id)
	{
		char soundFile[32];
		sprintf(soundFile, "%s.VOIC", name);
		FilePath soundPath;
		FileStream file;
		if (!TFE_Paths::getFilePath(soundFile, &soundPath)) { return nullptr; }
		if (!file.open(&soundPath, FileStream::MODE_READ))   { return nullptr; }

		u32 len = (u32)file.getSize();
		u8* data = (u8*)landru_alloc(len);
		if (!data)
		{
			file.close();
			return nullptr;
		}
		file.readBuffer(data, len);
		file.close();

		LSound* sound = lsound_alloc(data, 0);
		if (!sound) { return nullptr; }

		lsound_setName(sound, type, name);
		sound->flags |= SOUND_DISCARD;
		sound->id = id;

		return sound;
	}

	void lsound_setName(LSound* sound, u32 resType, const char* name)
	{
		sound->resType = resType;
		strcpy(sound->name, name);
	}

	LSound* lsound_alloc(u8* data, s16 extend)
	{
		LSound* sound = (LSound*)landru_alloc(sizeof(LSound) + extend);
		if (sound)
		{
			lsound_initSound(sound);
			sound->data = data;

			if (!s_firstSound)
			{
				s_firstSound = sound;
			}
			else
			{
				sound->next = s_firstSound;
				s_firstSound = sound;
			}
		}
		return sound;
	}

	void lsound_free(LSound* sound)
	{
		lsound_freePlaying(sound, JTRUE);
	}

	void lsound_freePlaying(LSound* sound, JBool stop)
	{

	}

	void lsound_initSound(LSound* sound)
	{
		sound->next = nullptr;
		sound->type = noSound;
		sound->id = 0;
		sound->flags = 0;
		sound->volume = 128;
		
		sound->var1 = 0;
		sound->var2 = 0;

		sound->varPtr = nullptr;
		sound->varPtr2 = nullptr;

		sound->callback = nullptr;
		sound->data = nullptr;
	}

	void lsound_actionFunc(s16 state, LSound* sound, s16 var1, s16 var2)
	{
		switch (state)
		{
			case SOUND_CMD_PAUSE:
				// ImPause();
				// group_vol_gbl = ImSetGroupVol(groupMaster, 0);
				break;

			case SOUND_CMD_UNPAUSE:
				// ImSetGroupVol(groupMaster, group_vol_gbl);
				// ImResume();
				break;
			case SOUND_CMD_STARTMUSIC:
				// ImStartMusic((DWORD)the_sound, PRIORITY);
				break;

			case SOUND_CMD_STARTSFX:
				// ImStartSfx((DWORD)the_sound, PRIORITY);
				break;

			case SOUND_CMD_STARTSPEECH:
				// ImStartVoice((DWORD)the_sound, PRIORITY);
				break;

			case SOUND_CMD_STOP:
				// ImStopSound((DWORD)the_sound);
				break;

			case SOUND_CMD_VOLUME:
				// ImSetParam((DWORD)the_sound, soundVol, var1);
				break;

			case SOUND_CMD_FADE:
				// ImFadeParam((DWORD)the_sound, soundVol, var1, var2);
				// ImFade ((DWORD)the_sound, fadeVol, var1, var2);
				break;

			case SOUND_CMD_PAN:
				// ImSetParam((DWORD)the_sound, soundPan, var1);
				break;

			case SOUND_CMD_PAN_FADE:
				// ImFadeParam((DWORD)the_sound, soundPan, var1, var2);
				// ImFade ((DWORD)the_sound, fadePan, var1, var2);
				break;
		}
	}
}  // namespace TFE_DarkForces