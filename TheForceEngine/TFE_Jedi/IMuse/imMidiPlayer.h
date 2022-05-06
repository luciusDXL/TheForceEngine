#pragma once
//////////////////////////////////////////////////////////////////////
// iMuse System
// The IMuse system used by Dark Forces.
//
// Copyright note:
// While the project as a whole is licensed under GPL-2.0, some of the
// code under TFE_Jedi/ was derived from reverse-engineered
// code from "Dark Forces" (DOS) which is copyrighted by LucasArts.
// The original renderer is contained in RClassic_Fixed/ and the
// RClassic_Float/ and RClassic_GPU/ sub-renderers are derived from
// that code.
//
// I consider the reverse-engineering to be "Fair Use" - a means of 
// supporting the games on other platforms and to improve support on
// existing platforms without claiming ownership of the games
// themselves or their IPs.
//
// That said using this code in a commercial project is risky without
// permission of the original copyright holders (LucasArts).
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "imConst.h"
#include "imMidiCmd.h"

namespace TFE_Jedi
{
	struct ImMidiPlayer;
	struct ImMidiOutChannel;
	struct ImMidiChannel;

	struct InstrumentSound
	{
		InstrumentSound* prev;
		InstrumentSound* next;
		ImMidiPlayer* midiPlayer;
		s32 instrumentId;
		s32 channelId;
		s32 curTick;
		s32 curTickFixed;
	};

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

		u32* instrumentMask;
		u32* instrumentMask2;
		ImMidiPlayer* sharedPart;
		ImMidiOutChannel* sharedChannel;
		ImMidiChannel* prevChannel;
		s32 sharedId;

		s32 sharedPgm;
		s32 sharedPriority;
		s32 sharedNoteReq;
		s32 sharedVolume;

		s32 sharedPan;
		s32 sharedModulation;
		s32 sharedFinalPan;
		s32 sharedSustain;

		u32* sharedInstrumentMask;
		u32* sharedInstrumentMask2;
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

	struct ImPlayerData
	{
		ImMidiPlayer* player;
		ImSoundId soundId;
		s32 seqIndex;
		s32 chunkOffset;
		s32 nextTick;			// Next tick
		s32 curTick;			// Current tick
		s32 chunkPtr;
		s32 ticksPerBeat;
		s32 beatsPerMeasure;
		s32 tickAccum;			// Fixed-point tick accumulator, preserves the fractional part of the tick to avoid time being lost.
		s32 tempo;
		s32 step;
		s32 speed;
		s32 stepFixed;
	};

	struct ImMidiPlayer
	{
		ImMidiPlayer* prev;
		ImMidiPlayer* next;
		ImPlayerData* data;
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

		ImMidiOutChannel channels[MIDI_CHANNEL_COUNT];
	};
	
	extern s32 s_imEndOfTrack;
	extern s32 s_midiTrackEnd;
	extern InstrumentSound* s_imActiveSustainedSounds;
	extern InstrumentSound* s_imFreeSustainedSounds;
	extern InstrumentSound s_instrumentSounds[24];
	extern ImMidiPlayer* s_midiPlayerList;
	
	extern s32 ImFixupSoundTick(ImPlayerData* data, s32 value);
	extern void ImAdvanceMidi(ImPlayerData* playerData, u8* sndData, MidiCmdFuncUnion* midiCmdFunc);
	extern void ImReleaseMidiPlayer(ImMidiPlayer* player);
	extern void ImResetMidiOutChannel(ImMidiOutChannel* channel);
	extern void ImFreeMidiChannel(ImMidiChannel* channelData);
	extern void ImRemoveInstrumentSound(ImMidiPlayer* player);
}  // namespace TFE_Jedi