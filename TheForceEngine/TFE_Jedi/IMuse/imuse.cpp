#include "imuse.h"
#include <TFE_Audio/midi.h>
#include <TFE_System/system.h>
#include <TFE_System/Threads/thread.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <assert.h>
#include "imTrigger.h"
#include "imSoundFader.h"
#include "midiData.h"

namespace TFE_Jedi
{
	#define IM_MAX_SOUNDS 32
	#define imuse_alloc(size) TFE_Memory::region_alloc(s_memRegion, size)
	#define imuse_realloc(ptr, size) TFE_Memory::region_realloc(s_memRegion, ptr, size)
	#define imuse_free(ptr) TFE_Memory::region_free(s_memRegion, ptr)
	#define IM_NULL_SOUNDID ImSoundId(0)

	////////////////////////////////////////////////////
	// Structures
	////////////////////////////////////////////////////
	struct ImMidiPlayer;
	struct ImMidiOutChannel;
	struct ImMidiChannel;

	struct ImMidiChannel
	{
		ImMidiPlayer* player;
		ImMidiOutChannel* channel;
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
		ImMidiPlayer* sharedPart;

		s32 u3c;
		s32 u40;
		s32 sharedPartChannelId;
	};

	struct ImMidiOutChannel
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
		ImMidiPlayer* player;
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

	struct ImList
	{
		ImList* prev;
		ImList* next;
	};

	struct ImMidiPlayer
	{
		ImMidiPlayer* prev;
		ImMidiPlayer* next;
		PlayerData* data;
		ImMidiPlayer* sharedPart;
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

		ImMidiOutChannel channels[imChannelCount];
	};

	struct InstrumentSound
	{
		ImMidiPlayer* soundList;
		// Members used per sound.
		InstrumentSound* next;
		ImMidiPlayer* midiPlayer;
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

	struct ImSound
	{
		ImSound* prev;
		ImSound* next;
		s32 id;
		char name[10];
		s16 u16;
		s32 u18;
		s32 u1c;
		s32 u20;
		s32 refCount;
	};

	typedef void(*MidiCmdFunc1)(PlayerData* playerData, u8* chunkData);
	typedef void(*MidiCmdFunc2)(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);
	union MidiCmdFunc
	{
		MidiCmdFunc1 func1;
		MidiCmdFunc2 func2;
	};

	static u8 s_midiMsgSize[] =
	{
		3, 3, 3, 3, 2, 2, 3, 1,
	};
	static s32 s_midiMessageSize2[] =
	{
		3, 3, 3, 3, 2, 2, 3,
	};

	enum ImMidiMessageIndex
	{
		IM_MID_NOTE_OFF = 0,
		IM_MID_NOTE_ON = 1,
		IM_MID_KEY_PRESSURE = 2,
		IM_MID_CONTROL_CHANGE = 3,
		IM_MID_PROGRAM_CHANGE = 4,
		IM_MID_CHANNEL_PRESSURE = 5,
		IM_MID_PITCH_BEND = 6,
		IM_MID_SYS_FUNC = 7,
		IM_MID_EVENT = 8,
		IM_MID_COUNT = 9,
	};
		
	/////////////////////////////////////////////////////
	// Forward Declarations
	/////////////////////////////////////////////////////
	void TFE_ImStartupThread();
	void TFE_ImShutdownThread();
		
	void ImUpdate();
	void ImUpdateMidi();
	void ImUpdateInstrumentSounds();
		
	void ImAdvanceMidi(PlayerData* playerData, u8* sndData, MidiCmdFunc* midiCmdFunc);
	void ImAdvanceMidiPlayer(PlayerData* playerData);
	void ImJumpSustain(ImMidiPlayer* player, u8* sndData, PlayerData* playerData1, PlayerData* playerData2);
	void ImMidiGetInstruments(ImMidiPlayer* player, s32* soundMidiInstrumentMask, s32* midiInstrumentCount);
	s32  ImMidiGetTickDelta(PlayerData* playerData, u32 prevTick, u32 tick);
	void ImMidiProcessSustain(PlayerData* playerData, u8* sndData, MidiCmdFunc* midiCmdFunc, ImMidiPlayer* player);
		
	void ImMidiPlayerLock();
	void ImMidiPlayerUnlock();
	void ImPrintMidiState();
	s32  ImPauseMidi();
	s32  ImPauseDigitalSound();
	s32  ImResumeMidi();
	s32  ImResumeDigitalSound();
	s32  ImHandleChannelGroupVolume();
	s32  ImGetGroupVolume(s32 group);
	void ImHandleChannelVolumeChange(ImMidiPlayer* player, ImMidiOutChannel* channel);
	void ImResetMidiOutChannel(ImMidiOutChannel* channel);
	void ImFreeMidiChannel(ImMidiChannel* channelData);
	void ImMidiSetupParts();
	void ImAssignMidiChannel(ImMidiPlayer* player, ImMidiOutChannel* channel, ImMidiChannel* midiChannel);
	u8*  ImGetSoundData(ImSoundId id);
	u8*  ImInternalGetSoundData(ImSoundId soundId);
	ImMidiChannel* ImGetFreeMidiChannel();
	ImMidiPlayer* ImGetFreePlayer(s32 priority);
	s32 ImStartMidiPlayerInternal(PlayerData* data, ImSoundId soundId);
	s32 ImSetupMidiPlayer(ImSoundId soundId, s32 priority);
	s32 ImFreeMidiPlayer(ImSoundId soundId);
	s32 ImReleaseAllPlayers();
	s32 ImReleaseAllWaveSounds();
	s32 ImGetSoundType(ImSoundId soundId);
	s32 ImSetMidiParam(ImSoundId soundId, s32 param, s32 value);
	s32 ImSetWaveParam(ImSoundId soundId, s32 param, s32 value);
	s32 ImGetMidiTimeParam(PlayerData* data, s32 param);
	s32 ImGetMidiParam(ImSoundId soundId, s32 param);
	s32 ImGetWaveParam(ImSoundId soundId, s32 param);
	s32 ImGetPendingSoundCount(s32 soundId);
	s32 ImWrapTranspose(s32 value, s32 a, s32 b);
	void ImHandleChannelPriorityChange(ImMidiPlayer* player, ImMidiOutChannel* channel);
	ImSoundId ImFindNextMidiSound(ImSoundId soundId);
	ImSoundId ImFindNextWaveSound(ImSoundId soundId);
	ImMidiPlayer* ImGetMidiPlayer(ImSoundId soundId);
	s32 ImSetHookMidi(ImSoundId soundId, s32 value);
	s32 ImFixupSoundTick(PlayerData* data, s32 value);
	s32 ImSetSequence(PlayerData* data, u8* sndData, s32 seqIndex);

	void ImMidiLock();
	void ImMidiUnlock();
	s32  ImGetDeltaTime();
	s32  ImMidiSetSpeed(PlayerData* data, u32 value);
	void ImSetTempo(PlayerData* data, u32 tempo);
	void ImSetMidiTicksPerBeat(PlayerData* data, s32 ticksPerBeat, s32 beatsPerMeasure);;
	void ImReleaseMidiPlayer(ImMidiPlayer* player);
	void ImRemoveInstrumentSound(ImMidiPlayer* player);

	s32 ImListRemove(ImList** list, ImList* item);
	s32 ImListAdd(ImList** list, ImList* item);

	ImSoundId ImFindMidi(const char* name);
	ImSoundId ImOpenMidi(const char* name);
	s32 ImCloseMidi(char* name);

	// Midi channel commands
	void ImMidiChannelSetVolume(ImMidiChannel* midiChannel, s32 volume);
	void ImHandleChannelDetuneChange(ImMidiPlayer* player, ImMidiOutChannel* channel);
	void ImHandleChannelPitchBend(ImMidiPlayer* player, s32 channelIndex, s32 fractValue, s32 intValue);
	void ImMidiChannelSetPgm(ImMidiChannel* midiChannel, s32 pgm);
	void ImMidiChannelSetPriority(ImMidiChannel* midiChannel, s32 priority);
	void ImMidiChannelSetPartNoteReq(ImMidiChannel* midiChannel, s32 noteReq);
	void ImMidiChannelSetPan(ImMidiChannel* midiChannel, s32 pan);
	void ImMidiChannelSetModulation(ImMidiChannel* midiChannel, s32 modulation);
	void ImHandleChannelPan(ImMidiChannel* midiChannel, s32 pan);
	void ImSetChannelSustain(ImMidiChannel* midiChannel, s32 sustain);

