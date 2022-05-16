#include "imuse.h"
#include "imDigitalSound.h"
#include "imDigitalVolumeTable.h"
#include "imSoundFader.h"
#include "imTrigger.h"
#include "imList.h"
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_System/system.h>
#include <TFE_Audio/midi.h>
#include <TFE_Audio/audioSystem.h>
#include <assert.h>

namespace TFE_Jedi
{
	#define MAX_SOUND_CHANNELS 16
	#define DEFAULT_SOUND_CHANNELS 8

	////////////////////////////////////////////////////
	// Structures
	////////////////////////////////////////////////////
	struct ImWaveData;

	struct ImWaveSound
	{
		ImWaveSound* prev;
		ImWaveSound* next;
		ImWaveData* data;
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

	struct ImWaveData
	{
		ImWaveSound* sound;
		s32 offset;
		s32 chunkSize;
		s32 baseOffset;
		s32 chunkIndex;
	};

	/////////////////////////////////////////////////////
	// Internal State
	/////////////////////////////////////////////////////
	atomic_s32 s_digitalPause = 0;
	atomic_s32 s_sndPlayerLock = 0;

	static ImWaveSound* s_imWaveSoundList = nullptr;
	static ImWaveSound  s_imWaveSound[MAX_SOUND_CHANNELS];
	static ImWaveData   s_imWaveData[MAX_SOUND_CHANNELS];
	static u8  s_imWaveChunkData[48];
	static s32 s_imWaveMixCount = DEFAULT_SOUND_CHANNELS;
	static s32 s_imWaveNanosecsPerSample;
	static iMuseInitData* s_imDigitalData;

	// In DOS these are 8-bit outputs since that is what the driver is accepting.
	// For TFE, floating-point audio output is used, so these convert to floating-point.
	static f32  s_audioNormalizationMem[DEFAULT_SOUND_CHANNELS * 256 + 4];
	// Normalizes the sum of all audio playback (16-bit) to a [-1,1) floating point value.
	// The mapping can be addressed with negative values (i.e. s_audioNormalization[-16]), which is why
	// it is built this way.
	static f32* s_audioNormalization = &s_audioNormalizationMem[DEFAULT_SOUND_CHANNELS * 128 + 4];

	static f32* s_audioDriverOut;
	static s16 s_audioOut[512];
	static s32 s_audioOutSize;
	static u8* s_audioData;
			
	extern void ImDigitalPlayerLock();
	extern void ImDigitalPlayerUnlock();
	extern s32 ImWrapValue(s32 value, s32 a, s32 b);
	extern s32 ImGetGroupVolume(s32 group);
	extern u8* ImInternalGetSoundData(ImSoundId soundId);

	ImWaveData* ImGetWaveData(s32 index);
	void ImFreeWaveSound(ImWaveSound* sound);
	s32 ImComputeAudioNormalization(iMuseInitData* initData);
	s32 ImSetWaveParamInternal(ImSoundId soundId, s32 param, s32 value);
	s32 ImGetWaveParamIntern(ImSoundId soundId, s32 param);
	s32 ImFreeWaveSoundByIdIntern(ImSoundId soundId);
	s32 ImStartDigitalSoundIntern(ImSoundId soundId, s32 priority, s32 chunkIndex);
	s32 audioPlaySoundFrame(ImWaveSound* sound);
	s32 audioWriteToDriver(f32 systemVolume);
		
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
		s_imWaveSoundList = nullptr;

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

		ImWaveSound* sound = s_imWaveSound;
		for (s32 i = 0; i < s_imWaveMixCount; i++, sound++)
		{
			sound->prev = nullptr;
			sound->next = nullptr;
			ImWaveData* data = ImGetWaveData(i);
			sound->data = data;
			data->sound = sound;
			sound->soundId = IM_NULL_SOUNDID;
		}

		TFE_Audio::setAudioThreadCallback(ImUpdateWave);

		s_sndPlayerLock = 0;
		return ImComputeAudioNormalization(initData);
	}

	void ImTerminateDigitalAudio()
	{
		ImFreeAllWaveSounds();
		TFE_Audio::setAudioThreadCallback();
		s_imWaveSoundList = nullptr;
	}

	s32 ImSetWaveParam(ImSoundId soundId, s32 param, s32 value)
	{
		ImDigitalPlayerLock();
		s32 res = ImSetWaveParamInternal(soundId, param, value);
		ImDigitalPlayerUnlock();
		return res;
	}

	s32 ImGetWaveParam(ImSoundId soundId, s32 param)
	{
		ImDigitalPlayerLock();
		s32 res = ImGetWaveParamIntern(soundId, param);
		ImDigitalPlayerUnlock();
		return res;
	}

