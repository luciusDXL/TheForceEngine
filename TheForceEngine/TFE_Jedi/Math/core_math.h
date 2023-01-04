#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "fixedPoint.h"
#include "cosTable.h"

namespace TFE_Jedi
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

	inline void swap(s32& a, s32& b)
	{
		s32 tmp = a;
		a = b;
		b = tmp;
	}

	// Smoothly interpolate a value in range x0..x1 to a new value in range y0..y1.
	inline s32 interpolate(s32 value, s32 x0, s32 x1, s32 y0, s32 y1)
	{
		return y0 + (value - x0) * (y1 - y0) / (x1 - x0);
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
	void rotateVectorM3x3(vec3_fixed* inVec, vec3_fixed* outVec, s32* mtx);

	inline fixed16_16 tanFixed(angle14_32 angle)
	{
		angle &= ANGLE_MASK;
		if (angle < 8192)
		{
			return s_tanTable[angle];
		}
		return s_tanTable[angle - 8192];
	}
	
	inline void sinCosFixed(angle14_32 angle, fixed16_16* sinValue, fixed16_16* cosValue)
	{
		angle &= ANGLE_MASK;
		if (!(angle & 0x1000))
		{
			if (!(angle & 0x2000))	// Quadrant 1
			{
				assert(angle < 4096);
				*sinValue = s_cosTable[4096 - angle];
				*cosValue = s_cosTable[angle];
			}
			else  // Quadrant 3
			{
				assert(angle >= 8192 && angle < 12288);
				*sinValue = -s_cosTable[12288 - angle];
				*cosValue = -s_cosTable[angle - 8192];
			}
		}
		else
		{
			if (!(angle & 0x2000))  // Quadrant 2
			{
				assert(angle >= 4096 && angle < 8192);
				*sinValue =  s_cosTable[angle - 4096];
				*cosValue = -s_cosTable[8192 - angle];
			}
			else  // Quadrant 4
			{
				assert(angle >= 12288 && angle < 16384);
				*sinValue = -s_cosTable[angle - 12288];
				*cosValue =  s_cosTable[16384 - angle];
			}
		}
	}

	angle14_32 arcCosFixed(fixed16_16 sinAngle, angle14_32 angle);
		
	inline fixed16_16 sinFixed(angle14_32 angle)
	{
		angle &= ANGLE_MASK;
		if (!(angle & 0x1000))
		{
			if (!(angle & 0x2000))	// Quadrant 1
			{
				return s_cosTable[4096 - angle];
			}
			else  // Quadrant 3
			{
				return -s_cosTable[12288 - angle];
			}
		}
		if (!(angle & 0x2000))  // Quadrant 2
		{
			return s_cosTable[angle - 4096];
		}
		// Quadrant 4
		return -s_cosTable[angle - 12288];
	}

	inline fixed16_16 cosFixed(angle14_32 angle)
	{
		angle &= ANGLE_MASK;
		if (!(angle & 0x1000))
		{
			if (!(angle & 0x2000))	// Quadrant 1
			{
				return s_cosTable[angle];			// cos(angle)
			}
			else  // Quadrant 3
			{
				return -s_cosTable[angle - 8192];
			}
		}
		if (!(angle & 0x2000))  // Quadrant 2
		{
			return -s_cosTable[8192 - angle];
		}
		// Quadrant 4
		return s_cosTable[16384 - angle];
	}

	inline fixed16_16 vec2Length(fixed16_16 dx, fixed16_16 dz)
	{
		// Trade precision to avoid overflow.
		dx = (dx + ((dx < 0) ? -1 : 0)) >> 1;
		dz = (dz + ((dz < 0) ? -1 : 0)) >> 1;
		return fixedSqrt(mul16(dx, dx) + mul16(dz, dz)) << 1;
	}

	inline fixed16_16 vec3Length(fixed16_16 dx, fixed16_16 dy, fixed16_16 dz)
	{
		// Trade precision to avoid overflow.
		dx = (dx + ((dx < 0) ? -1 : 0)) >> 1;
		dy = (dy + ((dy < 0) ? -1 : 0)) >> 1;
		dz = (dz + ((dz < 0) ? -1 : 0)) >> 1;
		return fixedSqrt(mul16(dx, dx) + mul16(dy, dy) + mul16(dz, dz)) << 1;
	}

	fixed16_16 computeDirAndLength(fixed16_16 dx, fixed16_16 dz, fixed16_16* dirX, fixed16_16* dirZ);

	void computeTransformFromAngles_Fixed(angle14_32 yaw, angle14_32 pitch, angle14_32 roll, fixed16_16* transform);

	// Returns an DF angle, where 360 degrees = 16384 angular units (~45.5 units / degree).
	angle14_32 vec2ToAngle(fixed16_16 dx, fixed16_16 dz);

	// This is an approximate distance between points.
	// It is basically manhattan distance - smallest component / 2
	// dist = |dx| + |dz| - min(|dx|, |dz|)/2
	inline fixed16_16 distApprox(fixed16_16 x0, fixed16_16 z0, fixed16_16 x1, fixed16_16 z1)
	{
		fixed16_16 dx = abs(x1 - x0);
		fixed16_16 dz = abs(z1 - z0);
		return dx + dz - (min(dx, dz) >> 1);
	}

	angle14_32 getAngleDifference(angle14_32 angle0, angle14_32 angle1);
	void mulMatrix3x3(fixed16_16* mtx0, fixed16_16* mtx1, fixed16_16* mtxOut);

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
	void rotateVectorM3x3(vec3_float* inVec, vec3_float* outVec, f32* mtx);

	// Returns an DF angle, where 360 degrees = 16384 angular units (~45.5 units / degree).
	s32 vec2ToAngle(f32 dx, f32 dz);

	inline f32 tanFlt(f32 angle)
	{
		const f32 scale = 2.0f * PI / 16384.0f;
		return tanf(scale * angle);
	}

	inline void sinCosFlt(f32 angle, f32* sinValue, f32* cosValue)
	{
		const f32 scale = 2.0f * PI / 16384.0f;
		*sinValue = sinf(scale * angle);
		*cosValue = cosf(scale * angle);
	}

	inline f32 sinFlt(f32 angle)
	{
		const f32 scale = 2.0f * PI / 16384.0f;
		return sinf(scale * angle);
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

	// Size_t
	inline size_t min(size_t a, size_t b)
	{
		return a < b ? a : b;
	}

	inline size_t max(size_t a, size_t b)
	{
		return a > b ? a : b;
	}

	inline size_t clamp(size_t x, size_t a, size_t b)
	{
		return min(max(x, a), b);
	}
}
