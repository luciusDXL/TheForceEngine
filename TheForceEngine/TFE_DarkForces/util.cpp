#include <cstring>

#include "util.h"
#include <TFE_Game/igame.h>

namespace TFE_DarkForces
{
	char* copyAndAllocateString(const char* start, const char* end)
	{
		s32 count = s32(end - start);
		char* outString = (char*)game_alloc(count + 1);
		memcpy(outString, start, count);
		outString[count] = 0;

		return outString;
	}

	char* copyAndAllocateString(const char* str)
	{
		size_t count = strlen(str);
		char* outString = (char*)game_alloc(count + 1);
		memcpy(outString, str, count);
		outString[count] = 0;

		return outString;
	}

	char* strCopyAndZero(char* dst, const char* src, s32 bufferSize)
	{
		char* dstPtr = dst;
		while (bufferSize > 0)
		{
			char ch = *src;
			if (ch == 0) { break; }
			bufferSize--;
			*dst = ch;

			src++;
			dst++;
		}
		while (bufferSize)
		{
			bufferSize--;
			*dst = 0;
			dst++;
		}
		return dstPtr;
	}

	s32 strToInt(const char* param)
	{
		char* endPtr = nullptr;
		return strtol(param, &endPtr, 10);
	}

	u32 strToUInt(const char* param)
	{
		return (u32)strToInt(param);
	}
}  // TFE_DarkForces