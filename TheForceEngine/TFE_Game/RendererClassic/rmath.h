#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "fixedPoint.h"

struct vec2
{
	fixed16_16 x, z;
};

struct vec3
{
	fixed16_16 x, y, z;
};

namespace RMath
{
	inline s32 abs(s32 x)
	{
		return x < 0 ? -x : x;
	}

	inline s32 min(s32 a, s32 b)
	{
		return a < b ? a : b;
	}

	inline s32 max(s32 a, s32 b)
	{
		return a > b ? a : b;
	}

	inline s32 clamp(s32 x, s32 a, s32 b)
	{
		return min(max(x, a), b);
	}

	inline fixed48_16 abs(fixed48_16 x)
	{
		return x < 0 ? -x : x;
	}

	inline fixed48_16 min(fixed48_16 a, fixed48_16 b)
	{
		return a < b ? a : b;
	}

	inline fixed48_16 max(fixed48_16 a, fixed48_16 b)
	{
		return a > b ? a : b;
	}

	inline fixed48_16 clamp(fixed48_16 x, fixed48_16 a, fixed48_16 b)
	{
		return min(max(x, a), b);
	}
}
