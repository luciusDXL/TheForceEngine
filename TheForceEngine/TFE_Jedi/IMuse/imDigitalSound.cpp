#include "imuse.h"
#include "imDigitalSound.h"
#include <TFE_System/system.h>
#include <TFE_Audio/midi.h>
#include <assert.h>

namespace TFE_Jedi
{
	////////////////////////////////////////////////////
	// Structures
	////////////////////////////////////////////////////
	struct ImWaveSound
	{
		s32 u00;
		ImWaveSound* next;
		u32 waveStreamFlag;
		ImSoundId soundId;
		s32 marker;
		s32 group;
		s32 priority;
		s32 baseVolume;
		s32 volume;
		s32 pan;
		s32 detune;
		s32 transpose;
		s32 detuneTrans;
		s32 mailbox;
	};

	struct ImWaveData;

	struct ImWavePlayer
	{
		ImWavePlayer* prev;
		ImWavePlayer* next;
		ImWaveData*   data;
		iptr soundId;
	};

	struct ImWaveData
	{
		ImWavePlayer* player;
		s32 offset;
		s32 chunkSize;
		s32 u0c;

		s32 chunkIndex;
		s32 u14;
		s32 u18;
		s32 u1c;

		s32 u20;
		s32 u24;
		s32 u28;
		s32 u2c;

		s32 u30;
	};
	
	/////////////////////////////////////////////////////
	// Internal State
	/////////////////////////////////////////////////////
	static ImWaveSound* s_imWaveSounds = nullptr;
	static ImWaveSound  s_imWaveSoundStore[16];
	static ImWavePlayer s_imWavePlayer[16];
	static ImWaveData   s_imWaveData[16];
	static u8  s_imWaveChunkData[48];
	static s32 s_imWaveMixCount = 8;
	static s32 s_imWaveNanosecsPerSample;
	static iMuseInitData* s_imDigitalData;

	static u8  s_imWaveFalloffTableNeg[1028];
	static u8  s_imWaveFalloffTablePos[1024];
	static u8* s_imWaveFalloffTable = s_imWaveFalloffTablePos;

	extern atomic_s32 s_sndPlayerLock;
	extern atomic_s32 s_digitalPause;

	extern void ImMidiPlayerLock();
	extern void ImMidiPlayerUnlock();
	extern s32 ImWrapValue(s32 value, s32 a, s32 b);
	extern s32 ImGetGroupVolume(s32 group);

	ImWaveData* ImGetWaveData(s32 index);
	s32 ImComputeDigitalFalloff(iMuseInitData* initData);
	s32 ImSetWaveParamInternal(ImSoundId soundId, s32 param, s32 value);
	s32 ImGetWaveParamIntern(ImSoundId soundId, s32 param);
	
	/////////////////////////////////////////////////////////// 
	// API
	/////////////////////////////////////////////////////////// 
	s32 ImInitializeDigitalAudio(iMuseInitData* initData)
	{
		IM_DBG_MSG("TRACKS module...");
		if (initData->waveMixCount <= 0 || initData->waveMixCount > 16)
		{
			IM_LOG_ERR("TR: waveMixCount NULL or too big, defaulting to 4...");
			initData->waveMixCount = 4;
		}
		s_imWaveMixCount = initData->waveMixCount;
		s_digitalPause = 0;
		s_imWaveSounds = nullptr;

		if (initData->waveSpeed == IM_WAVE_11kHz) // <- this is the path taken by Dark Forces DOS
		{
			// Nanoseconds per second / wave speed in Hz
			// 1,000,000,000 / 11,000
			s_imWaveNanosecsPerSample = 90909;
		}
		else // IM_WAVE_22kHz
		{
			// Nanoseconds per second / wave speed in Hz
			// 1,000,000,000 / 22,000
			s_imWaveNanosecsPerSample = 45454;
		}

		ImWavePlayer* player = s_imWavePlayer;
		for (s32 i = 0; i < s_imWaveMixCount; i++, player++)
		{
			player->prev = nullptr;
			player->next = nullptr;
			ImWaveData* data = ImGetWaveData(i);
			player->data = data;
			data->player = player;
			player->soundId = IM_NULL_SOUNDID;
		}

		s_sndPlayerLock = 0;
		return ImComputeDigitalFalloff(initData);
	}

	s32 ImSetWaveParam(ImSoundId soundId, s32 param, s32 value)
	{
		ImMidiPlayerLock();
		s32 res = ImSetWaveParamInternal(soundId, param, value);
		ImMidiPlayerUnlock();
		return res;
	}

