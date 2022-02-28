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
#include "imOpCodes.h"

// Used by ImSetParam() and ImGetParam().
enum iMuseParameter
{
	// empty low byte allows bit field for MIDI channel
	soundType        = 0x0000,	// see next enum
	soundPlayCount   = 0x0100,	// iterations playing
	soundPendCount   = 0x0200,	// iterations pending
	soundMarker      = 0x0300,	// last marker seen
	soundGroup       = 0x0400,	// group volume select
	soundPriority    = 0x0500,	// priority
	soundVol         = 0x0600,	// volume
	soundPan         = 0x0700,	// pan
	soundDetune      = 0x0800,	// absolute detune
	soundTranspose   = 0x0900,	// relative transpose
	soundMailbox     = 0x0A00,	// general purpose
	midiChunk        = 0x0B00,	// midifile chunk
	midiMeasure      = 0x0C00,	// measure
	midiBeat         = 0x0D00,	// beat
	midiTick         = 0x0E00,	// tick 0-479
	midiSpeed        = 0x0F00,	// speed
	midiPartTrim     = 0x1100,	// part trim
	midiPartStatus   = 0x1200,	// see bit fields below
	midiPartPriority = 0x1300,	// set by MIDI ch msg
	midiPartNoteReq  = 0x1400,
	midiPartVol      = 0x1500,
	midiPartPan      = 0x1600,
	midiPartPgm      = 0x1700,
	waveStreamFlag   = 0x1800	// set if wave streamed
};

// ImGetParam() soundType.
enum iMuseSoundType  
{
	typeMidi = 1,
	typeWave = 2
};

enum iMuseErrorCode
{
	imSuccess        =  0,
	imFail           = -1,
	imNotImplemented = -2,
	imNotFound       = -3,
	imInvalidSound   = -4,
	imArgErr         = -5,
	imAllocErr       = -6,
	imIllegalErr     = -7
};

enum MidiInstruments
{
	// General midi supports instruments 0 - 127
	MIDI_INSTRUMENT_COUNT = 128,
};

namespace TFE_Jedi
{
	////////////////////////////////////////////////////
	// High level functions
	////////////////////////////////////////////////////
	extern s32 ImSetMasterVol(s32 vol);
	extern s32 ImGetMasterVol(void);

	extern s32 ImSetMusicVol(s32 vol);
	extern s32 ImGetMusicVol(void);

	extern s32 ImSetSfxVol(s32 vol);
	extern s32 ImGetSfxVol(void);

	extern s32 ImSetVoiceVol(s32 vol);
	extern s32 ImGetVoiceVol(void);

	extern s32 ImStartSfx(s32 sound, s32 priority);
	extern s32 ImStartVoice(s32 sound, s32 priority);
	extern s32 ImStartMusic(s32 sound, s32 priority);

	////////////////////////////////////////////////////
	// Low level functions
	////////////////////////////////////////////////////
	extern s32 ImInitialize();
	extern s32 ImTerminate(void);
	extern s32 ImPause(void);
	extern s32 ImResume(void);
	extern s32 ImSetGroupVol(s32 group, s32 vol);
	extern s32 ImStartSound(s32 sound, s32 priority);
	extern s32 ImStopSound(s32 sound);
	extern s32 ImStopAllSounds(void);
	extern s32 ImGetNextSound(s32 sound);
	extern s32 ImSetParam(s32 sound, s32 param, s32 val);
	extern s32 ImGetParam(s32 sound, s32 param);
	extern s32 ImFadeParam(s32 sound, s32 param, s32 val, s32 time);
	extern s32 ImSetHook(s32 sound, s32 val);
	extern s32 ImGetHook(s32 sound);
	extern s32 ImSetTrigger(s32 sound, s32 marker, s32 opcode);  // Modified based on actual usage to simplify
	extern s32 ImCheckTrigger(s32 sound, s32 marker, s32 opcode);
	extern s32 ImClearTrigger(s32 sound, s32 marker, s32 opcode);
	extern s32 ImDeferCommand(s32 time, s32 opcode, s32 sound);  // Modified based on actual usage to simplify

	extern s32 ImJumpMidi(s32 sound, s32 chunk, s32 measure, s32 beat, s32 tick, s32 sustain);
	extern s32 ImSendMidiMsg(s32 sound, s32 arg1, s32 arg2, s32 arg3);
	extern s32 ImShareParts(s32 sound1, s32 sound2);
}  // namespace TFE_Jedi