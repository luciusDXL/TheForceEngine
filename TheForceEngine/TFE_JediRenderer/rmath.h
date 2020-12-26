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

	struct vec2_fixed
	{
		fixed16_16 x, z;
	};

	struct vec3_fixed
	{
		fixed16_16 x, y, z;
	};

	struct vec2_float
	{
		f32 x, z;
	};

	struct vec3_float
	{
		f32 x, y, z;
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

	inline fixed16_16 dot(const vec3_fixed* v0, const vec3_fixed* v1)
	{
		return mul16(v0->x, v1->x) + mul16(v0->y, v1->y) + mul16(v0->z, v1->z);
	}

	void normalizeVec3(vec3_fixed* vIn, vec3_fixed* vOut);

	// Convert from integer angle to fixed point sin/cos.
	inline void sinCosFixed(s32 angle, fixed16_16& sinValue, fixed16_16& cosValue)
	{
		// Cheat and use floating point sin/cos functions...
		const f32 scale = -2.0f * PI / 16484.0f;
		const f32 s = sinf(scale * f32(angle));
		const f32 c = cosf(scale * f32(angle));

		sinValue = floatToFixed16(s);
		cosValue = floatToFixed16(c);
	}

	void computeTransformFromAngles_Fixed(s16 yaw, s16 pitch, s16 roll, fixed16_16* transform);
	void transformPointByCamera(vec3_fixed* worldPoint, vec3_fixed* viewPoint);
	void transformPointByCameraFixed(vec3* worldPoint, vec3* viewPoint);

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

	void normalizeVec3(vec3_float* vIn, vec3_float* vOut);

	inline void sinCosFlt(f32 angle, f32& sinValue, f32& cosValue)
	{
		const f32 scale = -PI / 180.0f;
		sinValue = sinf(scale * angle);
		cosValue = cosf(scale * angle);
	}

	void computeTransformFromAngles_Float(f32 yaw, f32 pitch, f32 roll, f32* transform);
	void transformPointByCamera(vec3_float* worldPoint, vec3_float* viewPoint);
	void transformPointByCameraFloat(vec3* worldPoint, vec3* viewPoint);
}
