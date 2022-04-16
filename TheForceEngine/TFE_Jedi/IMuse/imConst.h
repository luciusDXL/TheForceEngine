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

namespace TFE_Jedi
{
	typedef u32 ImSoundId;

	extern const u8 s_midiMsgSize[];
	extern const s32 s_midiMessageSize2[];
	extern const s32 s_channelMask[imChannelCount];
}  // namespace TFE_Jedi