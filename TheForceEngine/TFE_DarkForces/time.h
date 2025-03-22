#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Player -
// Player information and object data.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>

typedef u32 Tick;
typedef s32 TickSigned;

namespace TFE_DarkForces
{
	// The actual timer frequency used by the DOS code.
	#define TIMER_FREQ 145.6521

	// Integer number of ticks per second.
	#define TICKS_PER_SECOND 145

	// Exact value from converting from seconds to ticks.
	#define SECONDS_TO_TICKS_EXACT f32(TICKS_PER_SECOND)
	// Note the extra 0.5f, this seems to be a way of 'rounding' the results.
	// This seems to have been done to make the timing slightly more "generous" in some cases (such as elevator delays).
	#define SECONDS_TO_TICKS_ROUNDED (SECONDS_TO_TICKS_EXACT + 0.5f)

	// Simple way of setting readable tick constants.
	// This is the integer equivalent to x * SECONDS_TO_TICKS_ROUNDED
	#define TICKS(x) ((x)*TICKS_PER_SECOND + ((x)>>1))

	// The maximum delta time = 64 seconds.
	// I'm not sure why it is so high, but the original value in the DF code is 0x400000 = 64 * ONE_16
	#define MAX_DELTA_TIME FIXED(64)

	// Current game-time in "ticks"
	extern Tick s_curTick;
	// Previous frame time in "ticks" - used when calculating s_deltaTime.
	extern Tick s_prevTick;
	// Delta-time since the last frame in "seconds" - though it is derived from the delta in ticks, so the smallest non-zero
	// delta is 1.0 / 145.0, i.e. s_deltaTime = (s_curTick - s_prevTick) / 145.0
	// This is used for movement, physics, elevator value changes based on speed, and similar situations.
	extern fixed16_16 s_deltaTime;
	// This array ticks at the fps defined by its index.
	// Each computes dt*frameRate and the indexed framerate (i.e. s_frameTicks[12] = 12 fps).
	extern fixed16_16 s_frameTicks[13];

	extern f64 s_timeAccum;

	// Convert from frames per second (fps) to Ticks.
	Tick time_frameRateToDelay(u32 frameRate);
	Tick time_frameRateToDelay(s32 frameRate);
	Tick time_frameRateToDelay(f32 frameRate);
	void updateTime();
	void time_pause(JBool pause);

	void time_serialize(Stream* stream);
}  // namespace TFE_DarkForces