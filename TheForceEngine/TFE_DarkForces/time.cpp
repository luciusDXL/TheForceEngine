#include "time.h"

namespace TFE_DarkForces
{
	Tick s_curTick = 0;		// current time in "ticks" - where there are TICKS_PER_SECOND(145) ticks per second.
	fixed16_16 s_deltaTime;	// current delta time in seconds.

	Tick time_frameRateToDelay(u32 frameRate)
	{
		return Tick(SECONDS_TO_TICKS_ROUNDED / f32(frameRate));
	}
}  // TFE_DarkForces