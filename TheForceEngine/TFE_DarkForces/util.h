#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Game Message
// This handles HUD messages, local messages, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_Input/inputEnum.h>
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>

namespace TFE_DarkForces
{
	// language-specific hotkeys
	struct LangHotkeys {
		KeyboardCode k_yes;
		KeyboardCode k_quit;
		KeyboardCode k_cont;	// resume mission
		KeyboardCode k_conf;	// configure
		KeyboardCode k_agdel;	// delete agent
		KeyboardCode k_begin;	// begin mission
		KeyboardCode k_easy;
		KeyboardCode k_med;
		KeyboardCode k_hard;
		KeyboardCode k_canc;
	};

	char* copyAndAllocateString(const char* start, const char* end);
	char* copyAndAllocateString(const char* str);

	char* strCopyAndZero(char* dst, const char* src, s32 bufferSize);

	s32 strToInt(const char* param);
	u32 strToUInt(const char* param);
}  // TFE_DarkForces