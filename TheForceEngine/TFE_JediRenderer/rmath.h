#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "fixedPoint.h"

// The "decimal" structure is a "32-bit decimal number" that can be either floating point or fixed point.
// This is here so that structures can be shared between fixed point and floating point based renderers to reduce code
// duplication.
struct decimal
{
	union
	{
		fixed16_16 f16_16;
		f32 f32;
	};
};

struct decimalPtr
{
	union
	{
		fixed16_16* f16_16;
		f32* f32;
	};
};

struct vec2
{
	decimal x, z;
};

struct vec3
{
	decimal x, y, z;
};

namespace RMath
{
	// Int/Fixed Point
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

	// Float
	inline f32 abs(f32 x)
	{
		return x < 0 ? -x : x;
	}

	inline f32 min(f32 a, f32 b)
	{
		return a < b ? a : b;
	}

	inline f32 max(f32 a, f32 b)
	{
		return a > b ? a : b;
	}

	inline f32 clamp(f32 x, f32 a, f32 b)
	{
		return min(max(x, a), b);
	}
}
