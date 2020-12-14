#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "fixedPoint.h"

namespace TFE_JediRenderer
{
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

	inline s32 sign(s32 x)
	{
		return x < 0 ? -1 : 1;
	}

	inline s32 signZero(s32 x)
	{
		if (x == 0) { return 0; }
		return x < 0 ? -1 : 1;
	}

	inline s32 signV2A(s32 x) { return (x < 0 ? 1 : 0); }

	inline fixed16_16 dotFixed(vec3 v0, vec3 v1) { return mul16(v0.x.f16_16, v1.x.f16_16) + mul16(v0.y.f16_16, v1.y.f16_16) + mul16(v0.z.f16_16, v1.z.f16_16); }

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

	inline s32 roundFloat(f32 x)
	{
		return s32(x + 0.5f);
	}

	inline s32 floorFloat(f32 x)
	{
		return (s32)floorf(x);
	}

	inline s32 sign(f32 x)
	{
		return x < 0.0f ? -1 : 1;
	}

	inline s32 signZero(f32 x)
	{
		if (x == 0.0f) { return 0; }
		return x < 0.0f ? -1 : 1;
	}

	inline s32 signV2A(f32 x) { return (x < 0.0f ? 1 : 0); }

	inline f32 dotFloat(vec3 v0, vec3 v1) { return v0.x.f32*v1.x.f32 + v0.y.f32*v1.y.f32 + v0.z.f32*v1.z.f32; }
}
