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

enum iMuseConst
{
	imMaxVolume    = 127,  // Maximum allowed volume, correlated with imVolumeShift.
	imMaxPriority  = 127,
	imPanCenter    = 64,
	imPanMax       = 127,
	imVolumeShift  = 7,    // Amount to shift when multiplying volumes.
	imChannelCount = 16,   // Number of midi channels.
	imGetValue     = -1,   // Magic number to get values instead of set them.
	imSoundWildcard = 0xffffffff,
};

// Time in iMuse is stored like the following:
// Where, generally, there are 480 ticks/beat and 4 beats/measure.
// ---------------------------------------------------------
// | 12-bits            | 4-bits         | 16-bits         |
// ---------------------------------------------------------
// | offset=20-bits     | offset=16-bits | offset=0 bits   |
// ---------------------------------------------------------
// | Measure (0 - 999)  | Beat (0 - 11)  | Tick (0 - 479)  |
// ---------------------------------------------------------

// Get Time
#define ImTime_getTicks(t)   ((t) & 0xffff)
#define ImTime_getBeat(t)    (((t) >> 16) & 0xf)
#define ImTime_getMeasure(t) ((t) >> 20)

// Set Time
#define ImTime_setTick(t)    (t)
#define ImTime_setBeat(b)    ((b)<<16)
#define ImTime_setMeasure(m) ((m)<<20)

// These debug messages are very verbose so should be disabled in release builds.
// Lower frequency messages that are kept in release use logWrite() directly.
#define IM_SHOW_DEBUG_MSG 0
#if IM_SHOW_DEBUG_MSG
	#define IM_DBG_MSG(str, ...) TFE_System::debugWrite("IMuse", str, __VA_ARGS__)
#else
	#define IM_DBG_MSG(str, ...)
#endif

#define IM_NULL_SOUNDID 0

namespace TFE_Jedi
{
	typedef u32 ImSoundId;

	extern const u8  c_midiMsgSize[];
	extern const u32 c_channelMask[imChannelCount];
}  // namespace TFE_Jedi