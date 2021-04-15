#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Player -
// Player information and object data.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Level/rsector.h>
#include <TFE_Level/robject.h>

typedef u32 Tick;

namespace TFE_DarkForces
{
	#define SECONDS_TO_TICKS 145.5f

	// Simple way of setting readable tick constants.
	// This is the same as x * 145.5f
	#define TICKS(x) ((x)*145 + ((x)>>1))

	extern Tick s_curTick;
	extern fixed16_16 s_deltaTime;
}  // namespace TFE_DarkForces