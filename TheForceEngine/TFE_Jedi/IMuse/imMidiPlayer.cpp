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
	extern void ImNoteOff(s32 channelId, s32 instrId);
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
		IM_DBG_MSG("t: %03d.%02d.%03d -> %03d.%02d.%03d", ImTime_getMeasure(tick), ImTime_getBeat(tick), ImTime_getTicks(tick),
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
					chunkData += c_midiMsgSize[msgType];
				}
				else
				{
					IM_LOG_ERR("Invalid midi message: %u", data);
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
						IM_LOG_ERR("sq unknown msg type 0x%x...", data);
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
		while (ImTime_getTicks(value) >= data->ticksPerBeat)
		{
			value = value - data->ticksPerBeat + ImTime_setBeat(1);
			// This needs to be here so that we don't overflow the number of beats (15).
			while (ImTime_getBeatFixed(value) >= data->beatsPerMeasure)
			{
				value = value - data->beatsPerMeasure + ImTime_setMeasure(1);
			}
		}
		// Just in case.
		while (ImTime_getBeatFixed(value) >= data->beatsPerMeasure)
		{
			value = value - data->beatsPerMeasure + ImTime_setMeasure(1);
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
				ImMidiNoteOff(instrInfo->midiPlayer, instrInfo->channelId, instrInfo->instrumentId, 0);
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
			const u32 channelMask = c_channelMask[channelData->channelId];
			channelData->sustain = 0;
			for (s32 i = 0; i < MIDI_INSTRUMENT_COUNT; i++)
			{
				s32 instrMask = channelData->instrumentMask[i];
				if (channelData->instrumentMask[i] & channelMask)
				{
					ImNoteOff(channelData->channelId, i);
					channelData->instrumentMask[i] &= ~channelMask;
				}
				else if (channelData->instrumentMask2[i] & channelMask)
				{
					ImNoteOff(channelData->channelId, i);
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
