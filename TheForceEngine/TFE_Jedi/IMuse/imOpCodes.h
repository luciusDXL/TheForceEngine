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

enum ImOpCode
{
	imInitialize    = 0x0000,
	imTerminate     = 0x0001,
	imPrintf        = 0x0002,
	imPause         = 0x0003,
	imResume        = 0x0004,
	imSave          = 0x0005,
	imRestore       = 0x0006,
	imSetGroupVol   = 0x0007,
	imStartSound    = 0x0008,
	imStopSound     = 0x0009,
	imStopAllSounds = 0x000A,
	imGetNextSound  = 0x000B,
	imSetParam      = 0x000C,
	imGetParam      = 0x000D,
	imFadeParam     = 0x000E,
	imSetHook       = 0x000F,
	imGetHook       = 0x0010,
	imSetTrigger    = 0x0011,
	imCheckTrigger  = 0x0012,
	imClearTrigger  = 0x0013,
	imDeferCommand  = 0x0014,
		
	imJumpMidi      = 0x0015,
	imScanMidi      = 0x0016,
	imSendMidiMsg   = 0x0017,
	imShareParts    = 0x0018,

	imStartStream     = 0x0019,
	imSwitchStream    = 0x001A,
	imProcessStreams  = 0x001B,
	imQueryNextStream = 0x001C,

	imUndefined = 0x001D
};