#include "lsound.h"
#include <TFE_DarkForces/Landru/cutscene_film.h>
#include <TFE_System/system.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Jedi/Math/core_math.h>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	enum FaderType
	{
		FADER_VOLUME = 0,
		FADER_PAN,
	};

	struct SoundFader
	{
		FaderType type;
		s16 target;
		s16 time;

		fixed16_16 cur;
		fixed16_16 delta;

		LSound* sound;
		LTick startTick;
		LTick lastTick;
	};
	
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
		MAX_SOUNDS = 48,
		MAX_FADERS = 48,
		PRIORITY   = 64,
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

	static s32 s_faderCount = 0;
	static SoundFader s_faders[MAX_FADERS];
	static LSound* s_firstSound = nullptr;
	static const f32 c_volumeScale = 1.0f/128.0f;
	static const f32 c_panScale = 1.0f/128.0f;

	void soundFinished(void* userData, s32 arg);

	void lsound_actionFunc(s16 state, LSound* sound, s16 var1, s16 var2);
	void lsound_initSound(LSound* sound);
	void lsound_setName(LSound* sound, u32 resType, const char* name);
	void lsound_freePlaying(LSound* sound, JBool stop);

	LSound* lsound_alloc(u8* data, s16 extend);
	LSound* lsound_load(u32 type, const char* name, s16 id);
	
	void lsound_init()
	{
		s_firstSound = nullptr;
		s_faderCount = 0;
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
		return lsound_load(CF_TYPE_GMIDI, name, gmidiSound);
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
		sprintf(soundFile, "%s.%s", name, type == CF_TYPE_VOC_SOUND ? "VOIC" : "GMID");
		FilePath soundPath;
		FileStream file;
		if (!TFE_Paths::getFilePath(soundFile, &soundPath)) { return nullptr; }
		if (!file.open(&soundPath, FileStream::MODE_READ)) { return nullptr; }

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

		if (type == CF_TYPE_VOC_SOUND)
		{
			TFE_VocAsset::parseVoc(&sound->soundBuffer, sound->data);
		}
		else
		{
		}
		sound->soundSource = nullptr;
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
		if (!sound) { return; }

		if ((sound->flags & SOUND_KEEP) || (sound->flags & SOUND_KEEPABLE))	// Keep the sound for one cycle.
		{
			sound->flags &= ~SOUND_KEEP;
			if (!(sound->flags & SOUND_KEEP_USER))
			{
				sound->flags &= ~SOUND_KEEP_USER;
			}
		}
		else  // Remove
		{
			LSound* curSound  = s_firstSound;
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
					s_firstSound = sound->next;
				}
				sound->next = nullptr;
			}
			else
			{
				return;
			}

			if (stop)
			{
				lsound_stop(sound);
			}

			if ((sound->flags & SOUND_DISCARD) && sound->data)
			{
				landru_free(sound->data);

				// TFE
				if (sound->soundSource)
				{
					TFE_Audio::freeSource(sound->soundSource);
				}
				free(sound->soundBuffer.data);
			}

			if (sound->varPtr)  { landru_free(sound->varPtr); }
			if (sound->varPtr2) { landru_free(sound->varPtr2); }
			landru_free(sound);
		}
	}

	LSound* lsound_getList()
	{
		return s_firstSound;
	}

	void lsound_freeSounds(LSound* sound)
	{
		while (sound)
		{
			LSound* nextSound = sound->next;
			lsound_free(sound);
			sound = nextSound;
		}
		s_faderCount = 0;
	}

	void lsound_initSound(LSound* sound)
	{
		sound->next = nullptr;
		sound->type = noSound;
		sound->id = 0;
		sound->flags = 0;
		sound->volume = 128;
		sound->pan = 64;
		
		sound->var1 = 0;
		sound->var2 = 0;

		sound->varPtr = nullptr;
		sound->varPtr2 = nullptr;

		sound->callback = nullptr;
		sound->data = nullptr;
	}
		
	void lsound_addFader(LSound* sound, FaderType type, s16 target, s16 time)
	{
		if (s_faderCount >= MAX_FADERS) { return; }
		SoundFader* fader = &s_faders[s_faderCount];
		s_faderCount++;

		fader->target = target;
		fader->time = time;
		fader->cur = (type == FADER_VOLUME) ? intToFixed16(sound->volume) : intToFixed16(sound->pan);

		fixed16_16 volumeDelta = intToFixed16(target) - fader->cur;
		fader->delta = time ? div16(volumeDelta, intToFixed16(time)) : volumeDelta;
		fader->sound = sound;
		fader->startTick = ltime_curTick();
		fader->lastTick  = fader->startTick;
	}

	void lsound_removeFader(s32 index)
	{
		for (s32 i = index; i < s_faderCount - 1; i++)
		{
			s_faders[i] = s_faders[i + 1];
		}
		s_faderCount--;
	}

	void lsound_update()
	{
		LTick curTick = ltime_curTick();
		for (s32 i = s_faderCount - 1; i >= 0; i--)
		{
			SoundFader* fader = &s_faders[i];

			s32 dt = s32(curTick - fader->lastTick);
			fader->lastTick = curTick;
			
			fader->cur += mul16(fader->delta, intToFixed16(dt));
			fader->time -= dt;

			fixed16_16 cur = fader->cur;
			LSound* sound  = fader->sound;
			FaderType type = fader->type;
			if (fader->time <= 0)
			{
				cur = intToFixed16(fader->target);
				lsound_removeFader(i);
			}
	
			if (type == FADER_VOLUME)
			{
				sound->volume = floor16(cur);
				assert(sound->volume > 0);
				if (sound->soundSource)
				{
					TFE_Audio::setSourceVolume(sound->soundSource, f32(sound->volume)*c_volumeScale);
				}
			}
			else
			{
				sound->pan = floor16(cur);
				if (sound->soundSource)
				{
					TFE_Audio::setSourceStereoSeperation(sound->soundSource, f32(sound->pan)*c_panScale);
				}
			}
		}
	}
		
	void lsound_actionFunc(s16 state, LSound* sound, s16 var1, s16 var2)
	{
		switch (state)
		{
			case SOUND_CMD_PAUSE:
			{
				// I don't think this is actually used.
				assert(0);
			} break;
			case SOUND_CMD_UNPAUSE:
			{
				// I don't think this is actually used.
				assert(0);
			} break;
			case SOUND_CMD_STARTMUSIC:
			{
				// I don't think this is actually used.
				assert(0);
			} break;
			case SOUND_CMD_STARTSFX:
			{
				bool looping = (sound->soundBuffer.flags & SBUFFER_FLAG_LOOPING) != 0;
				if (!sound->soundSource)
				{
					sound->soundSource = TFE_Audio::createSoundSource(SoundType::SOUND_2D, f32(sound->volume)*c_volumeScale, f32(sound->pan)*c_panScale,
						&sound->soundBuffer, nullptr, false, soundFinished, sound);
					TFE_Audio::playSource(sound->soundSource, looping);
				}
			} break;
			case SOUND_CMD_STARTSPEECH:
			{
				if (!sound->soundSource)
				{
					sound->soundSource = TFE_Audio::createSoundSource(SoundType::SOUND_2D, f32(sound->volume)*c_volumeScale, f32(sound->pan)*c_panScale,
						&sound->soundBuffer, nullptr, false, soundFinished, sound);
					TFE_Audio::playSource(sound->soundSource);
				}
			} break;
			case SOUND_CMD_STOP:
			{
				if (sound->soundSource)
				{
					TFE_Audio::freeSource(sound->soundSource);
					sound->soundSource = nullptr;
				}
			} break;
			case SOUND_CMD_VOLUME:
			{
				sound->volume = var1;
				if (sound->soundSource)
				{
					TFE_Audio::setSourceVolume(sound->soundSource, f32(sound->volume)*c_volumeScale);
				}
			} break;
			case SOUND_CMD_FADE:
			{
				lsound_addFader(sound, FADER_VOLUME, var1, var2);
			} break;
			case SOUND_CMD_PAN:
			{
				sound->pan = var1;
				if (sound->soundSource)
				{
					TFE_Audio::setSourceStereoSeperation(sound->soundSource, f32(sound->pan)*c_panScale);
				}
			} break;
			case SOUND_CMD_PAN_FADE:
			{
				lsound_addFader(sound, FADER_PAN, var1, var2);
			} break;
		}
	}
	   
	void soundFinished(void* userData, s32 arg)
	{
		LSound* sound = (LSound*)userData;
		sound->soundSource = nullptr;
	}
}  // namespace TFE_DarkForces