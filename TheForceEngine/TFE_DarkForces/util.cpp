#include "util.h"

namespace TFE_DarkForces
{
	char* copyAndAllocateString(const char* start, const char* end)
	{
		s32 count = s32(end - start);
		char* outString = (char*)malloc(count + 1);
		memcpy(outString, start, count);
		outString[count] = 0;

		return outString;
	}

	char* copyAndAllocateString(const char* str)
	{
		size_t count = strlen(str);
		char* outString = (char*)malloc(count + 1);
		memcpy(outString, str, count);
		outString[count] = 0;

		return outString;
	}
}  // TFE_DarkForces