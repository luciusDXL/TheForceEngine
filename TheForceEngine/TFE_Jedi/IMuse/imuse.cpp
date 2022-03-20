#include "imuse.h"
#include <TFE_Audio/midi.h>
#include <TFE_System/system.h>
#include <assert.h>
#include "imTrigger.h"
#include "imSoundFader.h"

namespace TFE_Jedi
{
	////////////////////////////////////////////////////
	// Structures
	////////////////////////////////////////////////////
	struct SoundPlayer;
	struct SoundChannel;
	struct ImMidiChannel;

	struct ImMidiChannel
	{
		SoundPlayer* player;
		SoundChannel* channel;
		ImMidiChannel* sharedMidiChannel;
		s32 channelId;

		s32 pgm;
		s32 priority;
		s32 noteReq;
		s32 volume;

		s32 pan;
		s32 modulation;
		s32 finalPan;
		s32 sustain;

		s32* instrumentMask;
		s32* instrumentMask2;
		SoundPlayer* sharedPart;

		s32 u3c;
		s32 u40;
		s32 sharedPartChannelId;
	};

	struct SoundChannel
	{
		ImMidiChannel* data;
		s32 partStatus;
		s32 partPgm;
		s32 partTrim;
		s32 partPriority;
		s32 priority;
		s32 partNoteReq;
		s32 partVolume;
		s32 groupVolume;
		s32 partPan;
		s32 modulation;
		s32 sustain;
		s32 pitchBend;
		s32 outChannelCount;
		s32 pan;
	};

	struct PlayerData
	{
		SoundPlayer* player;
		ImSoundId soundId;
		s32 seqIndex;
		s32 chunkOffset;
		s32 tick;
		s32 prevTick;
		s32 chunkPtr;
		s32 ticksPerBeat;
		s32 beatsPerMeasure;
		s32 tickFixed;
		s32 tempo;
		s32 step;
		s32 speed;
		s32 stepFixed;
	};

	struct SoundPlayer
	{
		SoundPlayer* prev;
		SoundPlayer* next;
		PlayerData* data;
		SoundPlayer* sharedPart;
		s32 sharedPartId;
		ImSoundId soundId;
		s32 marker;
		s32 group;
		s32 priority;
		s32 volume;
		s32 groupVolume;
		s32 pan;
		s32 detune;
		s32 transpose;
		s32 mailbox;
		s32 hook;

		SoundChannel channels[imChannelCount];
	};

	struct InstrumentSound
	{
		SoundPlayer* soundList;
		// Members used per sound.
		InstrumentSound* next;
		SoundPlayer* soundPlayer;
		s32 instrumentId;
		s32 channelId;
		s32 curTick;
		s32 curTickFixed;
	};

	struct ImSoundFader
	{
		s32 active;
		ImSoundId soundId;
		iMuseParameter param;
		s32 curValue;
		s32 timeRem;
		s32 fadeTime;
		s32 signedStep;
		s32 unsignedStep;
		s32 errAccum;
		s32 stepDir;
	};

	/////////////////////////////////////////////////////
	// Forward Declarations
	/////////////////////////////////////////////////////
	void ImSoundPlayerLock();
	void ImSoundPlayerUnlock();
	s32  ImPauseMidi();
	s32  ImPauseDigitalSound();
	s32  ImResumeMidi();
	s32  ImResumeDigitalSound();
	s32  ImHandleChannelGroupVolume();
	s32  ImGetGroupVolume(s32 group);
	void ImHandleChannelVolumeChange(SoundPlayer* player, SoundChannel* channel);
	void ImResetSoundChannel(SoundChannel* channel);
	void ImFreeMidiChannel(ImMidiChannel* channelData);
	void ImMidiSetupParts();
	void ImAssignMidiChannel(SoundPlayer* player, SoundChannel* channel, ImMidiChannel* midiChannel);
	u8*  ImGetSoundData(ImSoundId id);
	u8*  ImInternalGetSoundData(ImSoundId soundId);
	ImMidiChannel* ImGetFreeMidiChannel();
	s32 ImSetupPlayer(ImSoundId soundId, s32 priority);
	s32 ImFreeSoundPlayer(ImSoundId soundId);
	s32 ImReleaseAllPlayers();
	s32 ImReleaseAllWaveSounds();
	s32 ImGetSoundType(ImSoundId soundId);
	s32 ImSetMidiParam(ImSoundId soundId, s32 param, s32 value);
	s32 ImSetWaveParam(ImSoundId soundId, s32 param, s32 value);
	s32 ImGetMidiParam(ImSoundId soundId, s32 param);
	s32 ImGetWaveParam(ImSoundId soundId, s32 param);
	s32 ImGetPendingSoundCount(s32 soundId);
	ImSoundId ImFindNextMidiSound(ImSoundId soundId);
	ImSoundId ImFindNextWaveSound(ImSoundId soundId);
	SoundPlayer* ImGetSoundPlayer(ImSoundId soundId);
	s32 ImSetHookMidi(ImSoundId soundId, s32 value);

