#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Game Message
// This handles HUD messages, local messages, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>

namespace TFE_DarkForces
{
	char* copyAndAllocateString(const char* start, const char* end);
	char* copyAndAllocateString(const char* str);
}  // TFE_DarkForces