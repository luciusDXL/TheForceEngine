#include "imMidiCmd.h"
#include "imMidiPlayer.h"
#include "imuse.h"
#include "imList.h"
#include "midiData.h"
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Audio/midi.h>
#include <TFE_System/system.h>
#include <assert.h>

namespace TFE_Jedi
{
	#define MIDI_FUNC(f) (MidiEventFunc)f

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
}  // namespace TFE_Jedi
