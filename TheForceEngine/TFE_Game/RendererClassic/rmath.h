#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "fixedPoint.h"

struct vec2
{
	fixed16 x, z;
};

struct vec3
{
	fixed16 x, y, z;
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

#if ENABLE_HIGH_PRECISION_FIXED_POINT == 1
	inline fixed16 abs(fixed16 x)
	{
		return x < 0 ? -x : x;
	}

	inline fixed16 min(fixed16 a, fixed16 b)
	{
		return a < b ? a : b;
	}

	inline fixed16 max(fixed16 a, fixed16 b)
	{
		return a > b ? a : b;
	}

	inline fixed16 clamp(fixed16 x, fixed16 a, fixed16 b)
	{
		return min(max(x, a), b);
	}
#endif
}
