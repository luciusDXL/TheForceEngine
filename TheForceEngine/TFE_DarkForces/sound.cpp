#include <cstring>

#include "sound.h"
#include "player.h"
#include <TFE_Jedi/Sound/gameSound.h>
#include <TFE_Game/igame.h>
#include <TFE_Asset/vocAsset.h>
#include <TFE_Audio/audioSystem.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/IMuse/imuse.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_DarkForces/time.h>

namespace TFE_DarkForces
{
	/////////////////////////////////////////////////////////////
	// TODO: Add a callback to iMuse to get the sound data from
	// the ID. Then hook it up here and with GameSound.
	/////////////////////////////////////////////////////////////
	struct LevelSound
	{
		SoundEffectId id;
		u32 time;
		u8* data;
		u32 size;

		s32 volume;
		s32 refCount;

		s32 priority;
		s32 mark;
		char name[13];
	};

	#define MAX_LEVEL_SOUNDS 300
	#define CUE_RING1 FIXED(30)
	#define CUE_RING2 FIXED(150)

	static s32 s_tPan[32] = { 00,-06,-12,-18,-24,-30,-36,-42,-48,-42,-36,-30,-24,-18,-12,-06,00,06,12,18,24,30,36,42,48,42,36,30,24,18,12,06 };
	static Allocator* s_levelSoundList = nullptr;
	static s32 s_instance = 1;
	s32 s_lastMaintainVolume;

	SoundEffectId soundInstance(SoundSourceId soundId, s32 instance);
	LevelSound* getSoundPtr(SoundSourceId id);
	void soundCalculateCue(LevelSound* sound, fixed16_16 x, fixed16_16 y, fixed16_16 z, s32* vol, s32* pan);
	u8* sound_getResource(SoundEffectId id);

	// Called at game startup and shutdown.
	void sound_open(MemoryRegion* memRegion)
	{
		s_levelSoundList = allocator_create(sizeof(LevelSound), s_gameRegion);
		s_instance = 0;
		ImInitialize(memRegion);
	}

	void sound_close()
	{
		allocator_free(s_levelSoundList);
		s_levelSoundList = nullptr;
		s_instance = 0;
		ImTerminate();
	}

	// Called at level startup and shutdown.
	void sound_levelStart()
	{
		ImSetResourceCallback(sound_getResource);
		s_instance = 1;
	}

	void sound_levelStop()
	{
		sound_stopAll();
		ImSetResourceCallback(nullptr);
	}

	SoundSourceId sound_load(const char* fileName, u32 priority)
	{
		SoundSourceId newId = 0;
		LevelSound* sound = (LevelSound*)allocator_getHead(s_levelSoundList);
		while (sound)
		{
			if (!strcasecmp(fileName, sound->name))
			{
				sound->refCount++;
				return soundInstance((SoundSourceId)sound, 0);
			}
			sound = (LevelSound*)allocator_getNext(s_levelSoundList);
		}

		u32 size = 0;
		u8* data = readVocFileData(fileName, &size);
		if (data)
		{
			sound = (LevelSound*)allocator_newItem(s_levelSoundList);
			sound->id = (SoundSourceId)sound;
			sound->time = s_curTick;
			sound->data = data;
			strcpy_s(sound->name, 13, fileName);
			sound->priority = priority;
			sound->size = size;
			sound->volume = 127;
			sound->refCount = 1;
			newId = soundInstance(sound->id, 0);
		}

		return newId;
	}

	void sound_free(SoundSourceId id)
	{
		if (id)
		{
			LevelSound* sound = getSoundPtr(id);
			sound->refCount--;

			if (sound->refCount == 0)
			{
				if (sound->data)
				{
					game_free(sound->data);
				}
				sound->data = nullptr;
				allocator_deleteItem(s_levelSoundList, sound);
			}
		}
	}

	void sound_setBaseVolume(SoundSourceId id, s32 volume)
	{
		LevelSound* sound = getSoundPtr(id);
		if (sound)
		{
			sound->volume = volume;
		}
	}

	void sound_setVolume(SoundEffectId id, s32 volume)
	{
		if (id)
		{
			ImSetParam(id, soundVol, volume & 0x7f);
		}
	}

	void sound_setPan(SoundEffectId id, s32 pan)
	{
		if (id)
		{
			ImSetParam(id, soundPan, (pan + 64) & 0x7f);
		}
	}

	void sound_pitchShift(SoundEffectId soundId, s32 shift)
	{
		// no-op.
	}
		