	void ImCheckForTrackEnd(PlayerData* playerData, u8* data);
	void ImMidiJump2_NoteOn(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);
	void ImMidiCommand(ImMidiPlayer* player, s32 channelIndex, s32 midiCmd, s32 value);
	void ImMidiStopAllNotes(ImMidiPlayer* player);

	void ImMidiNoteOff(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);
	void ImMidiNoteOn(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);
	void ImMidiProgramChange(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);
	void ImMidiPitchBend(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);
	void ImMidiEvent(PlayerData* playerData, u8* chunkData);

	void ImHandleNoteOff(ImMidiChannel* midiChannel, s32 instrumentId);
	void ImHandleNoteOn(ImMidiChannel* channel, s32 instrumentId, s32 velocity);
	void ImMidi_Channel9_NoteOn(s32 priority, s32 partNoteReq, s32 volume, s32 instrumentId, s32 velocity);
			
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
	static s32 s_iMuseTimestepMicrosec = 6944;

	static MemoryRegion* s_memRegion = nullptr;
	static s32 s_iMuseTimeInMicrosec = 0;
	static s32 s_iMuseTimeLong = 0;
	static s32 s_iMuseSystemTime = 0;

	static ImMidiChannel s_midiChannels[imChannelCount];
	static s32 s_imPause = 0;
	static s32 s_midiFrame = 0;
	static s32 s_midiPaused = 0;
	static s32 s_digitalPause = 0;
	static s32 s_sndPlayerLock = 0;
	static s32 s_midiLock = 0;
	static s32 s_trackTicksRemaining;
	static s32 s_midiTickDelta;
	static s32 s_midiTrackEnd;
	static s32 s_imEndOfTrack;
	static bool s_imEnableMusic = true;

	static atomic_bool s_imuseThreadActive;
	static Thread* s_thread = nullptr;
	
	static ImMidiPlayer s_midiPlayers[2];
	static ImMidiPlayer** s_midiPlayerList = nullptr;
	static PlayerData* s_imSoundHeaderCopy1 = nullptr;
	static PlayerData* s_imSoundHeaderCopy2 = nullptr;

	static s32 s_groupVolume[groupMaxCount] = { 0 };
	static s32 s_soundGroupVolume[groupMaxCount] =
	{
		imMaxVolume, imMaxVolume, imMaxVolume, imMaxVolume,
		imMaxVolume, imMaxVolume, imMaxVolume, imMaxVolume,
		imMaxVolume, imMaxVolume, imMaxVolume, imMaxVolume,
		imMaxVolume, imMaxVolume, imMaxVolume, imMaxVolume,
	};

	static s32 s_midiChannelTrim[imChannelCount] = { 0 };
	static s32 s_midiInstrumentChannelMask[MIDI_INSTRUMENT_COUNT];
	static s32 s_midiInstrumentChannelMask3[MIDI_INSTRUMENT_COUNT];
	static s32 s_midiInstrumentChannelMask2[MIDI_INSTRUMENT_COUNT];
	static s32 s_curMidiInstrumentMask[MIDI_INSTRUMENT_COUNT];
	static s32 s_curInstrumentCount;

	static ImSound s_sounds[IM_MAX_SOUNDS];
	static ImSound* s_soundList = nullptr;

	static s32 s_ImCh9_priority;
	static s32 s_ImCh9_partNoteReq;
	static s32 s_ImCh9_volume;

	// Midi functions for each message type - see ImMidiMessageIndex{} above.
	static MidiCmdFunc s_jumpMidiCmdFunc[IM_MID_COUNT] =
	{
		nullptr, nullptr, nullptr, nullptr,
		nullptr, nullptr, nullptr, nullptr,
		ImMidiEvent,
	};
	static MidiCmdFunc s_jumpMidiCmdFunc2[IM_MID_COUNT] =
	{
		nullptr, (MidiCmdFunc1)ImMidiJump2_NoteOn, nullptr, nullptr,
		nullptr, nullptr, nullptr, nullptr,
		ImCheckForTrackEnd
	};
	static MidiCmdFunc s_midiCmdFunc[IM_MID_COUNT] =
	{
		(MidiCmdFunc1)ImMidiNoteOff, (MidiCmdFunc1)ImMidiNoteOn, nullptr, (MidiCmdFunc1)ImMidiCommand,
		(MidiCmdFunc1)ImMidiProgramChange, nullptr, (MidiCmdFunc1)ImMidiPitchBend, nullptr,
		ImMidiEvent,
	};

	static InstrumentSound** s_imActiveInstrSounds = nullptr;
	static InstrumentSound** s_imInactiveInstrSounds = nullptr;

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
		
	s32 ImLoadMidi(const char *name)
	{
		if (!s_imEnableMusic || !name || !name[0])
		{
			return imFail;
		}

		ImSoundId soundId = ImFindMidi(name);
		if (soundId)
		{
			ImSound* sound = s_soundList;
			while (sound)
			{
				if (sound->id == soundId)
				{
					sound->refCount++;
					return soundId;
				}
				sound = sound->next;
			}
		}

		for (s32 n = 0; n < 10; n++)
		{
			ImSound* sound = &s_sounds[n];
			if (!sound->id)
			{
				if (strlen(name) >= 17)
				{
					TFE_System::logWrite(LOG_ERROR, "IMuse", "Name too long: %s", name);
					return imFail;
				}
				soundId = ImOpenMidi(name);
				if (!soundId)
				{
					TFE_System::logWrite(LOG_ERROR, "IMuse", "Host couldn't load sound");
					return imFail;
				}

				strcpy(sound->name, name);
				sound->id = soundId;
				sound->refCount = 1;
				ImListAdd((ImList**)&s_soundList, (ImList*)sound);
				return soundId;
			}
		}

		TFE_System::logWrite(LOG_ERROR, "IMuse", "Sound List FULL!");
		return imFail;
	}

	////////////////////////////////////////////////////
	// Low level functions
	////////////////////////////////////////////////////
	s32 ImInitialize(MemoryRegion* memRegion)
	{
		s_soundList = &s_sounds[0];
		memset(s_sounds, 0, sizeof(ImSound) * IM_MAX_SOUNDS);
		s_memRegion = memRegion;

		// In the original code, the interrupt is setup here, TFE uses a thread to simulate this.
		TFE_ImStartupThread();
		return imSuccess;
	}

	s32 ImTerminate(void)
	{
		TFE_ImShutdownThread();
		return imSuccess;
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
			return ImSetupMidiPlayer(soundId, priority);
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
			return ImFreeMidiPlayer(soundId);
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
		if (param == soundType)
		{
			return type;
		}
		else if (param == soundPendCount)
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
		ImMidiPlayer* player = ImGetMidiPlayer(soundId);
		if (!player) { return imInvalidSound; }

		PlayerData* data = player->data;
		u8* sndData = ImInternalGetSoundData(data->soundId);
		if (!sndData) { return imInvalidSound; }

		measure--;
		beat--;
		chunk--;
		if (measure >= 1000 || beat >= 12 || tick >= 480)
		{
			return imArgErr;
		}

		ImMidiLock();
		memcpy(s_imSoundHeaderCopy2, data, 56);
		memcpy(s_imSoundHeaderCopy1, data, 56);

		u32 newTick = ImFixupSoundTick(data, intToFixed16(measure)*16 + intToFixed16(beat) + tick);
		// If we are jumping backwards - we reset the chunk.
		if (chunk != s_imSoundHeaderCopy1->seqIndex || newTick < (u32)s_imSoundHeaderCopy1->chunkOffset)
		{
			if (ImSetSequence(s_imSoundHeaderCopy1, sndData, chunk))
			{
				TFE_System::logWrite(LOG_ERROR, "iMuse", "sq jump to invalid chunk.");
				ImMidiUnlock();
				return imFail;
			}
		}
		s_imSoundHeaderCopy1->tick = newTick;
		ImAdvanceMidi(s_imSoundHeaderCopy1, sndData, s_jumpMidiCmdFunc);
		if (s_imEndOfTrack)
		{
			TFE_System::logWrite(LOG_ERROR, "iMuse", "sq jump to invalid ms:bt:tk...");
			ImMidiUnlock();
			return imFail;
		}

		if (sustain)
		{
			// Loop through the channels.
			for (s32 c = 0; c < imChannelCount; c++)
			{
				ImMidiCommand(data->player, c, MID_SUSTAIN_SWITCH, 0);
				ImMidiCommand(data->player, c, MID_MODULATIONWHEEL_MSB, 0);
				ImHandleChannelPitchBend(data->player, c, 0, 64);
			}
			ImJumpSustain(data->player, sndData, s_imSoundHeaderCopy1, s_imSoundHeaderCopy2);
		}
		else
		{
			ImMidiStopAllNotes(data->player);
		}

		memcpy(data, s_imSoundHeaderCopy1, 56);
		s_imEndOfTrack = 1;
		ImMidiUnlock();

		return imSuccess;
	}

