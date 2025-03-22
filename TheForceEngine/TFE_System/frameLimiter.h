#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine System Library
// System functionality, such as timers and logging.
//////////////////////////////////////////////////////////////////////

#include "system.h"

namespace TFE_System
{
	// Set the frame limit in Frames Per Second (FPS).
	// A value of 0 sets no limit.
	void frameLimiter_set(f64 limitFPS = 0.0);
	f64 frameLimiter_get();
	f64 frameLimiter_getAccuracy();

	void frameLimiter_begin();
	void frameLimiter_end();
}