	SoundEffectId sound_playPriority(SoundSourceId id, s32 priority)
	{
		if (!id) { return 0; }

		LevelSound* sound = getSoundPtr(id);
		SoundEffectId idInstance = soundInstance(sound->id, s_instance++);

		if (!sound->data)
		{
			return 0;
		}

		if (priority < 0)
		{
			priority = sound->priority;
		}
		// Here is where it breaks - I should pass in a function to convert IDs to data.
		ImStartSfx(idInstance, priority);
		sound_setVolume(idInstance, sound->volume);

		return idInstance;
	}

	SoundEffectId sound_play(SoundSourceId id)
	{
		return sound_playPriority(id, -1);
	}

	SoundEffectId sound_playCued(SoundSourceId id, vec3_fixed pos)
	{
		SoundEffectId idInstance = 0;
		if (id)
		{
			LevelSound* sound = getSoundPtr(id);
			s32 vol, pan;
			soundCalculateCue(sound, pos.x, pos.y, pos.z, &vol, &pan);

			idInstance = sound_play(id);
			sound_setVolume(idInstance, vol);
			sound_setPan(idInstance, pan);
		}
		return idInstance;
	}

	void sound_adjustCued(SoundEffectId id, vec3_fixed pos)
	{
		if (id)
		{
			s32 vol, pan;
			LevelSound* sound = getSoundPtr(id);
			soundCalculateCue(sound, pos.x, pos.y, pos.z, &vol, &pan);
			sound_setVolume(id, vol);
			sound_setPan(id, pan);
		}
	}

	void sound_stop(SoundEffectId id)
	{
		if (id)
		{
			ImStopSound(id);
		}
	}

	void sound_stopAll()
	{
		ImStopAllSounds();
	}
		
	SoundEffectId sound_maintain(SoundEffectId idInstance, SoundSourceId idSound, vec3_fixed pos)
	{
		if (idSound)
		{
			s32 vol, pan;
			LevelSound* sound = getSoundPtr(idSound);
			soundCalculateCue(sound, pos.x, pos.y, pos.z, &vol, &pan);
			s_lastMaintainVolume = vol;

			if (vol)
			{
				// Check to see if the soiund is still playing at specific intervals.
				if (idInstance && !(s_curTick & 0x0f))
				{
					if (ImGetParam(idInstance, soundPlayCount) == 0)
					{
						sound_stop(idInstance);
						idInstance = 0;
					}
				}

				if (!idInstance)
				{
					idInstance = sound_play(idSound);
				}
				sound_setVolume(idInstance, vol);
				sound_setPan(idInstance, pan);
			}
			else if (idInstance)
			{
				sound_stop(idInstance);
				idInstance = 0;
			}
			return idInstance;
		}
		return 0;
	}
		   
	////////////////////////////////////////////////////////////////////
	// Internal
	////////////////////////////////////////////////////////////////////
	void soundCalculateCue(LevelSound* sound, fixed16_16 x, fixed16_16 y, fixed16_16 z, s32* vol, s32* pan)
	{
		fixed16_16 dist = TFE_Jedi::abs(s_eyePos.y - y) + distApprox(x, z, s_eyePos.x, s_eyePos.z);

		// Calculate the volume.
		s32 volume = 0;
		if (dist < CUE_RING2)
		{
			if (dist < CUE_RING1)
			{
				// The sound is within the full volume radius.
				volume = sound->volume;
			}
			else
			{
				// The sound will be attenuated based on distance.
				fixed16_16 ratio = div16(CUE_RING2 - dist, CUE_RING2 - CUE_RING1);
				volume = floor16(mul16(ratio, intToFixed16(sound->volume)));
				volume = min(127, volume);
			}
		}
		*vol = volume;

		// Calculate the pan.
		angle14_32 angle = vec2ToAngle(x - s_eyePos.x, z - s_eyePos.z);
		angle = getAngleDifference(s_yaw, angle);
		*pan = s_tPan[((angle + 8192) / 512) & 31];
	}
		
	SoundEffectId soundInstance(SoundSourceId soundId, s32 instance)
	{
		SoundEffectId id = soundId - (SoundEffectId)s_levelSoundList;
		assert(id <= 0xffffffffffffll);
		if (instance > 0)
		{
			static s32 __x = 0;
			__x++;
		}
		id |= s64(instance & 0x7fff) << s64(48);
		return id;
	}

	LevelSound* getSoundPtr(SoundSourceId id)
	{
		return (LevelSound*)((u8*)s_levelSoundList + (id & s64(0xffffffffffffll)));
	}

	u8* sound_getResource(SoundEffectId id)
	{
		LevelSound* sound = getSoundPtr(id);
		return sound ? sound->data : nullptr;
	}
}  // TFE_DarkForces