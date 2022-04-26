#include "imMidiPlayer.h"
#include "imuse.h"
#include "imList.h"
#include "imSoundFader.h"
#include "imTrigger.h"
#include "midiData.h"
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Audio/midi.h>
#include <TFE_System/system.h>
#include <assert.h>

namespace TFE_Jedi
{
	// TODO: Fix these externs.
	extern void _ImNoteOff(s32 channelId, s32 instrId);
	extern void ImMidiSetupParts();
	
	InstrumentSound* s_imActiveInstrSounds = nullptr;
	InstrumentSound* s_imInactiveInstrSounds = nullptr;
	InstrumentSound s_instrumentSounds[24];
	ImMidiPlayer* s_midiPlayerList = nullptr;
	s32 s_midiTrackEnd = 0;
	s32 s_imEndOfTrack = 0;

	void ImSetEndOfTrack();

	// Advance the current sound to the next tick.
	void ImAdvanceMidi(ImPlayerData* playerData, u8* sndData, MidiCmdFuncUnion* midiCmdFunc)
	{
		s_imEndOfTrack = 0;
		s32 tick = playerData->curTick;
		s32 nextTick = playerData->nextTick;
		u8* chunkData = sndData + playerData->chunkOffset + playerData->chunkPtr;
		IM_DBG_MSG("IMuse", "cur: %03d.%02d.%04d, next: %03d.%02d.%04d", ImTime_getMeasure(tick), ImTime_getBeat(tick), ImTime_getTicks(tick),
		           ImTime_getMeasure(nextTick), ImTime_getBeat(nextTick), ImTime_getTicks(nextTick));

		while (tick < nextTick)
		{
			u8 data = chunkData[0];
			if (data < MID_EXCLUSIVE_START)
			{
				if (data & 0x80)
				{
					u8 msgType = (data & 0x70) >> 4;
					u8 channel = data & 0x0f;
					if (midiCmdFunc[msgType].cmdFunc)
					{
						midiCmdFunc[msgType].cmdFunc(playerData->player, channel, chunkData[1], chunkData[2]);
					}
					chunkData += s_midiMsgSize[msgType];
				}
				else
				{
					TFE_System::logWrite(LOG_ERROR, "iMuse", "Invalid midi message: %u", data);
					assert(0);
				}
			}
			else
			{
				u8 msgType = 0;
				if (data == MID_EXCLUSIVE_START)
				{
					msgType = IM_MID_SYS_FUNC;
				}
				else
				{
					if (data != 0xff)
					{
						TFE_System::logWrite(LOG_ERROR, "iMuse", "sq unknown msg type 0x%x...", data);
						ImReleaseMidiPlayer(playerData->player);
						return;
					}
					msgType = IM_MID_EVENT;
					chunkData++;
				}
				if (midiCmdFunc[msgType].evtFunc)
				{
					midiCmdFunc[msgType].evtFunc(playerData, chunkData);
				}

				// This steps past the current chunk, if we don't know what a chunk is, it should be skipped.
				chunkData++;
				chunkData += midi_getVariableLengthValue(&chunkData);
			}

			// If we have reached the end, then we are done for now.
			if (s_imEndOfTrack)
			{
				return;
			}

			u32 dt = midi_getVariableLengthValue(&chunkData);
			tick += dt;
			if ((tick & 0xffff) >= playerData->ticksPerBeat)
			{
				tick = ImFixupSoundTick(playerData, tick);
			}
		}
		playerData->curTick = tick;
		u8* chunkBase = sndData + playerData->chunkOffset;
		playerData->chunkPtr = s32(chunkData - chunkBase);
	}

	s32 ImFixupSoundTick(ImPlayerData* data, s32 value)
	{
		while ((value & 0xffff) >= data->ticksPerBeat)
		{
			value = value - data->ticksPerBeat + ONE_16;
			// This needs to be here so that we don't overflow the number of beats (15).
			while ((value & FIXED(15)) >= data->beatsPerMeasure)
			{
				value = value - data->beatsPerMeasure + FIXED(16);
			}
		}
		// Just in case.
		while ((value & FIXED(15)) >= data->beatsPerMeasure)
		{
			value = value - data->beatsPerMeasure + FIXED(16);
		}
		return value;
	}

	void ImRemoveInstrumentSound(ImMidiPlayer* player)
	{
		InstrumentSound* instrInfo = s_imActiveInstrSounds;
		while (instrInfo)
		{
			InstrumentSound* next = instrInfo->next;
			if (player == instrInfo->midiPlayer)
			{
				IM_LIST_REM(s_imActiveInstrSounds, instrInfo);
				IM_LIST_ADD(s_imInactiveInstrSounds, instrInfo);
			}
			instrInfo = next;
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

	void ImReleaseMidiPlayer(ImMidiPlayer* player)
	{
		s32 res = IM_LIST_REM(s_midiPlayerList, player);
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

	void ImSetEndOfTrack()
	{
		s_imEndOfTrack = 1;
	}
}  // namespace TFE_Jedi