	s32 ImStartDigitalSound(ImSoundId soundId, s32 priority)
	{
		ImDigitalPlayerLock();
		s32 res = ImStartDigitalSoundIntern(soundId, priority, 0);
		ImDigitalPlayerUnlock();
		return res;
	}
		
	void ImUpdateWave(f32* buffer, u32 bufferSize, f32 systemVolume)
	{
		// Prepare buffers.
		s_audioDriverOut = buffer;
		s_audioOutSize = bufferSize;
		memset(s_audioOut, 0, 2 * bufferSize * sizeof(s16));

		// Write sounds to s_audioOut.
		ImWaveSound* sound = s_imWaveSoundList;
		while (sound)
		{
			ImWaveSound* next = sound->next;
			audioPlaySoundFrame(sound);
			sound = next;
		}

		// Convert s_audioOut to "driver" buffer.
		audioWriteToDriver(systemVolume);
	}

	s32 ImPauseDigitalSound()
	{
		ImDigitalPlayerLock();
		s_digitalPause = 1;
		ImDigitalPlayerUnlock();
		return imSuccess;
	}

	s32 ImResumeDigitalSound()
	{
		ImDigitalPlayerLock();
		s_digitalPause = 0;
		ImDigitalPlayerUnlock();
		return imSuccess;
	}

	void ImDigitalPlayerLock()
	{
		s_sndPlayerLock++;
	}

	void ImDigitalPlayerUnlock()
	{
		if (s_sndPlayerLock)
		{
			s_sndPlayerLock--;
		}
	}

	////////////////////////////////////
	// Internal
	////////////////////////////////////
	ImWaveData* ImGetWaveData(s32 index)
	{
		assert(index < MAX_SOUND_CHANNELS);
		return &s_imWaveData[index];
	}

	s32 ImComputeAudioNormalization(iMuseInitData* initData)
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

