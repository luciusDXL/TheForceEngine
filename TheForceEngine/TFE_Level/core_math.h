#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "fixedPoint.h"

namespace TFE_CoreMath
{
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

	inline fixed16_16 dotFixed(vec3_fixed v0, vec3_fixed v1) { return mul16(v0.x, v1.x) + mul16(v0.y, v1.y) + mul16(v0.z, v1.z); }

	inline fixed16_16 dot(const vec3_fixed* v0, const vec3_fixed* v1)
	{
		return mul16(v0->x, v1->x) + mul16(v0->y, v1->y) + mul16(v0->z, v1->z);
	}

	void normalizeVec3(vec3_fixed* vIn, vec3_fixed* vOut);

	// Convert from integer angle to fixed point sin/cos.
	inline void sinCosFixed(angle14_32 angle, fixed16_16* sinValue, fixed16_16* cosValue)
	{
		// Cheat and use floating point sin/cos functions...
		// TODO: Use the original table-based calculation? -- This probably isn't necessary.
		const f32 scale = -2.0f * PI / 16484.0f;
		const f32 s = sinf(scale * f32(angle));
		const f32 c = cosf(scale * f32(angle));

		*sinValue = floatToFixed16(s);
		*cosValue = floatToFixed16(c);
	}

	fixed16_16 vec2Length(fixed16_16 dx, fixed16_16 dz)
	{
		// Trade precision to avoid overflow.
		// The same as: 2 * sqrt( (dx/2)^2 + (dz/2)^2 ) = 2 * sqrt( (dx^2)/4 + (dz^2)/4 )
		return fixedSqrt((mul16(dx, dx)>>2) + (mul16(dz, dz)>>2)) << 1;
	}

	fixed16_16 computeDirAndLength(fixed16_16 dx, fixed16_16 dz, fixed16_16* dirX, fixed16_16* dirZ);

	void computeTransformFromAngles_Fixed(angle14_32 yaw, angle14_32 pitch, angle14_32 roll, fixed16_16* transform);

	// Returns an DF angle, where 360 degrees = 16384 angular units (~45.5 units / degree).
	angle14_32 vec2ToAngle(fixed16_16 dx, fixed16_16 dz);

	// This is an approximate distance between points.
	// It is basically manhattan distance - smallest component / 2
	// dist = |dx| + |dz| - min(|dx|, |dz|)/2
	fixed16_16 distApprox(fixed16_16 x0, fixed16_16 z0, fixed16_16 x1, fixed16_16 z1)
	{
		fixed16_16 dx = abs(x1 - x0);
		fixed16_16 dz = abs(z1 - z0);
		return dx + dz - (min(dx, dz) >> 1);
	}

	angle14_32 getAngleDifference(angle14_32 angle0, angle14_32 angle1);

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

	inline f32 dotFloat(vec3_float v0, vec3_float v1) { return v0.x*v1.x + v0.y*v1.y + v0.z*v1.z; }

	inline f32 dot(const vec3_float* v0, const vec3_float* v1)
	{
		return v0->x*v1->x + v0->y*v1->y + v0->z*v1->z;
	}

	void normalizeVec3(vec3_float* vIn, vec3_float* vOut);

	inline void sinCosFlt(f32 angle, f32& sinValue, f32& cosValue)
	{
		const f32 scale = -PI / 180.0f;
		sinValue = sinf(scale * angle);
		cosValue = cosf(scale * angle);
	}

	void computeTransformFromAngles_Float(f32 yaw, f32 pitch, f32 roll, f32* transform);

	inline u32 previousPowerOf2(u32 x)
	{
		if (x == 0) { return 0; }

		x |= (x >> 1);
		x |= (x >> 2);
		x |= (x >> 4);
		x |= (x >> 8);
		x |= (x >> 16);
		return x - (x >> 1);
	}
}
