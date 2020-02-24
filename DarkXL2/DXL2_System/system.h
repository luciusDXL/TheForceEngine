#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 System Library
// System functionality, such as timers and logging.
//////////////////////////////////////////////////////////////////////

#include "types.h"

enum LogWriteType
{
	LOG_MSG = 0,
	LOG_WARNING,
	LOG_ERROR,
	LOG_CRITICAL,		//Critical log messages are CRITICAL errors that also act as asserts when attached to a debugger.
	LOG_COUNT
};

namespace DXL2_System
{
	void init(f32 refreshRate, bool synced);
	void shutdown();

	void update();

	// Timing
	// Return the delta time.
	f64 getDeltaTime();

	// Log
	bool logOpen(const char* filename);
	void logClose();
	void logWrite(LogWriteType type, const char* tag, const char* str, ...);
}
