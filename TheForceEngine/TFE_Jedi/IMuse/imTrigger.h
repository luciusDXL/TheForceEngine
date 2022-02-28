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

namespace TFE_Jedi
{
	s32 ImSetTrigger(s32 soundId, s32 marker, s32 opcode);
	s32 ImCheckTrigger(s32 soundId, s32 marker, s32 opcode);
	s32 ImClearTrigger(s32 soundId, s32 marker, s32 opcode);
	s32 ImClearTriggersAndCmds();

	s32  ImDeferCommand(s32 time, s32 opcode, s32 arg1);
	void ImHandleDeferredCommands();
}  // namespace TFE_Jedi