	s32 ImGetWaveParam(ImSoundId soundId, s32 param)
	{
		ImMidiPlayerLock();
		s32 res = ImGetWaveParamIntern(soundId, param);
		ImMidiPlayerUnlock();
		return res;
	}

	////////////////////////////////////
	// Internal
	////////////////////////////////////
	ImWaveData* ImGetWaveData(s32 index)
	{
		return &s_imWaveData[index];
	}

	s32 ImComputeDigitalFalloff(iMuseInitData* initData)
	{
		s_imDigitalData = initData;
		s32 waveMixCount = initData->waveMixCount;
		s32 volumeMidPoint = 128;
		s32 tableSize = waveMixCount << 7;
		for (s32 i = 0; i < tableSize; i++)
		{
			// Results for count ~= 8: (i=0) 0.0, 1.5, 2.5, 3.4, 4.4, 5.2, 6.3, 7.2, ... 127.1 (i = 1023).
			s32 volumeOffset = ((waveMixCount * 127 * i) << 8) / (waveMixCount * 127 + (waveMixCount - 1)*i) + 128;
			volumeOffset >>= 8;

			s_imWaveFalloffTable[i] = volumeMidPoint + volumeOffset;
			u8* vol = s_imWaveFalloffTable - i - 1;
			*vol = volumeMidPoint - volumeOffset - 1;
		}
		return imSuccess;
	}

	s32 ImSetWaveParamInternal(ImSoundId soundId, s32 param, s32 value)
	{
		ImWaveSound* sound = s_imWaveSounds;
		while (sound)
		{
			if (sound->soundId == soundId)
			{
				if (param == soundGroup)
				{
					if (value >= 16)
					{
						return imArgErr;
					}
					sound->volume = ((sound->baseVolume + 1) * ImGetGroupVolume(value)) >> 7;
					return imSuccess;
				}
				else if (param == soundPriority)
				{
					if (value > 127)
					{
						return imArgErr;
					}
					sound->priority = value;
					return imSuccess;
				}
				else if (param == soundVol)
				{
					if (value > 127)
					{
						return imArgErr;
					}
					sound->baseVolume = value;
					sound->volume = ((sound->baseVolume + 1) * ImGetGroupVolume(sound->group)) >> 7;
					return imSuccess;
				}
				else if (param == soundPan)
				{
					if (value > 127)
					{
						return imArgErr;
					}
					sound->pan = value;
					return imSuccess;
				}
				else if (param == soundDetune)
				{
					if (value < -9216 || value > 9216)
					{
						return imArgErr;
					}
					sound->detune = value;
					sound->detuneTrans = sound->detune + (sound->transpose << 8);
					return imSuccess;
				}
				else if (param == soundTranspose)
				{
					if (value < -12 || value > 12)
					{
						return imArgErr;
					}
					sound->transpose = value ? ImWrapValue(sound->transpose + value, -12, 12) : 0;
					sound->detuneTrans = sound->detune + (sound->transpose << 8);
					return imSuccess;
				}
				else if (param == soundMailbox)
				{
					sound->mailbox = value;
					return imSuccess;
				}
				// Invalid Parameter
				IM_LOG_ERR("ERR: TrSetParam() couldn't set param %lu...", param);
				return imArgErr;
			}
			sound = sound->next;
		}
		return imInvalidSound;
	}

	s32 ImGetWaveParamIntern(ImSoundId soundId, s32 param)
	{
		s32 soundCount = 0;
		ImWaveSound* sound = s_imWaveSounds;
		while (sound)
		{
			if (sound->soundId == soundId)
			{
				if (param == soundType)
				{
					return imFail;
				}
				else if (param == soundPlayCount)
				{
					soundCount++;
				}
				else if (param == soundMarker)
				{
					return sound->marker;
				}
				else if (param == soundGroup)
				{
					return sound->group;
				}
				else if (param == soundPriority)
				{
					return sound->priority;
				}
				else if (param == soundVol)
				{
					return sound->baseVolume;
				}
				else if (param == soundPan)
				{
					return sound->pan;
				}
				else if (param == soundDetune)
				{
					return sound->detune;
				}
				else if (param == soundTranspose)
				{
					return sound->transpose;
				}
				else if (param == soundMailbox)
				{
					return sound->mailbox;
				}
				else if (param == waveStreamFlag)
				{
					return sound->waveStreamFlag ? 1 : 0;
				}
				else
				{
					return imArgErr;
				}
			}
			sound = sound->next;
		}
		return (param == soundPlayCount) ? soundCount : imInvalidSound;
	}

}  // namespace TFE_Jedi
