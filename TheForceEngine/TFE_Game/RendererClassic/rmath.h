#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "fixedPoint.h"

struct vec2
{
	s32 x, z;
};

struct vec3
{
	s32 x, y, z;
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
}
