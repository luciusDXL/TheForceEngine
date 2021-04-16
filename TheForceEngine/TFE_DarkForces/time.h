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
	// Integer number of ticks per second.
	#define TICKS_PER_SECOND 145

	// Exact value from converting from seconds to ticks.
	#define SECONDS_TO_TICKS_EXACT f32(TICKS_PER_SECOND)
	// Note the extra 0.5f, this seems to be a way of 'rounding' the results.
	// This seems to have been done to make the timing slightly more "generous" in some cases (such as elevator delays).
	#define SECONDS_TO_TICKS_ROUNDED (SECONDS_TO_TICKS_EXACT + 0.5f)

	// Simple way of setting readable tick constants.
	// This is the same as x * 145.5f
	#define TICKS(x) ((x)*TICKS_PER_SECOND + ((x)>>1))

	extern Tick s_curTick;
	extern fixed16_16 s_deltaTime;

	// Convert from frames per second (fps) to Ticks.
	Tick time_frameRateToDelay(u32 frameRate);
}  // namespace TFE_DarkForces