	s32 ImSendMidiMsg(ImSoundId soundId, s32 arg1, s32 arg2, s32 arg3)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImShareParts(ImSoundId sound1, ImSoundId sound2)
	{
		ImMidiPlayer* player1 = ImGetMidiPlayer(sound1);
		ImMidiPlayer* player2 = ImGetMidiPlayer(sound2);
		if (!player1 || player1->sharedPart || !player2 || player2->sharedPart)
		{
			return imFail;
		}

		player1->sharedPart = player2;
		player2->sharedPart = player1;
		player1->sharedPartId = sound2;
		player2->sharedPartId = sound1;
		ImMidiSetupParts();
		return imSuccess;
	}

	///////////////////////////////////////////////////////////
	// Internal "main loop"
	///////////////////////////////////////////////////////////
	// This is the equivalent of the iMuse interrupt handler in DOS.
	TFE_THREADRET TFE_ImThreadFunc(void* userData)
	{
		const f64 timeStep = TFE_System::microsecondsToSeconds((f64)ImGetDeltaTime());

		bool runThread = true;
		u64  localTime = 0;
		f64  accumTime = 0.0;
		while (runThread)
		{
			accumTime += TFE_System::updateThreadLocal(&localTime);
			while (accumTime >= timeStep)
			{
				ImUpdate();
				accumTime -= timeStep;
			}
			runThread = s_imuseThreadActive.load();
		}
		return (TFE_THREADRET)0;
	}
		
	// The equivalent of setting up a interrupt callback.
	void TFE_ImStartupThread()
	{
		s_imuseThreadActive.store(true);
		s_thread = Thread::create("IMuseThread", TFE_ImThreadFunc, nullptr);
		if (s_thread)
		{
			s_thread->run();
		}
	}

	void TFE_ImShutdownThread()
	{
		s_imuseThreadActive.store(false);
		if (s_thread->isPaused())
		{
			s_thread->resume();
		}
		s_thread->waitOnExit();

		delete s_thread;
		s_thread = nullptr;
	}
	
	// Main update entry point which is called at a fixed rate (see ImGetDeltaTime).
	// This is responsible for updating the midi players and updating digital audio playback.
	void ImUpdate()
	{
		const s32 dtInMicrosec = ImGetDeltaTime();

		// Update Midi and Audio
		ImUpdateMidi();
		// TODO: ImUpdateWave();
		if (s_imPause)
		{
			return;
		}

		// Update faders and deferred commands that update at a rate of 60 fps.
		s_iMuseTimeInMicrosec += dtInMicrosec;
		while (s_iMuseTimeInMicrosec >= 16667)
		{
			s_iMuseTimeInMicrosec -= 16667;
			s32 prevTime = s_iMuseSystemTime;
			s_iMuseSystemTime++;	// increment the system time every 1/60th of a second.

			ImUpdateSoundFaders();
			ImHandleDeferredCommands();
		}

		// Update Group Volumes every 6 "frames".
		s_iMuseTimeLong += dtInMicrosec;
		while (s_iMuseTimeLong >= 100000)	// ~6 frames @ 60Hz
		{
			s_iMuseTimeLong -= 100000;

			s32 musicVolume = ImSetGroupVol(groupMusic, imGetValue);
			ImSoundId soundId = 0;
			do
			{
				soundId = ImGetNextSound(soundId);
				if (soundId)
				{
					if (ImGetParam(soundId, soundGroup) == groupVoice)
					{
						musicVolume = (musicVolume * 82) >> imVolumeShift;
						break;
					}
				}
			} while (soundId);

			s32 dippedMusicVolume = ImSetGroupVol(groupDippedMusic, imGetValue);
			if (dippedMusicVolume > musicVolume)
			{
				dippedMusicVolume -= 6;
				if (dippedMusicVolume <= musicVolume)
				{
					dippedMusicVolume = musicVolume;
				}
				ImSetGroupVol(groupDippedMusic, dippedMusicVolume);
			}
			else if (dippedMusicVolume != musicVolume)
			{
				dippedMusicVolume += 3;
				if (dippedMusicVolume >= musicVolume)
				{
					dippedMusicVolume = musicVolume;
				}
				ImSetGroupVol(groupDippedMusic, dippedMusicVolume);
			}
		}
	}

	void ImUpdateMidi()
	{
		if (s_midiLock)
		{
			return;
		}

		ImMidiLock();
		s_midiFrame++;
		if (s_midiFrame > 75)  // Prints the iMuse state every 75 "frames".
		{
			s_midiFrame = 0;
			ImPrintMidiState();
		}
		if (!s_midiPaused)  // Update the midi state and instruments.
		{
			ImMidiPlayer* player = *s_midiPlayerList;
			while (player)
			{
				ImAdvanceMidiPlayer(player->data);
				player = player->next;
			}
			ImUpdateInstrumentSounds();
		}
		ImMidiUnlock();
	}

	/////////////////////////////////////////////////////////// 
	// Internal Implementation
	/////////////////////////////////////////////////////////// 
	void ImPrintMidiState()
	{
		// Compiled out in the original code.
		// TODO: Replace with TFE debug data.
	}

	void ImUpdateInstrumentSounds()
	{
		if (!s_imActiveInstrSounds)
		{
			return;
		}

		InstrumentSound* instrInfo = *s_imActiveInstrSounds;
		while (instrInfo)
		{
			InstrumentSound* next = instrInfo->next;
			ImMidiPlayer* player = instrInfo->midiPlayer;
			PlayerData* data = player->data;

			instrInfo->curTickFixed += data->stepFixed;
			s32 curTickInt = floor16(instrInfo->curTickFixed);
			instrInfo->curTick -= curTickInt;

			// Set the high bytes to 0.
			instrInfo->curTickFixed &= 0x0000ffff;
			if (instrInfo->curTick < 0)
			{
				ImMidiNoteOff(instrInfo->midiPlayer, instrInfo->channelId, instrInfo->instrumentId, 0);
				ImListRemove((ImList**)s_imActiveInstrSounds, (ImList*)instrInfo);
				ImListAdd((ImList**)s_imInactiveInstrSounds, (ImList*)instrInfo);
			}
			instrInfo = next;
		}
	}

	s32 ImPauseMidi()
	{
		s_midiPaused = 1;
		return imSuccess;
	}

	s32 ImPauseDigitalSound()
	{
		ImMidiPlayerLock();
		s_digitalPause = 1;
		ImMidiPlayerUnlock();
		return imSuccess;
	}
		
	s32 ImResumeMidi()
	{
		s_midiPaused = 0;
		return imSuccess;
	}

	s32 ImResumeDigitalSound()
	{
		ImMidiPlayerLock();
		s_digitalPause = 0;
		ImMidiPlayerUnlock();
		return imSuccess;
	}
		
	void ImMidiPlayerLock()
	{
		s_sndPlayerLock++;
	}

	void ImMidiPlayerUnlock()
	{
		if (s_sndPlayerLock)
		{
			s_sndPlayerLock--;
		}
	}

