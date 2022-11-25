#include "ltimer.h"
#include <TFE_System/system.h>
#include <TFE_Jedi/Math/core_math.h>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	static f64 s_timeInSec = 0.0;
	static LTick s_curTick = 0;
	static LTick s_lastTick = 0;
	static s32 s_frameDelay = 20;	// 12 frames per second

	static LTick s_prevAccum = 0;
	static LTick s_lastAccum = 0;

	static f64 s_prevTime = 0.0f;

	// This takes the place of the timer interrupt and should be called every frame.
	void tfe_updateLTime()
	{
		f64 curTime = TFE_System::getTime();
		s_timeInSec += s_prevTime ? (curTime - s_prevTime) : 0.0;
		s_prevTime = curTime;

		// Convert from seconds to ticks.
		f64 curTick = f64(LTICKS_PER_SECOND) * s_timeInSec * TFE_System::c_gameTimeScale;
		s_curTick = LTick(curTick);
	}

	void ltime_init()
	{
		s_prevTime = 0.0;
		s_timeInSec = 0.0;
		s_curTick = 0;
		s_lastTick = 0;
		s_frameDelay = 20;

		s_prevAccum = 0;
		s_lastAccum = 0;
	}

	void ltime_setFrameDelay(u16 delay)
	{
		s_frameDelay = delay;
	}

	u16 ltime_getDelay()
	{
		return s_frameDelay;
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
		s_lastAccum += ltime_elapsed();
		if (s_lastAccum >= (LTick)s_frameDelay)
		{
			return JFALSE;
		}
		return JTRUE;
	}

	// returns JTRUE if the frame is ready.
	// This is implemented instead of the DOS while loop.
	JBool ltime_isFrameReady()
	{
		if (!ltime_checkTimeElapsed())
		{
			s_prevAccum = s_lastAccum;
			s_lastAccum = 0;
			return JTRUE;
		}
		return JFALSE;
	}
}  // namespace TFE_DarkForces