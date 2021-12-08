#include "ltimer.h"
#include <TFE_System/system.h>
#include <TFE_Jedi/Math/core_math.h>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	static f64 s_timeInSec = 0.0;
	static f64 s_accum = 0.0;
	static LTick s_curTick = 0;
	static LTick s_lastTick = 0;
	static s16 s_frameRate = 20;	// 12 frames per second

	static LTick s_prevRate = 0;
	static LTick s_lastRate = 0;
	static LTick s_time = 0;
		
	// This takes the place of the timer interrupt and should be called every frame.
	void tfe_updateLTime()
	{
		s_timeInSec += TFE_System::getDeltaTime();
		// Convert from seconds to ticks.
		f64 curTick = f64(LTICKS_PER_SECOND) * s_timeInSec;
		s_curTick = LTick(curTick);
	}

	void ltime_init()
	{
		s_timeInSec = 0.0;
		s_accum = 0.0;
		s_curTick = 0;
		s_lastTick = 0;
		s_frameRate = 20;

		s_prevRate = 0;
		s_lastRate = 0;
		s_time = 0;
	}

	void ltime_setFrameRate(u16 delay)
	{
		s_frameRate = delay;
	}

	u16 ltime_getDelay()
	{
		return s_frameRate;
	}

	LTick ltime_curTick()
	{
		return s_curTick;
	}

	LTick ltime_elapsed()
	{
		if (s_lastTick == 0) { s_lastTick = s_curTick; }
		LTick dt = s_curTick - s_lastTick;
		s_lastTick = s_curTick;

		return dt;
	}
		
	JBool ltime_checkTimeElapsed()
	{
		LTick curRate = ltime_elapsed();
		s_lastRate += curRate;
		s_time += curRate;

		if (s_lastRate >= (LTick)s_frameRate)
		{
			return JFALSE;
		}
		return JTRUE;
	}

	JBool ltime_often()
	{
		JBool nextFrame = ltime_checkTimeElapsed();
		// fast poll input...
		return nextFrame;
	}

	// returns JTRUE if the frame is ready.
	// This is implemented instead of the DOS while loop.
	JBool ltime_isFrameReady()
	{
		if (!ltime_often())
		{
			s_prevRate = s_lastRate;
			s_lastRate = 0;
			return JTRUE;
		}
		return JFALSE;
	}
}  // namespace TFE_DarkForces