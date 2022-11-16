#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Profiler
// Simple "zone" based profiler.
// Add TFE_PROFILE_ENABLED to preprocessor defines in the build to enable.
// Currently does not respect the call path, that is TODO.
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include "system.h"

enum TFE_Message
{
	TFE_MSG_SAVE = 0,
	TFE_MSG_SECRET,
	TFE_MSG_COUNT
};

namespace TFE_System
{
	const char* getMessage(TFE_Message msg);
	bool loadMessages(const char* path);
	void freeMessages();
}