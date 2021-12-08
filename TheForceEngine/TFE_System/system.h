#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine System Library
// System functionality, such as timers and logging.
//////////////////////////////////////////////////////////////////////

#include "types.h"

#define TFE_MAJOR_VERSION 0
#define TFE_MINOR_VERSION 2
#define TFE_BUILD_VERSION 1

enum LogWriteType
{
	LOG_MSG = 0,
	LOG_WARNING,
	LOG_ERROR,
	LOG_CRITICAL,		//Critical log messages are CRITICAL errors that also act as asserts when attached to a debugger.
	LOG_COUNT
};

namespace TFE_System
{
	void init(f32 refreshRate, bool synced, const char* versionString);
	void shutdown();
	void resetStartTime();
	void setVsync(bool sync);
	bool getVSync();

	void update();
	f64 updateThreadLocal(u64* localTime);

	// Timing
	// --- The current time and delta time are determined once per frame, during the update() function.
	//     In other words an entire frame operates on a single instance of time.
	// Return the delta time.
	f64 getDeltaTime();
	// Get the absolute time since the last start time.
	f64 getTime();

	u64 getCurrentTimeInTicks();
	f64 convertFromTicksToSeconds(u64 ticks);

	// Log
	bool logOpen(const char* filename);
	void logClose();
	void logWrite(LogWriteType type, const char* tag, const char* str, ...);

	// System
	bool osShellExecute(const char* pathToExe, const char* exeDir, const char* param, bool waitForCompletion);
	void sleep(u32 sleepDeltaMS);

	void postQuitMessage();
	bool quitMessagePosted();

	void postSystemUiRequest();
	bool systemUiRequestPosted();

	const char* getVersionString();
}
