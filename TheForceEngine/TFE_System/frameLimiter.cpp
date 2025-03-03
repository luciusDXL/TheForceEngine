#include <TFE_System/frameLimiter.h>

namespace TFE_System
{
	static const f64 c_expAveF0 = 0.95;
	static const f64 c_expAveF1 = 1.0 - c_expAveF0;
	static const f64 c_epsilon = DBL_EPSILON + 0.001;	// We sleep for ~1ms each iteration, so add 1ms to the epsilon.
	
	static f64 s_limitFPS = 0.0;
	static f64 s_limitDelta = 0.0;
	static f64 s_limitDeltaActual = 0.0;
	static f64 s_accuracy = 0.0;
	static f64 s_accuracyAve = 0.0;
	static u64 s_beginTicks = 0;

	// Set the frame limit in Frames Per Second (FPS).
	// A value of 0 sets no limit.
	void frameLimiter_set(f64 limitFPS/* = 0.0*/)
	{
		if (limitFPS < 30.0)
		{
			s_limitFPS = 0.0;
			s_limitDeltaActual = 0.0;
			s_limitDelta = 0.0;
			if (limitFPS != 0.0)
			{
				TFE_System::logWrite(LOG_ERROR, "Frame Limiter", "The frame limit must be 30 fps or higher, %f is invalid.", limitFPS);
			}
		}
		else
		{
			s_limitFPS = limitFPS;
			s_limitDeltaActual = 1.0 / limitFPS;
			s_limitDelta = s_limitDeltaActual - c_epsilon;
			s_accuracy    = 0.0;
			s_accuracyAve = 0.0;
		}
	}

	f64 frameLimiter_get()
	{
		return s_limitFPS;
	}

	void frameLimiter_begin()
	{
		s_beginTicks = getCurrentTimeInTicks();
	}

	void frameLimiter_end()
	{
		if (s_limitDelta == 0.0) { return; }

		u64 curTick = TFE_System::getCurrentTimeInTicks();
		if (curTick >= s_beginTicks)
		{
			const f64 beginSec = TFE_System::convertFromTicksToSeconds(s_beginTicks);
			f64 curSec = TFE_System::convertFromTicksToSeconds(curTick);
			f64 dt = curSec - beginSec;
			while (dt < s_limitDelta)
			{
				// Give other threads a time slice.
				TFE_System::sleep(1);
				// Update delta time.
				curTick = TFE_System::getCurrentTimeInTicks();
				curSec = TFE_System::convertFromTicksToSeconds(curTick);
				dt = curSec - beginSec;
			}
			// Accuracy - how close is delta time to the desired delta?
			// 1.0 = 100% accurate, 0.0 = fully inaccurate (dt = 0)
			// > 1.0 : frame is too long; < 1.0 : frame is too short.
			s_accuracy = 1.0 - (dt - s_limitDeltaActual) / s_limitDeltaActual;
			s_accuracyAve = (s_accuracyAve == 0.0) ? s_accuracy : s_accuracyAve*c_expAveF0 + s_accuracy*c_expAveF1;
		}
	}

	f64 frameLimiter_getAccuracy()
	{
		return s_accuracyAve;
	}
}