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

// Debug/Log Message Verbosity Settings.
#define IM_SHOW_DEBUG_MSG   0  // High frequency debug messages, these are only written out to the console or debug output.
#define IM_SHOW_LOG_MSG     0  // IMuse messages, usually the original DOS messages. A few new messages were added for TFE.
#define IM_SHOW_LOG_WARNING 1  // IMuse warnings, usually the original DOS messages. A few new warnings were added for TFE.
// IMuse errors are always enabled. They are usually the original DOS messages. A few new errors were added for TFE.

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
#define ImTime_getBeatFixed(t) ((t) & 0xf0000)
#define ImTime_getMeasure(t) ((t) >> 20)

// Set Time
#define ImTime_setTick(t)    (t)
#define ImTime_setBeat(b)    ((b)<<16)
#define ImTime_setMeasure(m) ((m)<<20)

// Null sound define
#define IM_NULL_SOUNDID 0

// DEBUGGING & LOGGING
// These debug messages are very verbose so should be disabled in release builds.
// Lower frequency messages that are kept in release use logWrite() directly.
#if IM_SHOW_DEBUG_MSG
	#define IM_DBG_MSG(str, ...) TFE_System::debugWrite("IMuse", str, __VA_ARGS__)
#else
	#define IM_DBG_MSG(str, ...)
#endif

// Original messages from DOS and a few new errors added for TFE.
// Note, when enabled, these write to the console and log file. Dark Forces disables
// output from these messages but I have preserved them in TFE for debugging.
// Also note for TFE - errors are always written, less important or expected errors are relegated to warnings.
#define IM_LOG_ERR(str, ...) TFE_System::logWrite(LOG_ERROR, "iMuse", str, __VA_ARGS__)
#if IM_SHOW_LOG_MSG
	#define IM_LOG_MSG(str, ...) TFE_System::logWrite(LOG_MSG, "iMuse", str, __VA_ARGS__)
#else
	#define IM_LOG_MSG(str, ...)
#endif

#if IM_SHOW_LOG_WARNING
#define IM_LOG_WRN(str, ...) TFE_System::logWrite(LOG_WARNING, "iMuse", str, __VA_ARGS__)
#else
#define IM_LOG_WRN(str, ...)
#endif

namespace TFE_Jedi
{
	typedef u32 ImSoundId;

	extern const u8  c_midiMsgSize[];
	extern const u32 c_channelMask[imChannelCount];
}  // namespace TFE_Jedi