	s32 ImHandleChannelGroupVolume()
	{
		ImMidiPlayer* player = *s_midiPlayerList;
		while (player)
		{
			s32 sndVol = player->volume + 1;
			s32 groupVol = ImGetGroupVolume(player->group);
			player->groupVolume = (sndVol * groupVol) >> imVolumeShift;

			for (s32 i = 0; i < imChannelCount; i++)
			{
				ImMidiOutChannel* channel = &player->channels[i];
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

	void ImHandleChannelVolumeChange(ImMidiPlayer* player, ImMidiOutChannel* channel)
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
			ImResetMidiOutChannel(channel);
		}
		else if (channel->data) // has volume.
		{
			ImMidiChannelSetVolume(channel->data, channel->groupVolume);
		}
		ImMidiSetupParts();
	}

	void ImMidiSetupParts()
	{
		ImMidiPlayer* player = *s_midiPlayerList;
		ImMidiPlayer*  prevPlayer2  = nullptr;
		ImMidiPlayer*  prevPlayer   = nullptr;
		ImMidiOutChannel* prevChannel2 = nullptr;
		ImMidiOutChannel* prevChannel  = nullptr;
		s32 r;

		while (player)
		{
			for (s32 m = 0; m < imChannelCount; m++)
			{
				ImMidiOutChannel* channel = &player->channels[m];
				ImMidiPlayer* sharedPart = player->sharedPart;
				ImMidiOutChannel* sharedChannels = nullptr;
				if (player->sharedPart)
				{
					sharedChannels = player->sharedPart->channels;
				}
				ImMidiOutChannel* sharedChannels2 = sharedChannels;
				if (sharedChannels2)
				{
					if (channel->data && !sharedChannels->data && sharedChannels->partStatus)
					{
						ImMidiPlayer* sharedPart = player->sharedPart;
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
				ImMidiPlayer*  newPlayer  = nullptr;
				ImMidiOutChannel* newChannel = nullptr;
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

				player = *s_midiPlayerList;
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

	void ImAssignMidiChannel(ImMidiPlayer* player, ImMidiOutChannel* channel, ImMidiChannel* midiChannel)
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

	ImMidiPlayer* ImGetFreePlayer(s32 priority)
	{
		ImMidiPlayer* midiPlayer = s_midiPlayers;
		for (s32 i = 0; i < 2; i++, midiPlayer++)
		{
			if (!midiPlayer->soundId)
			{
				return midiPlayer;
			}
		}
		TFE_System::logWrite(LOG_ERROR, "iMuse", "no spare players");
		return nullptr;
	}
		
	s32 ImStartMidiPlayerInternal(PlayerData* data, ImSoundId soundId)
	{
		u8* sndData = ImInternalGetSoundData(soundId);
		if (!sndData)
		{
			return imInvalidSound;
		}

		data->soundId = soundId;
		data->tickFixed = 0;
		ImSetTempo(data, 500000); // microseconds per beat, 500000 = 120 beats per minute
		ImMidiSetSpeed(data, 128);
		ImSetMidiTicksPerBeat(data, 480, 4);
		return ImSetSequence(data, sndData, 0);
	}

	ImSoundId ImFindMidi(const char* name)
	{
		ImSound* sound = s_soundList;
		while (sound)
		{
			if (!strcasecmp(name, sound->name) && sound->refCount)
			{
				return sound->id;
			}
			sound = sound->next;
		}
		return IM_NULL_SOUNDID;
	}

	ImSoundId loadMidiFile(char* midiFile)
	{
		char midiPath[TFE_MAX_PATH];
		FilePath filePath;
		if (!TFE_Paths::getFilePath(midiPath, &filePath))
		{
			TFE_System::logWrite(LOG_ERROR, "IMuse", "Cannot find midi file '%s'.", midiFile);
			return IM_NULL_SOUNDID;
		}
		FileStream file;
		if (!file.open(&filePath, FileStream::MODE_READ))
		{
			TFE_System::logWrite(LOG_ERROR, "IMuse", "Cannot open midi file '%s'.", midiFile);
			return IM_NULL_SOUNDID;
		}
		size_t len = file.getSize();
		s_midiFiles[s_midiFileCount] = (u8*)imuse_alloc(len);
		file.readBuffer(s_midiFiles[s_midiFileCount], u32(len));
		file.close();

		ImSoundId id = ImSoundId(s_midiFileCount | imMidiFlag);
		s_midiFileCount++;
		return id;
	}

	ImSoundId ImOpenMidi(const char* name)
	{
		char filename[32];
		strcpy(filename, name);
		strcat(filename, ".gmd");
		return loadMidiFile(filename);
	}

	s32 ImCloseMidi(char* name)
	{
		ImSound* sound = s_soundList;
		while (sound)
		{
			if (!strcasecmp(sound->name, name) && sound->refCount)
			{
				sound->refCount--;
				if (sound->refCount <= 0)
				{
					u32 index = sound->id & imMidiMask;
					imuse_free(s_midiFiles[index]);
					s_midiFiles[index] = nullptr;

					sound->id = IM_NULL_SOUNDID;
					sound->refCount = 0;
				}
				break;
			}
			sound = sound->next;
		}
		return imSuccess;
	}

	s32 ImListAdd(ImList** list, ImList* item)
	{
		if (!item || item->next || item->prev)
		{
			TFE_System::logWrite(LOG_ERROR, "iMuse", "List arg err when adding");
			return imArgErr;
		}

		item->next = *list;
		if (*list)
		{
			item->next->prev = item;
		}

		item->prev = nullptr;
		*list = item;
		return imSuccess;
	}

	s32 ImSetupMidiPlayer(ImSoundId soundId, s32 priority)
	{
		s32 clampedPriority = clamp(priority, 0, imMaxPriority);
		ImMidiPlayer* player = ImGetFreePlayer(clampedPriority);
		if (!player)
		{
			return imAllocErr;
		}

		player->sharedPart = nullptr;
		player->sharedPartId = 0;
		player->marker = -1;
		player->group = groupMusic;
		player->priority = clampedPriority;
		player->volume = imMaxVolume;
		player->groupVolume = ImGetGroupVolume(player->group);
		player->pan = imPanCenter;

		player->detune = 0;
		player->transpose = 0;
		player->mailbox = 0;
		player->hook = 0;

		for (s32 i = 0; i < imChannelCount; i++)
		{
			ImMidiOutChannel* channel = &player->channels[i];

			channel->data = nullptr;
			channel->partStatus = 0;
			channel->partPgm = 128;
			channel->partTrim = imMaxVolume;
			channel->partPriority = 0;
			channel->priority = player->priority;
			channel->partNoteReq = 1;
			channel->partVolume = imMaxVolume;
			channel->groupVolume = player->groupVolume;
			channel->partPan = imPanCenter;
			channel->modulation = 0;
			channel->sustain = 0;
			channel->pitchBend = 0;
			channel->outChannelCount = 2;
			channel->pan = 0;
		}

		PlayerData* playerData = player->data;
		if (ImStartMidiPlayerInternal(playerData, soundId))
		{
			return imFail;
		}

		player->soundId = soundId;
		ImListAdd((ImList**)s_midiPlayerList, (ImList*)player);
		return imSuccess;
	}

	void _ImNoteOff(s32 channelId, s32 instrId)
	{
		// Stub
	}

	void _ImNoteOn(s32 channelId, s32 instrId, s32 velocity)
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
		
	void ImResetMidiOutChannel(ImMidiOutChannel* channel)
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

	void ImSetEndOfTrack()
	{
		s_imEndOfTrack = 1;
	}
				
	s32 ImListRemove(ImList** list, ImList* item)
	{
		ImList* curItem = *list;
		if (!item || !curItem)
		{
			TFE_System::logWrite(LOG_ERROR, "iMuse", "List arg err when removing.");
			return imArgErr;
		}
		while (curItem && item != curItem)
		{
			curItem = curItem->next;
		}
		if (!curItem)
		{
			TFE_System::logWrite(LOG_ERROR, "iMuse", "Item not on list.");
			return imNotFound;
		}

		if (item->next)
		{
			item->next->prev = item->prev;
		}
		if (item->prev)
		{
			item->prev->next = item->next;
		}
		else
		{
			*list = item->next;
		}
		item->next = nullptr;
		item->prev = nullptr;

		return imSuccess;
	}

	void ImRemoveInstrumentSound(ImMidiPlayer* player)
	{
		InstrumentSound* instrInfo = *s_imActiveInstrSounds;
		while (instrInfo)
		{
			InstrumentSound* next = instrInfo->next;
			if (player == instrInfo->midiPlayer)
			{
				ImListRemove((ImList**)s_imActiveInstrSounds, (ImList*)instrInfo);
				ImListAdd((ImList**)s_imInactiveInstrSounds, (ImList*)instrInfo);
			}
			instrInfo = next;
		}
	}

	void ImReleaseMidiPlayer(ImMidiPlayer* player)
	{
		s32 res = ImListRemove((ImList**)s_midiPlayerList, (ImList*)player);
		ImClearSoundFaders(player->soundId, -1);
		ImClearTrigger(player->soundId, -1, -1);
		ImRemoveInstrumentSound(player);
		ImSetEndOfTrack();

		for (s32 i = 0; i < imChannelCount; i++)
		{
			ImMidiOutChannel* channel = &player->channels[i];
			ImResetMidiOutChannel(channel);
		}

		player->soundId = 0;
		player->sharedPart;
		if (player->sharedPart)
		{
			player->sharedPart->sharedPartId = 0;
			player->sharedPart = nullptr;
		}
		ImMidiSetupParts();
	}

	s32 ImFreeMidiPlayer(ImSoundId soundId)
	{
		ImMidiPlayer* player = ImGetMidiPlayer(soundId);
		if (player)
		{
			ImReleaseMidiPlayer(player);
			// This should be null - but the original code handles duplicates.
			do
			{
				player = ImGetMidiPlayer(soundId);
				if (player)
				{
					ImReleaseMidiPlayer(player);
				}
			} while (player);
			return imSuccess;
		}
		return imFail;
	}

	s32 ImReleaseAllPlayers()
	{
		ImMidiPlayer* player = nullptr;
		do
		{
			player = *s_midiPlayerList;
			if (player)
			{
				ImReleaseMidiPlayer(player);
			}
		} while (player);
		return imSuccess;
	}

	s32 ImReleaseAllWaveSounds()
	{
		/*
		** Stubbed
		ImWaveSound* snd = s_imWaveSounds;
		ImMidiPlayerLock();
		if (snd)
		{
			// IM_TODO
		}
		ImMidiPlayerUnlock();
		*/
		return imSuccess;
	}
		
	s32 ImGetSoundType(ImSoundId soundId)
	{
		ImSoundId foundId = 0;
		// First check midi sounds.
		do
		{
			foundId = ImFindNextMidiSound(foundId);
			if (foundId && foundId == soundId)
			{
				return typeMidi;
			}
		} while (foundId);

		// Then check for wave sounds.
		foundId = 0;
		do
		{
			foundId = ImFindNextWaveSound(foundId);
			if (foundId && foundId == soundId)
			{
				return typeWave;
			}
		} while (foundId);

		// The sound doesn't exist.
		return imFail;
	}

	void ImHandleChannelPriorityChange(ImMidiPlayer* player, ImMidiOutChannel* channel)
	{
		channel->priority = channel->partPriority + player->priority;
		if (channel->data)
		{
			ImMidiChannelSetPriority(channel->data, channel->priority);
		}
	}

	s32 ImWrapTranspose(s32 value, s32 a, s32 b)
	{
		while (value < a)
		{
			value += 12;
		}
		while (value > b)
		{
			value -= 12;
		}
		return value;
	}

	s32 ImSetMidiParam(ImSoundId soundId, s32 param, s32 value)
	{
		ImMidiPlayer* player = ImGetMidiPlayer(soundId);
		if (!player)
		{
			return imInvalidSound;
		}

		s32 midiChannel = param & 0x00ff;
		iMuseParameter imParam = iMuseParameter(param & 0xff00);
		ImMidiOutChannel* channels = player->channels;

		if (imParam == soundGroup)
		{
			if (value >= groupMaxCount) { return imArgErr; }

			player->group = value;
			s32 playerVol = player->volume + 1;
			s32 groupVol = ImGetGroupVolume(value);
			player->groupVolume = (playerVol * groupVol) >> 7;

			for (s32 m = 0; m < imChannelCount; m++)
			{
				ImHandleChannelVolumeChange(player, &channels[m]);
			}
		}
		else if (imParam == soundPriority && value <= imMaxPriority)
		{
			player->priority = value;
			for (s32 m = 0; m < imChannelCount; m++)
			{
				ImHandleChannelPriorityChange(player, &channels[m]);
			}
		}
		else if (imParam == soundVol && value <= imMaxVolume)
		{
			player->volume = value;
			s32 groupVol = ImGetGroupVolume(player->group);
			player->groupVolume = ((value + 1) * groupVol) >> imVolumeShift;

			for (s32 m = 0; m < imChannelCount; m++)
			{
				ImHandleChannelVolumeChange(player, &channels[m]);
			}
		}
		else if (imParam == soundPan && value <= imPanMax)
		{
			player->pan = value;
			for (s32 m = 0; m < imChannelCount; m++)
			{
				s32 pan = clamp(channels[m].partPan + value - imPanCenter, 0, imPanMax);
				ImMidiChannelSetPan(channels[m].data, pan);
			}
		}
		else if (imParam == soundDetune)
		{
			if (value < -9216 || value > 9216) { return imArgErr; }
			player->detune = value;
			for (s32 m = 0; m < imChannelCount; m++)
			{
				ImHandleChannelDetuneChange(player, &channels[m]);
			}
		}
		else if (imParam == soundTranspose)
		{
			if (value < -12 || value > 12) { return imArgErr; }
			player->transpose = (value == 0) ? 0 : ImWrapTranspose(value + player->transpose, -12, 12);

			for (s32 m = 0; m < imChannelCount; m++)
			{
				ImHandleChannelDetuneChange(player, &channels[m]);
			}
		}
		else if (imParam == soundMailbox)
		{
			player->mailbox = value;
		}
		else if (imParam == midiSpeed)
		{
			return ImMidiSetSpeed(player->data, value);
		}
		else if (imParam == midiPartTrim)
		{
			if (midiChannel >= imChannelCount || value > imMaxVolume) { return imArgErr; }

			channels[midiChannel].partTrim = value;
			ImHandleChannelVolumeChange(player, &channels[midiChannel]);
		}
		else
		{
			TFE_System::logWrite(LOG_ERROR, "iMuse", "PlSetParam() couldn't set param %lu.", param);
			return imArgErr;
		}

		return imSuccess;
	}

	s32 ImSetWaveParam(ImSoundId soundId, s32 param, s32 value)
	{
		return imNotImplemented;
	}

	s32 ImGetMidiTimeParam(PlayerData* data, s32 param)
	{
		if (param == midiChunk)
		{
			return data->seqIndex + 1;
		}
		else if (param == midiMeasure)
		{
			return (data->tick >> 14) + 1;
		}
		else if (param == midiBeat)
		{
			return ((data->tick & 0xf0000) >> 16) + 1;
		}
		else if (param == midiTick)
		{
			return data->tick & 0xffff;
		}
		else if (param == midiSpeed)
		{
			return data->speed;
		}
		return imArgErr;
	}

	s32 ImGetMidiParam(ImSoundId soundId, s32 param)
	{
		s32 playCount = 0;
		ImMidiPlayer* player = *s_midiPlayerList;
		while (player)
		{
			if (player->soundId == soundId)
			{
				s32 midiChannel = param & 0x00ff;
				iMuseParameter imParam = iMuseParameter(param & 0xff00);
				ImMidiOutChannel* channel = &player->channels[midiChannel];

				switch (imParam)
				{
					case soundPlayCount:
					{
						playCount++;
					} break;
					case soundPendCount:
					{
						return imFail;
					} break;
					case soundMarker:
					{
						return player->marker;
					} break;
					case soundGroup:
					{
						return player->group;
					} break;
					case soundPriority:
					{
						return player->priority;
					} break;
					case soundVol:
					{
						return player->volume;
					} break;
					case soundPan:
					{
						return player->pan;
					} break;
					case soundDetune:
					{
						return player->detune;
					} break;
					case soundTranspose:
					{
						return player->transpose;
					} break;
					case soundMailbox:
					{
						return player->mailbox;
					} break;
					case midiChunk:
					case midiMeasure:
					case midiBeat:
					case midiTick:
					case midiSpeed:
					{
						return ImGetMidiTimeParam(player->data, param);
					} break;
					case midiPartTrim:
					{
						return channel->partTrim;
					} break;
					case midiPartStatus:
					{
						return channel->partStatus ? 1 : 0;
					} break;
					case midiPartPriority:
					{
						return channel->partPriority;
					} break;
					case midiPartNoteReq:
					{
						return channel->partNoteReq;
					} break;
					case midiPartVol:
					{
						return channel->partVolume;
					} break;
					case midiPartPan:
					{
						return channel->partPan;
					} break;
					case midiPartPgm:
					{
						return channel->partPgm;
					} break;
					default:
					{
						return imArgErr;
					}
				};
			}
			player = player->next;
		}
		if (param == soundPlayCount)
		{
			return playCount;
		}

		return imInvalidSound;
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
		ImMidiPlayer* player = *s_midiPlayerList;
		while (player)
		{
			if (soundId < player->soundId)
			{
				if (!value || value > player->soundId)
				{
					value = player->soundId;
				}
			}
			player = player->next;
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

	ImMidiPlayer* ImGetMidiPlayer(ImSoundId soundId)
	{
		ImMidiPlayer* player = *s_midiPlayerList;
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
		ImMidiPlayer* player = ImGetMidiPlayer(soundId);
		if (!player)
		{
			return imInvalidSound;
		}
		player->hook = value;
		return imSuccess;
	}

	void ImMidiLock()
	{
		s_midiLock++;
	}

	void ImMidiUnlock()
	{
		if (s_midiLock)
		{
			s_midiLock--;
		}
	}

	// Gets the deltatime in microseconds (i.e. 1,000 ms / 1 microsec).
	s32 ImGetDeltaTime()
	{
		return s_iMuseTimestepMicrosec;
	}
		
	s32 ImMidiSetSpeed(PlayerData* data, u32 value)
	{
		if (value > 255)
		{
			return imArgErr;
		}

		ImMidiLock();
		data->speed = value;
		data->stepFixed = (value * data->step) >> 7;
		ImMidiUnlock();
		return imSuccess;
	}
		
	void ImSetTempo(PlayerData* data, u32 tempo)
	{
		s32 ticks = ImGetDeltaTime() * 480;
		data->tempo = tempo;

		// Keep reducing the tick scale? until it is less than one?
		// This also reduces the tempo as well.
		while (1)
		{
			if (!(ticks & 0xffff0000))
			{
				if (!(tempo & 0xffff0000))
				{
					break;
				}
				ticks >>= 1;
				tempo >>= 1;
			}
			else
			{
				ticks >>= 1;
				tempo >>= 1;
			}
		}

		data->step = intToFixed16(ticks) / tempo;
		ImMidiSetSpeed(data, data->speed);
	}

	void ImSetMidiTicksPerBeat(PlayerData* data, s32 ticksPerBeat, s32 beatsPerMeasure)
	{
		ImMidiLock();
		data->ticksPerBeat = ticksPerBeat;
		data->beatsPerMeasure = intToFixed16(beatsPerMeasure);
		ImMidiUnlock();
	}

	s32 ImSetSequence(PlayerData* data, u8* sndData, s32 seqIndex)
	{
		u8* chunk = midi_gotoHeader(sndData, "MTrk", seqIndex);
		if (!chunk)
		{
			TFE_System::logWrite(LOG_ERROR, "iMuse", "Sq couldn't find chunk %d", seqIndex + 1);
			return imFail;
		}

		// Skip the header and size to the data.
		u8* chunkData = &chunk[8];
		data->seqIndex = seqIndex;
		// offset of the chunk data from the base sound data.
		data->chunkOffset = s32(chunkData - sndData);

		u32 dt = midi_getVariableLengthValue(&chunkData);
		data->prevTick = ImFixupSoundTick(data, dt);
		data->tick = 0;
		data->chunkPtr = s32(chunkData - (sndData + data->chunkOffset));
		return imSuccess;
	}

	s32 ImFixupSoundTick(PlayerData* data, s32 value)
	{
		while ((value & 0xffff) >= data->ticksPerBeat)
		{
			value = value - data->ticksPerBeat + ONE_16;
			while ((value & FIXED(15)) >= data->beatsPerMeasure)
			{
				value = value - data->beatsPerMeasure + ONE_16;
			}
		}
		return value;
	}
						
	// Advance the current sound to the next tick.
	void ImAdvanceMidi(PlayerData* playerData, u8* sndData, MidiCmdFunc* midiCmdFunc)
	{
		s_imEndOfTrack = 0;
		s32 prevTick = playerData->prevTick;
		s32 tick = playerData->tick;
		u8* chunkData = sndData + playerData->chunkOffset + playerData->chunkPtr;
		while (tick > prevTick)
		{
			u8 data = chunkData[0];
			s32 idx = 0;
			if (data < 0xf0)
			{
				if (data & 0x80)
				{
					u8 msgType = (data & 0x70) >> 4;
					u8 channel = data & 0x0f;
					if (midiCmdFunc[msgType].func2)
					{
						midiCmdFunc[msgType].func2(playerData->player, channel, chunkData[1], chunkData[2]);
					}
					chunkData += s_midiMsgSize[msgType];
				}
			}
			else
			{
				if (data == 0xf0)
				{
					idx = IM_MID_SYS_FUNC;
				}
				else
				{
					if (data != 0xff)
					{
						TFE_System::logWrite(LOG_ERROR, "iMuse", "sq unknown msg type 0x%x...", data);
						ImReleaseMidiPlayer(playerData->player);
						return;
					}

					idx = IM_MID_EVENT;
					chunkData++;
				}
				if (midiCmdFunc[idx].func1)
				{
					midiCmdFunc[idx].func1(playerData, chunkData);
				}

				// This steps past the current chunk, if we don't know what a chunk is, it should be skipped.
				chunkData++;
				// chunkSize is a variable length value.
				chunkData += midi_getVariableLengthValue(&chunkData);
			}

			if (s_imEndOfTrack)
			{
				TFE_System::logWrite(LOG_ERROR, "iMuse", "ImAdvanceMidi() - Invalid end of track encountered.");
				return;
			}

			u32 dt = midi_getVariableLengthValue(&chunkData);
			prevTick += dt;
			if ((prevTick & 0xffff) >= playerData->ticksPerBeat)
			{
				prevTick = ImFixupSoundTick(playerData, prevTick);
			}
		}
		playerData->prevTick = prevTick;
		u8* chunkBase = sndData + playerData->chunkOffset;
		playerData->chunkPtr = s32(chunkData - chunkBase);
	}

	// This gets called at a fixed rate, where each step = 'stepFixed' ticks.
	void ImAdvanceMidiPlayer(PlayerData* playerData)
	{
		playerData->tickFixed += playerData->stepFixed;
		playerData->tick += floor16(playerData->tickFixed);
		playerData->tickFixed &= 0xffff0000;

		if ((playerData->tick & 0xffff) >= playerData->ticksPerBeat)
		{
			playerData->tick = ImFixupSoundTick(playerData, playerData->tick);
		}
		if (playerData->prevTick < playerData->tick)
		{
			u8* sndData = ImInternalGetSoundData(playerData->soundId);
			if (sndData)
			{
				ImAdvanceMidi(playerData, sndData, s_midiCmdFunc);
				return;
			}
			TFE_System::logWrite(LOG_ERROR, "iMuse", "sq int handler got null addr");
			// TODO: ERROR handling
		}
	}
		
	void ImMidiGetInstruments(ImMidiPlayer* player, s32* soundMidiInstrumentMask, s32* midiInstrumentCount)
	{
		*midiInstrumentCount = 0;

		u32 channelMask = 0;
		// Determine which outout channels that this sound is playing on.
		// The result is a 16 bit mask. For example, if the sound is only playing on channel 1,
		// the resulting mask = 1<<1 = 2.
		for (s32 i = 0; i < imChannelCount; i++)
		{
			ImMidiChannel* midiChannel = &s_midiChannels[i];
			if (player == midiChannel->player)
			{
				channelMask |= s_channelMask[midiChannel->channelId];
			}
		}

		for (s32 i = 0; i < MIDI_INSTRUMENT_COUNT; i++)
		{
			u32 value = s_midiInstrumentChannelMask[i] & channelMask;
			soundMidiInstrumentMask[i] = value;
			// Count the number of channels currently playing the instrument.
			// This is counting the bits.
			while (value)
			{
				(*midiInstrumentCount) += (value & 1);
				value >>= 1;
			}
		}

		channelMask = 0;
		for (s32 i = 0; i < imChannelCount - 1; i++)
		{
			ImMidiChannel* midiChannel = &s_midiChannels[i];
			if (player == midiChannel->sharedPart)
			{
				channelMask |= s_channelMask[midiChannel->sharedPartChannelId];
			}
		}

		for (s32 i = 0; i < MIDI_INSTRUMENT_COUNT; i++)
		{
			u32 value = s_midiInstrumentChannelMask2[i] & channelMask;
			soundMidiInstrumentMask[i] |= value;

			while (value)
			{
				(*midiInstrumentCount) += (value & 1);
				value >>= 1;
			}
		}
	}

	s32 ImMidiGetTickDelta(PlayerData* playerData, u32 prevTick, u32 tick)
	{
		const u32 ticksPerMeasure = playerData->ticksPerBeat * playerData->beatsPerMeasure;

		// Compute the previous midi tick.
		u32 prevMeasure = prevTick >> 14;
		u32 prevMeasureTick = prevMeasure * ticksPerMeasure;

		u32 prevBeat = (prevTick & 0xf0000) >> 16;
		u32 prevBeatTick = prevBeat * playerData->ticksPerBeat;
		u32 prevMidiTick = prevMeasureTick + prevBeatTick + (prevTick & 0xffff);

		// Compute the current midi tick.
		u32 curMeasure = tick >> 14;
		u32 curMeasureTick = curMeasure * ticksPerMeasure;

		u32 curBeat = (tick & 0xf0000) >> 16;
		u32 curBeatTick = curBeat * playerData->ticksPerBeat;
		u32 curMidiTick = curMeasureTick + curBeatTick + (tick & 0xffff);

		return curMidiTick - prevMidiTick;
	}

	void ImMidiProcessSustain(PlayerData* playerData, u8* sndData, MidiCmdFunc* midiCmdFunc, ImMidiPlayer* player)
	{
		s_midiTrackEnd = 0;

		u8* data = sndData + playerData->chunkOffset + playerData->chunkPtr;
		s_midiTickDelta = ImMidiGetTickDelta(playerData, playerData->prevTick, playerData->tick);

		while (!s_midiTrackEnd)
		{
			u8 value = data[0];
			u8 msgFuncIndex;
			u32 msgSize;
			if (value < 0xf0 && (value & 0x80))
			{
				u8 msgType = (value & 0x70) >> 4;
				u8 channel = (value & 0x0f);
				if (midiCmdFunc[msgType].func2)
				{
					midiCmdFunc[msgType].func2(player, channel, data[1], data[2]);
				}
				msgSize = s_midiMessageSize2[msgType];
			}
			else
			{
				if (value == 0xf0)
				{
					msgFuncIndex = IM_MID_SYS_FUNC;
				}
				else if (value != 0xff)
				{
					TFE_System::logWrite(LOG_ERROR, "iMuse", "su unknown  msg type 0x%x.", value);
					return;
				}
				else
				{
					msgFuncIndex = IM_MID_EVENT;
					data++;
				}
				if (midiCmdFunc[msgFuncIndex].func1)
				{
					midiCmdFunc[msgFuncIndex].func1(playerData, data);
				}
				data++;
				msgSize = midi_getVariableLengthValue(&data);
			}
			data += msgSize;
			// Length of message in "ticks"
			s_midiTickDelta += midi_getVariableLengthValue(&data);
		}
	}

	void ImJumpSustain(ImMidiPlayer* player, u8* sndData, PlayerData* playerData1, PlayerData* playerData2)
	{
		for (s32 i = 0; i < imChannelCount; i++)
		{
			ImMidiOutChannel* channel = &player->channels[i];
			s32 trim = 0;
			if (channel)
			{
				trim = 1 << channel->partTrim;
			}
			s_midiChannelTrim[i] = trim;
		}

		// Remove instruments based on the midi channel 'trim'.
		ImMidiGetInstruments(player, s_curMidiInstrumentMask, &s_curInstrumentCount);
		InstrumentSound* instrInfo = *s_imActiveInstrSounds;
		while (instrInfo && s_curInstrumentCount)
		{
			if (instrInfo->midiPlayer == player)
			{
				s32 mask = s_curMidiInstrumentMask[instrInfo->instrumentId];
				s32 trim = s_midiChannelTrim[instrInfo->channelId];
				if (trim & mask)
				{
					s_curMidiInstrumentMask[instrInfo->instrumentId] &= ~trim;
					s_curInstrumentCount--;
				}
			}
			instrInfo = instrInfo->next;
		}

		if (s_curInstrumentCount)
		{
			ImMidiProcessSustain(playerData1, sndData, s_jumpMidiCmdFunc2, player);

			// Make sure all notes were turned off during the sustain, and if not then clean up or there will be hanging notes.
			if (s_curInstrumentCount)
			{
				TFE_System::logWrite(LOG_ERROR, "iMuse", "su couldn't find all note-offs...");
				for (s32 i = 0; i < MIDI_INSTRUMENT_COUNT; i++)
				{
					if (s_curMidiInstrumentMask[i])
					{
						for (s32 t = 0; t < imChannelCount; t++)
						{
							if (s_curMidiInstrumentMask[i] & s_midiChannelTrim[t])
							{
								TFE_System::logWrite(LOG_ERROR, "iMuse", "missing note %d on chan %d...", i, s_curMidiInstrumentMask[i]);
								ImMidiNoteOff(player, t, i, 0);
							}
						}
					}
				}
			}
		}

		s_trackTicksRemaining = 0;
		instrInfo = *s_imActiveInstrSounds;
		while (instrInfo)
		{
			s32 curTick = instrInfo->curTick;
			if (curTick > s_trackTicksRemaining)
			{
				s_trackTicksRemaining = curTick;
			}
			instrInfo = instrInfo->next;
		}
		ImMidiGetInstruments(player, s_curMidiInstrumentMask, &s_curInstrumentCount);
		ImMidiProcessSustain(playerData2, sndData, s_jumpMidiCmdFunc2, player);
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
		
	void ImHandleChannelDetuneChange(ImMidiPlayer* player, ImMidiOutChannel* channel)
	{
		channel->pan = (player->transpose << 8) + player->detune + channel->pitchBend;
		ImMidiChannel* data = channel->data;
		if (data)
		{
			ImHandleChannelPan(data, channel->pan);
		}
	}

	void ImHandleChannelPitchBend(ImMidiPlayer* player, s32 channelIndex, s32 fractValue, s32 intValue)
	{
		ImMidiOutChannel* channel = &player->channels[channelIndex];
		s32 pitchBend = ((intValue << 7) | fractValue) - (64 << 7);
		if (channel->outChannelCount)
		{
			channel->pan = (channel->outChannelCount * pitchBend) >> 5;	// range -256, 256
			ImHandleChannelDetuneChange(player, channel);
		}
		else
		{
			// No sound channels, this should never be hit.
			assert(0);
		}
	}

	//////////////////////////////////
	// Midi Advance Functions
	//////////////////////////////////
	void ImCheckForTrackEnd(PlayerData* playerData, u8* data)
	{
		if (data[0] == META_END_OF_TRACK)
		{
			s_midiTrackEnd = 1;
		}
	}

	void ImMidiJump2_NoteOn(ImMidiPlayer* player, u8 channelId, u8 arg1, u8 arg2)
	{
		const s32 instrumentId = arg1;
		if (s_midiTickDelta > s_trackTicksRemaining)
		{
			s_midiTrackEnd = 1;
		}
		else if (s_midiChannelTrim[channelId] & s_curMidiInstrumentMask[instrumentId])
		{
			s_curMidiInstrumentMask[instrumentId] &= ~s_midiChannelTrim[channelId];
			InstrumentSound* instrInfo = *s_imActiveInstrSounds;
			while (instrInfo)
			{
				s32 delta = s_midiTickDelta - 10;
				if (instrInfo->midiPlayer == player && instrInfo->instrumentId == instrumentId && instrInfo->channelId == channelId &&
					delta <= instrInfo->curTick)
				{
					instrInfo->curTick = delta;
					break;
				}
				instrInfo = instrInfo->next;
			}
		}
	}

	void ImMidiStopAllNotes(ImMidiPlayer* player)
	{
		for (s32 i = 0; i < imChannelCount; i++)
		{
			ImMidiCommand(player, i, MID_ALL_NOTES_OFF, 0);
		}
	}

	void ImMidiCommand(ImMidiPlayer* player, s32 channelIndex, s32 midiCmd, s32 value)
	{
		ImMidiOutChannel* channel = &player->channels[channelIndex];

		if (midiCmd == MID_MODULATIONWHEEL_MSB)
		{
			channel->modulation = value;
			if (channel->data)
			{
				ImMidiChannelSetModulation(channel->data, value);
			}
		}
		else if (midiCmd == MID_VOLUME_MSB)
		{
			channel->partVolume = value;
			ImHandleChannelVolumeChange(player, channel);
		}
		else if (midiCmd == MID_PAN_MSB)
		{
			channel->partPan = value;
			if (channel->data)
			{
				s32 pan = clamp(value + player->pan - imPanCenter, 0, imPanMax);
				ImMidiChannelSetPan(channel->data, pan);
			}
		}
		else if (midiCmd == MID_GPC1_MSB)
		{
			if (value < 12)
			{
				channel->outChannelCount = value;
				ImMidiPitchBend(player, channelIndex, 0, imPanCenter);
			}
		}
		else if (midiCmd == MID_GPC2_MSB)
		{
			channel->partNoteReq = value;
			if (channel->data)
			{
				ImMidiChannelSetPartNoteReq(channel->data, value);
			}
		}
		else if (midiCmd == MID_GPC3_MSB)
		{
			channel->partPriority = value;
			ImHandleChannelPriorityChange(player, channel);
			ImMidiSetupParts();
		}
		else if (midiCmd == MID_SUSTAIN_SWITCH)
		{
			channel->sustain = value;
			if (channel->data)
			{
				ImSetChannelSustain(channel->data, value);
			}
		}
		else if (midiCmd == MID_ALL_NOTES_OFF)
		{
			channel->sustain = 0;
			if (channel->data)
			{
				ImFreeMidiChannel(channel->data);
			}
		}
	}

	void ImMidiNoteOff(ImMidiPlayer* player, u8 channelId, u8 arg1, u8 arg2)
	{
		s32 instrumentId = arg1;
		ImMidiOutChannel* channel = &player->channels[channelId];
		if (channel->partStatus)
		{
			if (channel->data)
			{
				ImHandleNoteOff(channel->data, instrumentId);
			}
		}
		else
		{
			_ImNoteOff(9, instrumentId);
		}
	}

	void ImMidiNoteOn(ImMidiPlayer* player, u8 channelId, u8 arg1, u8 arg2)
	{
		s32 instrumentId = arg1;
		s32 velocity = arg2;
		ImMidiOutChannel* channel = &player->channels[channelId];
		if (!velocity)
		{
			ImMidiNoteOff(player, channelId, instrumentId, 0);
		}
		else if (channel->partStatus)
		{
			if (channel->data)
			{
				ImHandleNoteOn(channel->data, instrumentId, velocity);
			}
		}
		else
		{
			ImMidi_Channel9_NoteOn(channel->priority, channel->partNoteReq, channel->groupVolume, instrumentId, velocity);
		}
	}

	void ImMidiProgramChange(ImMidiPlayer* player, u8 channelId, u8 arg1, u8 arg2)
	{
		const s32 pgm = arg1;
		ImMidiOutChannel* channel = &player->channels[channelId];
		channel->partPgm = pgm;
		if (!channel->partStatus)
		{
			channel->partStatus = 1;
			ImMidiSetupParts();
		}
		else if (channel->data)
		{
			ImMidiChannelSetPgm(channel->data, pgm);
		}
	}

	void ImMidiPitchBend(ImMidiPlayer* player, u8 channelId, u8 arg1, u8 arg2)
	{
		const s32 pitchBendFract = arg1;
		const s32 pitchBendInt = arg2;

		ImMidiOutChannel* channel = &player->channels[channelId];
		s32 pitchBend = ((pitchBendInt << 7) | pitchBendFract) - (64 << 7);
		s32 channelCount = channel->outChannelCount;

		// There should always be channels.
		assert(channelCount);
		if (channelCount)
		{
			channel->pitchBend = (pitchBend * channelCount) >> 5;
			ImHandleChannelDetuneChange(player, channel);
		}
	}

	void ImMidiEvent(PlayerData* playerData, u8* chunkData)
	{
		u8* data = chunkData;
		MetaType metaType = MetaType(*data);
		// Skip past the metaType and length.
		data += 2;

		if (metaType == META_END_OF_TRACK)
		{
			s_imEndOfTrack = 1;
		}
		else if (metaType == META_TEMPO)
		{
			u32 tempo = (data[0] << 16) | (data[1] << 8) | data[2];
			ImSetTempo(playerData, tempo);
		}
		else if (metaType == META_TIME_SIG)
		{
			ImSetMidiTicksPerBeat(playerData, 960 >> (data[1] - 1), data[0]);
		}
	}

	void ImHandleNoteOn(ImMidiChannel* channel, s32 instrumentId, s32 velocity)
	{
		if (!channel)
		{
			return;
		}

		s32 channelMask = s_channelMask[channel->channelId];
		if (channel->instrumentMask[instrumentId] & channelMask)
		{
			_ImNoteOff(channel->channelId, instrumentId);
		}
		else
		{
			if (channel->instrumentMask2[instrumentId] & channelMask)
			{
				channel->instrumentMask2[instrumentId] = ~channelMask;
				channel->instrumentMask[instrumentId] |= channelMask;
				_ImNoteOff(channel->channelId, instrumentId);
			}
			else
			{
				channel->instrumentMask[instrumentId] |= channelMask;
			}
		}
		_ImNoteOn(channel->channelId, instrumentId, velocity);
	}

	void ImMidi_Channel9_NoteOn(s32 priority, s32 partNoteReq, s32 volume, s32 instrumentId, s32 velocity)
	{
		if (s_ImCh9_priority != priority)
		{
			s_ImCh9_priority = priority;
			ImSendMidiMsg_(9, MID_GPC3_MSB, priority);
		}
		if (s_ImCh9_partNoteReq != partNoteReq)
		{
			s_ImCh9_partNoteReq = partNoteReq;
			ImSendMidiMsg_(9, MID_GPC2_MSB, partNoteReq);
		}
		if (s_ImCh9_volume != volume)
		{
			s_ImCh9_volume = volume;
			ImSendMidiMsg_(9, MID_VOLUME_MSB, volume);
		}
		_ImNoteOn(9, instrumentId, velocity);
	}

	void ImHandleNoteOff(ImMidiChannel* midiChannel, s32 instrumentId)
	{
		if (midiChannel)
		{
			u32 channelMask = s_channelMask[midiChannel->channelId];
			if (midiChannel->instrumentMask[instrumentId] & channelMask)
			{
				midiChannel->instrumentMask[instrumentId] &= ~channelMask;
				if (!midiChannel->sustain)
				{
					_ImNoteOff(midiChannel->channelId, instrumentId);
				}
			}
		}
	}
}
