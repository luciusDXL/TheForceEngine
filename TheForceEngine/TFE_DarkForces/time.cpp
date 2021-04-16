#include "time.h"

namespace TFE_DarkForces
{
	Tick s_curTick = 0;		// current time in "ticks" - where there are TICKS_PER_SECOND(145) ticks per second.
	fixed16_16 s_deltaTime;	// current delta time in seconds.
}  // TFE_DarkForces