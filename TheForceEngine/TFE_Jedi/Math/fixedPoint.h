#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <math.h>
#include <assert.h>

// Set to 1 to assert on overflow, to ferret out precision issues with the Classic Renderer.
// Note, however, that overflow is expected when running at 320x200, so this should be 0 when making 
// releases.
#define ASSERT_ON_OVERFLOW 0

namespace TFE_Jedi
{
	// Constants
	#define HALF_16 0x8000
	#define ONE_16  0x10000
	#define ANGLE_MASK 0x3fff
	#define ANGLE_MAX  0x4000

	#define FRAC_BITS_16 16ll
	#define FRAC_MASK_16 ((1 << FRAC_BITS_16) - 1)
	#define FLOAT_SCALE_16 65536.0f
	#define INV_FLOAT_SCALE_16 (1.0f/FLOAT_SCALE_16)
	#define ANGLE_TO_FIXED_SCALE 4

	// Fixed point type
	typedef s32 fixed16_16;		// 16.16 fixed point
	typedef s32 angle14_32;		// 14-bit angle in 32-bits (maps 360 degrees to 16384 angle units).
	typedef s16 angle14_16;		// 14-bit angle in 16-bits (maps 360 degrees to 16384 angle units).

	// Simple way of setting readable fixed point constants.
	#define FIXED(x) ((x) << FRAC_BITS_16)
	
	// multiplies 2 fixed point numbers, the result is fixed point.
	// 16.16 * 16.16 overflows 32 bit, so the calculation is done in 64 bit and then shifted back to 32 bit.
	inline fixed16_16 mul16(fixed16_16 x, fixed16_16 y)
	{
		const s64 x64 = s64(x);
		const s64 y64 = s64(y);

		// Overflow precision test.
		#if ASSERT_ON_OVERFLOW == 1
		assert(((x64 * y64) >> FRAC_BITS_16) == fixed16_16((x64 * y64) >> FRAC_BITS_16));
		#endif

		return fixed16_16((x64 * y64) >> FRAC_BITS_16);
	}

	// divides 2 fixed point numbers, the result is fixed point.
	// 16.16 * FIXED_ONE overflows 32 bit, so the calculation is done in 64 bit but the result can be safely cast back to 32 bit.
	inline fixed16_16 div16(fixed16_16 num, fixed16_16 denom)
	{
		const s64 num64 = s64(num);
		const s64 den64 = s64(denom);

		// Overflow precision test.
		#if ASSERT_ON_OVERFLOW == 1
		assert(((num64 << FRAC_BITS_16) / den64) == fixed16_16((num64 << FRAC_BITS_16) / den64));
		#endif

		return fixed16_16((num64 << FRAC_BITS_16) / den64);
	}

	// truncates a 16.16 fixed point number, returns an int: x >> 16
	inline s32 floor16(fixed16_16 x)
	{
		return s32(x >> FRAC_BITS_16);
	}

	inline fixed16_16 fract16(fixed16_16 x)
	{
		return x & FRAC_MASK_16;
	}

	// computes a * b / c while keeping everything in 64 bits until the end.
	inline fixed16_16 fusedMulDiv(fixed16_16 a, fixed16_16 b, fixed16_16 c)
	{
		const s64 a64 = s64(a);
		const s64 b64 = s64(b);
		const s64 c64 = s64(c);
		s64 value = (a64 * b64) / c64;

		// Overflow precision test.
		#if ASSERT_ON_OVERFLOW == 1
		assert(((a64 * b64) / c64) == fixed16_16((a64 * b64) / c64));
		#endif

		return fixed16_16(value);
	}

	// rounds a 16.16 fixed point number, returns an int: (x + HALF_16) >> 16
	inline s32 round16(fixed16_16 x)
	{
		return s32((x + HALF_16) >> FRAC_BITS_16);
	}

	// converts an integer to a fixed point number: x << 16
	inline fixed16_16 intToFixed16(s32 x)
	{
		return fixed16_16(x) << FRAC_BITS_16;
	}

	inline fixed16_16 floatToFixed16(f32 x)
	{
		return fixed16_16(x * FLOAT_SCALE_16);
	}

	inline f32 fixed16ToFloat(fixed16_16 x)
	{
		return f32(x) * INV_FLOAT_SCALE_16;
	}

	inline angle14_16 floatToAngle(f32 angle)
	{
		return angle14_16(angle * 45.5111f);
	}

	inline f32 angleToFloat(angle14_16 angle)
	{
		return angle / 45.5111f;
	}

	// Convert a floating point angle from [0, 2pi) to a fixed point value where 0 -> 0 and 2pi -> 16384
	// This isn't really the same as fixed16_16 but matches the original source.
	inline fixed16_16 floatAngleToFixed(f32 angle)
	{
		return fixed16_16(16384.0f * angle / (2.0f*PI));
	}

	// Convert a floating point angle from [0, 2pi) to a fixed point value where 0 -> 0 and 2pi -> 16384
	// This isn't really the same as fixed16_16 but matches the original source.
	inline fixed16_16 floatDegreesToFixed(f32 angle)
	{
		return fixed16_16(16384.0f * angle / 360.0f);
	}

	inline s32 fixed16to12(fixed16_16 x)
	{
		return s32(x >> 4);
	}

	// I cheat here with the fixedSqrt and just do a regular square root...
	// TODO: Replace with 15-bit table-based sqrt? -- Also probably not necessary.
	inline fixed16_16 fixedSqrt(fixed16_16 x)
	{
		const f32 fx = fixed16ToFloat(x);
		return floatToFixed16(sqrtf(fx));
	}
}
