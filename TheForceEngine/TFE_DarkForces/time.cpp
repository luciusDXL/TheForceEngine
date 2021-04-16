#include "time.h"

namespace TFE_DarkForces
{
	Tick s_curTick = 0;
	Tick s_prevTick = 0;
	fixed16_16 s_deltaTime;

	Tick time_frameRateToDelay(u32 frameRate)
	{
		return Tick(SECONDS_TO_TICKS_ROUNDED / f32(frameRate));
	}
}  // TFE_DarkForces