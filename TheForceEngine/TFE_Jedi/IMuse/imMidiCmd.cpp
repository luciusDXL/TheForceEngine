#include "imMidiCmd.h"
#include "imMidiPlayer.h"
#include "imuse.h"
#include "imTrigger.h"
#include "imList.h"
#include "midiData.h"
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Audio/midi.h>
#include <TFE_System/system.h>
#include <assert.h>

namespace TFE_Jedi
{
	extern s32 ImJumpMidiInternal(ImPlayerData* data, s32 chunk, s32 measure, s32 beat, s32 tick, s32 sustain);

	#define MIDI_FUNC(f) (MidiEventFunc)f
	void ImCheckForTrackEnd(ImPlayerData* playerData, u8* data);
	void ImMidiSystemFunc(ImPlayerData* playerData, u8* chunkData);
	void ImMidiPressure(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);

	// Midi functions for each message type - see ImMidiMessageIndex{} above.
	MidiCmdFuncUnion s_jumpMidiCmdFunc[IM_MID_COUNT] =
	{
		MIDI_FUNC(nullptr),        // IM_MID_NOTE_OFF
		MIDI_FUNC(nullptr),        // IM_MID_NOTE_ON
		MIDI_FUNC(nullptr),        // IM_MID_KEY_PRESSURE
		MIDI_FUNC(nullptr),        // IM_MID_CONTROL_CHANGE
		MIDI_FUNC(nullptr),        // IM_MID_PROGRAM_CHANGE
		MIDI_FUNC(nullptr),        // IM_MID_CHANNEL_PRESSURE
		MIDI_FUNC(nullptr),        // IM_MID_PITCH_BEND
		MIDI_FUNC(nullptr),        // IM_MID_SYS_FUNC
		MIDI_FUNC(ImMidiEvent),    // IM_MID_EVENT
	};
	MidiCmdFuncUnion s_jumpMidiSustainCmdFunc[IM_MID_COUNT] =
	{
		MIDI_FUNC(nullptr),                  // IM_MID_NOTE_OFF
		MIDI_FUNC(ImMidiJumpSustain_NoteOn), // IM_MID_NOTE_ON
		MIDI_FUNC(nullptr),                  // IM_MID_KEY_PRESSURE
		MIDI_FUNC(nullptr),                  // IM_MID_CONTROL_CHANGE
		MIDI_FUNC(nullptr),                  // IM_MID_PROGRAM_CHANGE
		MIDI_FUNC(nullptr),                  // IM_MID_CHANNEL_PRESSURE
		MIDI_FUNC(nullptr),                  // IM_MID_PITCH_BEND
		MIDI_FUNC(nullptr),                  // IM_MID_SYS_FUNC
		MIDI_FUNC(ImCheckForTrackEnd)        // IM_MID_EVENT
	};
	MidiCmdFuncUnion s_midiCmdFunc[IM_MID_COUNT] =
	{
		MIDI_FUNC(ImMidiNoteOff),         // IM_MID_NOTE_OFF
		MIDI_FUNC(ImMidiNoteOn),          // IM_MID_NOTE_ON
		MIDI_FUNC(ImMidiPressure),        // IM_MID_KEY_PRESSURE
		MIDI_FUNC(ImMidiCommand),         // IM_MID_CONTROL_CHANGE
		MIDI_FUNC(ImMidiProgramChange),   // IM_MID_PROGRAM_CHANGE
		MIDI_FUNC(ImMidiPressure),        // IM_MID_CHANNEL_PRESSURE
		MIDI_FUNC(ImMidiPitchBend),       // IM_MID_PITCH_BEND
		MIDI_FUNC(ImMidiSystemFunc),      // IM_MID_SYS_FUNC
		MIDI_FUNC(ImMidiEvent),           // IM_MID_EVENT
	};

	void ImCheckForTrackEnd(ImPlayerData* playerData, u8* data)
	{
		if (data[0] == META_END_OF_TRACK)
		{
			s_midiTrackEnd = 1;
		}
	}

	s32 ImUpdateHook(s32* hook, u8 newValue)
	{
		if (newValue)
		{
			if ((s32)newValue == *hook)
			{
				return imFail;
			}
			*hook = 0;
			return imSuccess;
		}

		if (*hook == 0x80)
		{
			*hook = (s32)newValue;
			return imFail;
		}

		return imSuccess;
	}

	void ImMidiSystemFunc(ImPlayerData* playerData, u8* chunkData)
	{
		if (*chunkData != 0xf0)
		{
			return;
		}

		u8* data = chunkData + 1;
		if (data[0] > 0x7f || data[1] != 0x7d)
		{
			return;
		}
		data += 2;

		u8 value = *data;
		data++;
		if (value == 0)
		{
			// Set Marker
			ImMidiPlayer* player = playerData->player;
			u8 marker = *data;
			player->marker = marker;
			ImSetSoundTrigger(player->soundId, marker);
		}
		else if (value == 1)
		{
			value = *data;
			data++;

			ImMidiPlayer* player = playerData->player;
			s32* hook = &player->hook;
			if (ImUpdateHook(hook, value) == imSuccess)
			{
				ImJumpMidiInternal(playerData, data[0], (data[1] << 7) | data[2], data[3], (data[4] << 7) | data[5], data[6]);
			}
		}
		else if (value == 3)
		{
			ImMidiPlayer* player = playerData->player;
			player->marker = 0x80;
			ImSetSoundTrigger(player->soundId, *data);
		}
		else
		{
			TFE_System::logWrite(LOG_ERROR, "IMuse", "ERROR:mp got bad sysex msg...");
		}
	}

	void ImMidiPressure(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2)
	{
		// Not used by Dark Forces.
		TFE_System::logWrite(LOG_ERROR, "IMuse", "ImMidiPressure() unimplemented.");
		assert(0);
	}
}  // namespace TFE_Jedi