	// Midi channel commands
	void ImMidiChannelSetVolume(ImMidiChannel* midiChannel, s32 volume);
	void ImMidiChannelSetPgm(ImMidiChannel* midiChannel, s32 pgm);
	void ImMidiChannelSetPriority(ImMidiChannel* midiChannel, s32 priority);
	void ImMidiChannelSetPartNoteReq(ImMidiChannel* midiChannel, s32 noteReq);
	void ImMidiChannelSetPan(ImMidiChannel* midiChannel, s32 pan);
	void ImMidiChannelSetModulation(ImMidiChannel* midiChannel, s32 modulation);
	void ImHandleChannelPan(ImMidiChannel* midiChannel, s32 pan);
	void ImSetChannelSustain(ImMidiChannel* midiChannel, s32 sustain);
		
	/////////////////////////////////////////////////////
	// Internal State
	/////////////////////////////////////////////////////
	static const s32 s_channelMask[imChannelCount] =
	{
		(1 <<  0), (1 <<  1), (1 <<  2), (1 <<  3),
		(1 <<  4), (1 <<  5), (1 <<  6), (1 <<  7),
		(1 <<  8), (1 <<  9), (1 << 10), (1 << 11),
		(1 << 12), (1 << 13), (1 << 14), (1 << 15)
	};

	const char* c_midi = "MIDI";
	const u32 c_crea = 0x61657243;	// "Crea"

	static ImMidiChannel s_midiChannels[imChannelCount];
	static s32 s_imPause = 0;
	static s32 s_midiPaused = 0;
	static s32 s_digitalPause = 0;
	static s32 s_sndPlayerLock = 0;

	static SoundPlayer** s_soundPlayerList = nullptr;

	static s32 s_groupVolume[groupMaxCount] = { 0 };
	static s32 s_soundGroupVolume[groupMaxCount] =
	{
		imMaxVolume, imMaxVolume, imMaxVolume, imMaxVolume,
		imMaxVolume, imMaxVolume, imMaxVolume, imMaxVolume,
		imMaxVolume, imMaxVolume, imMaxVolume, imMaxVolume,
		imMaxVolume, imMaxVolume, imMaxVolume, imMaxVolume,
	};

	// Midi files loaded.
	static u32 s_midiFileCount = 0;
	static u8* s_midiFiles[6];

	/////////////////////////////////////////////////////////// 
	// Main API
	// -----------------------------------------------------
	// This includes both high level functions and low level
	// Note:
	// The original DOS code wrapped calls to an internal
	// dispatch function which is removed for TFE, calling
	// the functions directly instead.
	/////////////////////////////////////////////////////////// 

