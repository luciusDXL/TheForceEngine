#include "time.h"
#include <TFE_System/system.h>

namespace TFE_DarkForces
{
	Tick s_curTick = 0;
	Tick s_prevTick = 0;
	static f64 s_timeAccum = 0.0;
	fixed16_16 s_deltaTime;

	Tick time_frameRateToDelay(u32 frameRate)
	{
		return Tick(SECONDS_TO_TICKS_ROUNDED / f32(frameRate));
	}

	void updateTime()
	{
		s_timeAccum += TFE_System::getDeltaTime() * 145.6521;
		s_curTick = Tick(s_timeAccum);
	}
}  // TFE_DarkForces