			// These values are 8-bit in DOS, but converted to floating point for TFE.
			s_audioNormalization[i]    = f32(volumeMidPoint + volumeOffset)     / 128.0f - 1.0f;
			s_audioNormalization[-i-1] = f32(volumeMidPoint - volumeOffset - 1) / 128.0f - 1.0f;
		}
		return imSuccess;
	}

	s32 ImSetWaveParamInternal(ImSoundId soundId, s32 param, s32 value)
	{
		ImWaveSound* sound = s_imWaveSoundList;
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
				IM_LOG_ERR("TrSetParam() couldn't set param %lu...", param);
				return imArgErr;
			}
			sound = sound->next;
		}
		return imInvalidSound;
	}

	s32 ImGetWaveParamIntern(ImSoundId soundId, s32 param)
	{
		s32 soundCount = 0;
		ImWaveSound* sound = s_imWaveSoundList;
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
					return sound->data ? 1 : 0;
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

	ImWaveSound* ImAllocWaveSound(s32 priority)
	{
		ImWaveSound* sound = s_imWaveSound;
		ImWaveSound* newSound = nullptr;
		for (s32 i = 0; i < s_imWaveMixCount; i++, sound++)
		{
			if (!sound->soundId)
			{
				return sound;
			}
		}

		IM_LOG_WRN("ERR: no spare tracks...");
		s32 minPriority = 127;
		ImWaveSound* minPrioritySound = nullptr;
		sound = s_imWaveSoundList;
		while (sound)
		{
			if (sound->priority <= minPriority)
			{
				minPriority = sound->priority;
				minPrioritySound = sound;
			}
			sound = sound->next;
		}

		newSound = nullptr;
		if (sound)
		{
			if (priority >= minPriority)
			{
				ImFreeWaveSound(minPrioritySound);
				newSound = minPrioritySound;
			}
		}
		return newSound;
	}

	u8* ImGetChunkSoundData(s32 chunkIndex, s32 rangeMin, s32 rangeMax)
	{
		IM_LOG_ERR("Digital Sound chunk index should be zero in Dark Forces, but is %d.", chunkIndex);
		assert(0);
		return nullptr;
	}

	s32 ImSeekToNextChunk(ImWaveData* data)
	{
		while (1)
		{
			u8* chunkData = s_imWaveChunkData;
			u8* sndData = nullptr;

			if (data->chunkIndex)
			{
				sndData = ImGetChunkSoundData(data->chunkIndex, 0, 48);
				if (!sndData)
				{
					sndData = ImGetChunkSoundData(data->chunkIndex, 0, 1);
				}
				if (!sndData)
				{
					return imNotFound;
				}
			}
			else  // chunkIndex == 0
			{
				ImWaveSound* sound = data->sound;
				sndData = ImInternalGetSoundData(sound->soundId);
				if (!sndData)
				{
					if (sound->mailbox == 0)
					{
						sound->mailbox = 8;
					}
					IM_LOG_ERR("null sound addr in SeekToNextChunk()...");
					return imFail;
				}
			}

			memcpy(chunkData, sndData + data->offset, 48);
			u8 id = *chunkData;
			chunkData++;

			if (id == 0)
			{
				return imFail;
			}
			else if (id == 1)	// found the next useful chunk.
			{
				s32 chunkSize = (chunkData[0] | (chunkData[1] << 8) | (chunkData[2] << 16)) - 2;
				chunkData += 5;

				data->chunkSize = chunkSize;
				if (chunkSize > 220000)
				{
					ImWaveSound* sound = data->sound;
					if (sound->mailbox == 0)
					{
						sound->mailbox = 9;
					}
				}

				data->offset += 6;
				if (data->chunkIndex)
				{
					IM_LOG_ERR("data->chunkIndex should be 0 in Dark Forces, it is: %d.", data->chunkIndex);
					assert(0);
				}
				return imSuccess;
			}
			else if (id == 4)
			{
				chunkData += 3;
				ImSetSoundTrigger((ImSoundId)data->sound, chunkData);
				data->offset += 6;
			}
			else if (id == 6)
			{
				data->baseOffset = data->offset;
				data->offset += 6;
				if (data->chunkIndex != 0)
				{
					IM_LOG_ERR("data->chunkIndex should be 0 in Dark Forces, it is: %d.", data->chunkIndex);
					assert(0);
				}
			}
			else if (id == 7)
			{
				data->offset = data->baseOffset;
				if (data->chunkIndex != 0)
				{
					IM_LOG_ERR("data->chunkIndex should be 0 in Dark Forces, it is: %d.", data->chunkIndex);
					assert(0);
				}
			}
			else if (id == 'C')
			{
				if (chunkData[0] != 'r' || chunkData[1] != 'e' || chunkData[2] != 'a')
				{
					IM_LOG_ERR("ERR: Illegal chunk in sound %lu...", data->sound->soundId);
					return imFail;
				}
				data->offset += 26;
				if (data->chunkIndex)
				{
					IM_LOG_ERR("data->chunkIndex should be 0 in Dark Forces, it is: %d.", data->chunkIndex);
					assert(0);
				}
			}
			else
			{
				IM_LOG_ERR("ERR: Illegal chunk in sound %lu...", data->sound->soundId);
				return imFail;
			}
		}
		return imSuccess;
	}

	s32 ImWaveSetupSoundData(ImWaveSound* sound, s32 chunkIndex)
	{
		ImWaveData* data = sound->data;
		data->offset = 0;
		data->chunkSize = 0;
		data->baseOffset = 0;

		if (chunkIndex)
		{
			IM_LOG_ERR("data->chunkIndex should be 0 in Dark Forces, it is: %d.", chunkIndex);
			assert(0);
		}

		data->chunkIndex = 0;
		return ImSeekToNextChunk(data);
	}

	s32 ImStartDigitalSoundIntern(ImSoundId soundId, s32 priority, s32 chunkIndex)
	{
		priority = clamp(priority, 0, 127);
		ImWaveSound* sound = ImAllocWaveSound(priority);
		if (!sound)
		{
			return imFail;
		}

		sound->soundId = soundId;
		sound->marker = 0;
		sound->group = 0;
		sound->priority = priority;
		sound->volume = 128;
		sound->baseVolume = ImGetGroupVolume(0);
		sound->pan = 64;
		sound->detune = 0;
		sound->transpose = 0;
		sound->detuneTrans = 0;
		sound->mailbox = 0;
		if (ImWaveSetupSoundData(sound, chunkIndex) != imSuccess)
		{
			IM_LOG_ERR("Failed to setup wave player data - soundId: 0x%x, priority: %d", soundId, priority);
			return imFail;
		}

		ImDigitalPlayerLock();
		IM_LIST_ADD(s_imWaveSoundList, sound);
		ImDigitalPlayerUnlock();

		return imSuccess;
	}

	void ImFreeWaveSound(ImWaveSound* sound)
	{
		IM_LIST_REM(s_imWaveSoundList, sound);
		ImClearSoundFaders(sound->soundId, -1);
		ImClearTrigger(sound->soundId, -1, -1);
		sound->soundId = IM_NULL_SOUNDID;
	}

	s32 ImFreeWaveSoundById(ImSoundId soundId)
	{
		ImDigitalPlayerLock();
		s32 res = ImFreeWaveSoundByIdIntern(soundId);
		ImDigitalPlayerUnlock();
		return res;
	}

	s32 ImFreeAllWaveSounds()
	{
		ImWaveSound* sound = s_imWaveSoundList;
		ImDigitalPlayerLock();
		while (sound)
		{
			ImWaveSound* next = sound->next;
			ImFreeWaveSound(sound);
			sound = next;
		}
		ImDigitalPlayerUnlock();
		return imSuccess;
	}

	ImSoundId ImFindNextWaveSound(ImSoundId soundId)
	{
		ImSoundId nextSoundId = IM_NULL_SOUNDID;
		ImWaveSound* sound = s_imWaveSoundList;
		// Find the smallest ID that is greater than 'soundId' or NULL if soundId is the last one.
		while (sound)
		{
			if (u64(sound->soundId) > u64(soundId))
			{
				if (!nextSoundId || u64(sound->soundId) < u64(nextSoundId))
				{
					nextSoundId = sound->soundId;
				}
			}
			sound = sound->next;
		}
		return nextSoundId;
	}
	
	// leftMapping:  map left channel samples to final values based on volume and pan.
	// rightMapping: map right channel samples to final values based on volume and pan.
	void digitalAudioOutput_Stereo(s16* audioOut, const u8* sndData, const s8* leftMapping, const s8* rightMapping, s32 size)
	{
		for (; size > 0; size--, sndData++, audioOut+=2)
		{
			const u8 sample = *sndData;
			audioOut[0] += leftMapping[sample];
			audioOut[1] += rightMapping[sample];
		}
	}

	void audioProcessFrame(u8* audioFrame, s32 size, s32 outOffset, s32 vol, s32 pan)
	{
		s32 vTop = vol >> 3;
		if (vol)
		{
			vTop++;
		}
		if (vTop >= 17)
		{
			vTop = 1;
		}

		s32 panTop = (pan >> 3) - 8;
		if (pan > 64)
		{
			panTop++;
		}

		// Calculate where the in panVolume mapping channel to read from for each channel.
		s32 leftVolume  = s_audioPanVolumeTable[8 - panTop + vTop*17];
		s32 rightVolume = s_audioPanVolumeTable[8 + panTop + vTop*17];
		// Map [0,255] sample values to signed output values based on volume.
		const s8* leftMapping  = (s8*)&s_audioVolumeToSignedMapping[leftVolume  << 8];
		const s8* rightMapping = (s8*)&s_audioVolumeToSignedMapping[rightVolume << 8];

		digitalAudioOutput_Stereo(&s_audioOut[outOffset * 2], audioFrame, leftMapping, rightMapping, size);
	}

	s32 audioPlaySoundFrame(ImWaveSound* sound)
	{
		ImWaveData* data = sound->data;
		s32 bufferSize = s_audioOutSize;
		s32 offset = 0;
		s32 res = imSuccess;
		while (bufferSize > 0)
		{
			res = imSuccess;
			if (!data->chunkSize)
			{
				res = ImSeekToNextChunk(data);
				if (res != imSuccess)
				{
					if (res == imFail)  // Sound has finished playing.
					{
						ImFreeWaveSound(sound);
					}
					break;
				}
			}

			s32 readSize = (bufferSize <= data->chunkSize) ? bufferSize : data->chunkSize;
			s_audioData = ImInternalGetSoundData(sound->soundId) + data->offset;
			audioProcessFrame(s_audioData, readSize, offset, sound->volume, sound->pan);

			offset += readSize;
			bufferSize -= readSize;
			data->offset += readSize;
			data->chunkSize -= readSize;
		}
		return res;
	}

	s32 audioWriteToDriver(f32 systemVolume)
	{
		if (!s_audioOut)
		{
			return imInvalidSound;
		}

		s32 bufferSize = s_audioOutSize * 2;
		s16* audioOut  = s_audioOut;
		f32* driverOut = s_audioDriverOut;
		for (; bufferSize > 0; bufferSize--, audioOut++, driverOut++)
		{
			*driverOut = s_audioNormalization[*audioOut] * systemVolume;
		}
		return imSuccess;
	}

	s32 ImFreeWaveSoundByIdIntern(ImSoundId soundId)
	{
		s32 result = imInvalidSound;

		ImWaveSound* sound = s_imWaveSoundList;
		while (sound)
		{
			ImWaveSound* next = sound->next;
			if (sound->soundId == soundId)
			{
				ImFreeWaveSound(sound);
				result = imSuccess;
			}
			sound = next;
		}

		return result;
	}

}  // namespace TFE_Jedi