	////////////////////////////////////////////////////
	// High level functions
	////////////////////////////////////////////////////
	s32 ImSetMasterVol(s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetMasterVol(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetMusicVol(s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetMusicVol(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetSfxVol(s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetSfxVol(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetVoiceVol(s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetVoiceVol(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImStartSfx(ImSoundId soundId, s32 priority)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImStartVoice(ImSoundId soundId, s32 priority)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImStartMusic(ImSoundId soundId, s32 priority)
	{
		// Stub
		return imNotImplemented;
	}

	////////////////////////////////////////////////////
	// Low level functions
	////////////////////////////////////////////////////
	s32 ImInitialize()
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImTerminate(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImPause(void)
	{
		s32 res = imSuccess;
		if (!s_imPause)
		{
			res  = ImPauseMidi();
			res |= ImPauseDigitalSound();
		}
		s_imPause++;
		return (res == imSuccess) ? s_imPause : res;
	}

	s32 ImResume(void)
	{
		s32 res = imSuccess;
		if (s_imPause == 1)
		{
			res  = ImResumeMidi();
			res |= ImResumeDigitalSound();
		}
		if (s_imPause)
		{
			s_imPause--;
		}
		return (res == imSuccess) ? s_imPause : res;
	}

	s32 ImSetGroupVol(s32 group, s32 volume)
	{
		if (group >= groupMaxCount || volume > imMaxVolume)
		{
			return imArgErr;
		}
		else if (volume == imGetValue)
		{
			return s_groupVolume[group];
		}

		s32 groupVolume = s_groupVolume[group];
		if (group == groupMaster)
		{
			s_groupVolume[groupMaster] = volume;
			s_soundGroupVolume[groupMaster] = 0;
			for (s32 i = 1; i < groupMaxCount; i++)
			{
				s_soundGroupVolume[i] = ((s_groupVolume[i] + 1) * volume) >> imVolumeShift;
			}
		}
		else
		{
			s_groupVolume[group] = volume;
			s_soundGroupVolume[group] = ((s_groupVolume[groupMaster] + 1) * volume) >> imVolumeShift;
		}
		ImHandleChannelGroupVolume();
		// TODO: func_2e9689();

		return groupVolume;
	}

	s32 ImStartSound(ImSoundId soundId, s32 priority)
	{
		u8* data = ImInternalGetSoundData(soundId);
		if (!data)
		{
			TFE_System::logWrite(LOG_ERROR, "IMuse", "null sound addr in StartSound()");
			return imFail;
		}

		s32 i = 0;
		for (; i < 4; i++)
		{
			if (data[i] != c_midi[i])
			{
				break;
			}
		}
		if (i == 4)  // Midi
		{
			return ImSetupPlayer(soundId, priority);
		}
		else  // Digital Sound
		{
			// IM_TODO: Digital sound
		}
		return imSuccess;
	}

	s32 ImStopSound(ImSoundId soundId)
	{
		s32 type = ImGetSoundType(soundId);
		if (type == typeMidi)
		{
			return ImFreeSoundPlayer(soundId);
		}
		else if (type == typeWave)
		{
			// IM_TODO: Digital sound
		}
		return imFail;
	}

	s32 ImStopAllSounds(void)
	{
		s32 res = ImClearAllSoundFaders();
		res |= ImClearTriggersAndCmds();
		res |= ImReleaseAllPlayers();
		res |= ImReleaseAllWaveSounds();
		return res;
	}

	s32 ImGetNextSound(ImSoundId soundId)
	{
		ImSoundId nextMidiId = ImFindNextMidiSound(soundId);
		ImSoundId nextWaveId = ImFindNextWaveSound(soundId);
		if (nextMidiId == 0 || (nextWaveId > 0 && nextMidiId >= nextWaveId))
		{
			return nextWaveId;
		}
		return nextMidiId;
	}

	s32 ImSetParam(ImSoundId soundId, s32 param, s32 value)
	{
		iMuseSoundType type = (iMuseSoundType)ImGetSoundType(soundId);
		if (type == typeMidi)
		{
			return ImSetMidiParam(soundId, param, value);
		}
		else if (type == typeWave)
		{
			return ImSetWaveParam(soundId, param, value);
		}
		return imFail;
	}

	s32 ImGetParam(ImSoundId soundId, s32 param)
	{
		iMuseSoundType type = (iMuseSoundType)ImGetSoundType(soundId);
		if (param)
		{
			if (param == soundPendCount)
			{
				return ImGetPendingSoundCount(soundId);
			}
			else if (type == typeMidi)
			{
				return ImGetMidiParam(soundId, param);
			}
			else if (type == typeWave)
			{
				return ImGetWaveParam(soundId, param);
			}
			else if (param == soundPlayCount)
			{
				// This is zero here, since there is no valid type - 
				// so no sounds are that type are playing.
				return 0;
			}
			return imFail;
		}
		return type;
	}

	s32 ImSetHook(ImSoundId soundId, s32 value)
	{
		iMuseSoundType type = (iMuseSoundType)ImGetSoundType(soundId);
		if (type == typeMidi)
		{
			return ImSetHookMidi(soundId, value);
		}
		else if (type == typeWave)
		{
			// IM_TODO
		}
		return imFail;
	}

	s32 ImGetHook(ImSoundId soundId)
	{
		// Not called from Dark Forces.
		return imSuccess;
	}

	s32 ImJumpMidi(ImSoundId soundId, s32 chunk, s32 measure, s32 beat, s32 tick, s32 sustain)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSendMidiMsg(ImSoundId soundId, s32 arg1, s32 arg2, s32 arg3)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImShareParts(ImSoundId sound1, ImSoundId sound2)
	{
		// Stub
		return imNotImplemented;
	}

	/////////////////////////////////////////////////////////// 
	// Internal Implementation
	/////////////////////////////////////////////////////////// 
	s32 ImPauseMidi()
	{
		s_midiPaused = 1;
		return imSuccess;
	}

	s32 ImPauseDigitalSound()
	{
		ImSoundPlayerLock();
		s_digitalPause = 1;
		ImSoundPlayerUnlock();
		return imSuccess;
	}
		
	s32 ImResumeMidi()
	{
		s_midiPaused = 0;
		return imSuccess;
	}

	s32 ImResumeDigitalSound()
	{
		ImSoundPlayerLock();
		s_digitalPause = 0;
		ImSoundPlayerUnlock();
		return imSuccess;
	}
		
	void ImSoundPlayerLock()
	{
		s_sndPlayerLock++;
	}

	void ImSoundPlayerUnlock()
	{
		if (s_sndPlayerLock)
		{
			s_sndPlayerLock--;
		}
	}

	s32 ImHandleChannelGroupVolume()
	{
		SoundPlayer* player = *s_soundPlayerList;
		while (player)
		{
			s32 sndVol = player->volume + 1;
			s32 groupVol = ImGetGroupVolume(player->group);
			player->groupVolume = (sndVol * groupVol) >> imVolumeShift;

			for (s32 i = 0; i < imChannelCount; i++)
			{
				SoundChannel* channel = &player->channels[i];
				ImHandleChannelVolumeChange(player, channel);
			}
			player = player->next;
		}
		return imSuccess;
	}

	s32 ImGetGroupVolume(s32 group)
	{
		if (group >= groupMaxCount)
		{
			return imArgErr;
		}
		return s_soundGroupVolume[group];
	}

	void ImHandleChannelVolumeChange(SoundPlayer* player, SoundChannel* channel)
	{
		if (!channel->outChannelCount)
		{
			// This should never be hit.
			TFE_System::logWrite(LOG_ERROR, "IMuse", "Sound channel has 0 output channels.");
			assert(0);
		}
		s32 partTrim   = channel->partTrim   + 1;
		s32 partVolume = channel->partVolume + 1;
		channel->groupVolume = (partVolume * partTrim * player->groupVolume) >> (2*imVolumeShift);

		// These checks seem redundant. If groupVolume is != 0, then by definition partTrim and partVolume are != 0.
		if (!player->groupVolume || !channel->partTrim || !channel->partVolume)
		{
			if (!channel->data) { return; }
			ImResetSoundChannel(channel);
		}
		else if (channel->data) // has volume.
		{
			ImMidiChannelSetVolume(channel->data, channel->groupVolume);
		}
		ImMidiSetupParts();
	}

	void ImMidiSetupParts()
	{
		SoundPlayer* player = *s_soundPlayerList;
		SoundPlayer*  prevPlayer2  = nullptr;
		SoundPlayer*  prevPlayer   = nullptr;
		SoundChannel* prevChannel2 = nullptr;
		SoundChannel* prevChannel  = nullptr;
		s32 r;

		while (player)
		{
			for (s32 m = 0; m < imChannelCount; m++)
			{
				SoundChannel* channel = &player->channels[m];
				SoundPlayer* sharedPart = player->sharedPart;
				SoundChannel* sharedChannels = nullptr;
				if (player->sharedPart)
				{
					sharedChannels = player->sharedPart->channels;
				}
				SoundChannel* sharedChannels2 = sharedChannels;
				if (sharedChannels2)
				{
					if (channel->data && !sharedChannels->data && sharedChannels->partStatus)
					{
						SoundPlayer* sharedPart = player->sharedPart;
						if (sharedPart->groupVolume && sharedChannels->partTrim && sharedChannels->partVolume)
						{
							ImAssignMidiChannel(sharedPart, sharedChannels, channel->data);
						}
					}
					else if (!channel->data && sharedChannels2->data && channel->partStatus && player->groupVolume &&
						channel->partTrim && channel->partVolume)
					{
						ImAssignMidiChannel(player, channel, sharedChannels2->data->sharedMidiChannel);
					}
				}
				if (channel->data)
				{
					if (sharedChannels2 && sharedChannels2->data)
					{
						if (sharedChannels2->priority > channel->priority)
						{
							continue;
						}
					}
					if (prevChannel)
					{
						if (channel->priority > prevChannel->priority)
						{
							continue;
						}
					}
					prevPlayer = player;
					prevChannel = channel;
					r = 1;
				}
				else if (channel->partStatus && player->groupVolume && channel->partTrim && channel->partVolume)
				{
					if (!prevChannel2 || channel->priority > prevChannel2->priority)
					{
						prevPlayer2 = player;
						prevChannel2 = channel;
						r = 0;
					}
				}
			}
			player = player->next;

			// This will only start once all of the players has been exhausted.
			if (prevChannel2 && !player)
			{
				ImMidiChannel* midiChannel = ImGetFreeMidiChannel();
				SoundPlayer*  newPlayer  = nullptr;
				SoundChannel* newChannel = nullptr;
				if (midiChannel)
				{
					newPlayer  = prevPlayer2;
					newChannel = prevChannel2;
				}
				else
				{
					// IM_TODO:
				}
				ImAssignMidiChannel(newPlayer, newChannel, midiChannel);

				player = *s_soundPlayerList;
				prevPlayer2  = nullptr;
				prevPlayer   = nullptr;
				prevChannel2 = nullptr;
				prevChannel  = nullptr;
			}
		}
	}

	ImMidiChannel* ImGetFreeMidiChannel()
	{
		for (s32 i = 0; i < imChannelCount - 1; i++)
		{
			if (!s_midiChannels[i].player && !s_midiChannels[i].sharedPart)
			{
				return &s_midiChannels[i];
			}
		}
		return nullptr;
	}

	void ImAssignMidiChannel(SoundPlayer* player, SoundChannel* channel, ImMidiChannel* midiChannel)
	{
		if (!channel || !midiChannel)
		{
			return;
		}

		channel->data = midiChannel;
		midiChannel->player  = player;
		midiChannel->channel = channel;

		ImMidiChannelSetPgm(midiChannel, channel->partPgm);
		ImMidiChannelSetPriority(midiChannel, channel->priority);
		ImMidiChannelSetPartNoteReq(midiChannel, channel->partNoteReq);
		ImMidiChannelSetVolume(midiChannel, channel->groupVolume);
		ImMidiChannelSetPan(midiChannel, channel->partPan);
		ImMidiChannelSetModulation(midiChannel, channel->modulation);
		ImHandleChannelPan(midiChannel, channel->pan);
		ImSetChannelSustain(midiChannel, channel->sustain);
	}

	u8* ImGetSoundData(ImSoundId id)
	{
		if (id & imMidiFlag)
		{
			return s_midiFiles[id & imMidiMask];
		}
		else
		{
			// IM_TODO: Digital sound.
		}
		return nullptr;
	}

	s32 ImInternal_SoundValid(ImSoundId soundId)
	{
		if (soundId && soundId < imValidMask)
		{
			return 1;
		}
		return 0;
	}

	u8* ImInternalGetSoundData(ImSoundId soundId)
	{
		if (ImInternal_SoundValid(soundId))
		{
			return ImGetSoundData(soundId);
		}
		return nullptr;
	}

	s32 ImSetupPlayer(ImSoundId soundId, s32 priority)
	{
		// Stub: TODO
		return imNotImplemented;
	}

	void _ImNoteOff(s32 channelId, s32 instrId)
	{
		// Stub
	}

	void ImSendMidiMsg_(s32 channelId, MidiController ctrl, s32 value)
	{
		// Stub
	}

	void ImSendMidiMsg_(u8 channel, u8 msg, u8 arg1)
	{
		// Stub
	}

	void ImSendMidiMsg_R_(u8 channel, u8 msg)
	{
		// Stub
	}

	// For Pan, "Fine" resolution is 14-bit where 8192 (0x2000) is center - MID_PAN_LSB
	// Most devices use coarse adjustment instead (7 bits, 64 is center) - MID_PAN_MSB
	void ImSetPanFine_(s32 channel, s32 pan)
	{
		// Stub
	}
		
	void ImResetSoundChannel(SoundChannel* channel)
	{
		ImMidiChannel* data = channel->data;
		if (data)
		{
			ImFreeMidiChannel(data);
			data->player = nullptr;
			data->channel = nullptr;
			channel->data = nullptr;
		}
	}
		
	void ImFreeMidiChannel(ImMidiChannel* channelData)
	{
		if (channelData)
		{
			u32 channelMask = s_channelMask[channelData->channelId];
			channelData->sustain = 0;
			for (s32 i = 0; i < MIDI_INSTRUMENT_COUNT; i++)
			{
				s32 instrMask = channelData->instrumentMask[i];
				if (channelData->instrumentMask[i] & channelMask)
				{
					_ImNoteOff(channelData->channelId, i);
					channelData->instrumentMask[i] &= ~channelMask;
				}
				else if (channelData->instrumentMask2[i] & channelMask)
				{
					_ImNoteOff(channelData->channelId, i);
					channelData->instrumentMask2[i] &= ~channelMask;
				}
			}
		}
	}

	void ImReleaseSoundPlayer(SoundPlayer* player)
	{
		// imNotImplemented
	}

	s32 ImFreeSoundPlayer(ImSoundId soundId)
	{
		return imNotImplemented;
	}

	s32 ImReleaseAllPlayers()
	{
		SoundPlayer* player = nullptr;
		do
		{
			player = *s_soundPlayerList;
			if (player)
			{
				ImReleaseSoundPlayer(player);
			}
		} while (player);
		return imSuccess;
	}

	s32 ImReleaseAllWaveSounds()
	{
		/*
		** Stubbed
		ImWaveSound* snd = s_imWaveSounds;
		ImSoundPlayerLock();
		if (snd)
		{
			// IM_TODO
		}
		ImSoundPlayerUnlock();
		*/
		return imSuccess;
	}
		
	s32 ImGetSoundType(ImSoundId soundId)
	{
		return imNotImplemented;
	}

	s32 ImSetMidiParam(ImSoundId soundId, s32 param, s32 value)
	{
		return imNotImplemented;
	}

	s32 ImSetWaveParam(ImSoundId soundId, s32 param, s32 value)
	{
		return imNotImplemented;
	}

	s32 ImGetMidiParam(ImSoundId soundId, s32 param)
	{
		return imNotImplemented;
	}

	s32 ImGetWaveParam(ImSoundId soundId, s32 param)
	{
		return imNotImplemented;
	}

	s32 ImGetPendingSoundCount(s32 soundId)
	{
		// Not called by Dark Forces.
		assert(0);
		return 0;
	}

	// Search a list for the smallest non-zero value that is atleast minValue.
	ImSoundId ImFindNextMidiSound(ImSoundId soundId)
	{
		ImSoundId value = 0;
		SoundPlayer* entry = *s_soundPlayerList;
		while (entry)
		{
			if (soundId < entry->soundId)
			{
				if (!value || value > entry->soundId)
				{
					value = entry->soundId;
				}
			}
			entry = entry->next;
		}
		return value;
	}

	ImSoundId ImFindNextWaveSound(ImSoundId soundId)
	{
		ImSoundId nextSoundId = 0;
		/*
		** Stubbed **
		ImWaveSound* snd = s_imWaveSounds;
		while (snd)
		{
			if (snd->soundId > soundId)
			{
				if (nextSoundId)
				{
					// IM_TODO
				}
				nextSoundId = snd->soundId;
			}
			snd = snd->next;
		}
		*/
		return nextSoundId;
	}

	SoundPlayer* ImGetSoundPlayer(ImSoundId soundId)
	{
		SoundPlayer* player = *s_soundPlayerList;
		while (player)
		{
			if (player->soundId == soundId)
			{
				break;
			}
			player = player->next;
		}
		return player;
	}

	s32 ImSetHookMidi(ImSoundId soundId, s32 value)
	{
		if ((u32)value > 0x80000000ul)
		{
			return imArgErr;
		}
		SoundPlayer* player = ImGetSoundPlayer(soundId);
		if (!player)
		{
			return imInvalidSound;
		}
		player->hook = value;
		return imSuccess;
	}

	////////////////////////////////////////
	// Midi Commands
	////////////////////////////////////////
	void ImMidiChannelSetPgm(ImMidiChannel* midiChannel, s32 pgm)
	{
		if (midiChannel && pgm != midiChannel->pgm)
		{
			midiChannel->sharedMidiChannel->pgm = pgm;
			midiChannel->pgm = pgm;
			ImSendMidiMsg_R_(midiChannel->channelId, pgm);
		}
	}

	void ImMidiChannelSetPriority(ImMidiChannel* midiChannel, s32 priority)
	{
		if (midiChannel && priority != midiChannel->priority)
		{
			midiChannel->sharedMidiChannel->priority = priority;
			midiChannel->priority = priority;
			ImSendMidiMsg_(midiChannel->channelId, MID_GPC1_MSB, priority);
		}
	}

	void ImMidiChannelSetPartNoteReq(ImMidiChannel* midiChannel, s32 noteReq)
	{
		if (midiChannel && noteReq != midiChannel->noteReq)
		{
			midiChannel->sharedMidiChannel->noteReq = noteReq;
			midiChannel->noteReq = noteReq;
			ImSendMidiMsg_(midiChannel->channelId, MID_GPC2_MSB, noteReq);
		}
	}

	void ImMidiChannelSetPan(ImMidiChannel* midiChannel, s32 pan)
	{
		if (midiChannel && pan != midiChannel->pan)
		{
			midiChannel->sharedMidiChannel->pan = pan;
			midiChannel->pan = pan;
			ImSendMidiMsg_(midiChannel->channelId, MID_PAN_MSB, pan);
		}
	}

	void ImMidiChannelSetModulation(ImMidiChannel* midiChannel, s32 modulation)
	{
		if (midiChannel && modulation != midiChannel->modulation)
		{
			midiChannel->sharedMidiChannel->modulation = modulation;
			midiChannel->modulation = modulation;
			ImSendMidiMsg_(midiChannel->channelId, MID_MODULATIONWHEEL_MSB, modulation);
		}
	}

	void ImHandleChannelPan(ImMidiChannel* midiChannel, s32 pan)
	{
		if (midiChannel && pan != midiChannel->finalPan)
		{
			midiChannel->sharedMidiChannel->finalPan = pan;
			midiChannel->finalPan = pan;
			ImSetPanFine_(midiChannel->channelId, 2 * pan + 8192);
		}
	}

	void ImSetChannelSustain(ImMidiChannel* midiChannel, s32 sustain)
	{
		if (midiChannel)
		{
			midiChannel->sustain = sustain;
			if (!sustain)
			{
				for (s32 r = 0; r < MIDI_INSTRUMENT_COUNT; r++)
				{
					if (midiChannel->instrumentMask2[r] & s_channelMask[midiChannel->channelId])
					{
						// IM_TODO
					}
				}
			}
		}
	}

	void ImMidiChannelSetVolume(ImMidiChannel* midiChannel, s32 volume)
	{
		if (midiChannel && volume != midiChannel->volume)
		{
			midiChannel->sharedMidiChannel->volume = volume;
			midiChannel->volume = volume;
			ImSendMidiMsg_(midiChannel->channelId, MID_VOLUME_MSB, volume);
		}
	}
}
