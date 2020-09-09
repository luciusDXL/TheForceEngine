#pragma once
//////////////////////////////////////////////////////////////////////
// 12.20 Fixed point, designed for inner scanline/column loops only.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <math.h>
#include <assert.h>

// Set to 1 to assert on overflow, to ferret out precision issues with the Classic Renderer.
// Note, however, that overflow is expected when running at 320x200, so this should be 0 when making 
// releases.
#define ASSERT_ON_OVERFLOW_20 0

namespace TFE_JediRenderer
{
	// Constants
	#define HALF_20 0x80000
	#define ONE_20  0x100000

	#define FRAC_BITS_20 20ll
	#define FLOAT_SCALE_20 1048576.0f

	// Fixed point type
	typedef s64 fixed44_20;

	// multiplies 2 fixed point numbers, the result is fixed point.
	// 16.16 * 16.16 overflows 32 bit, so the calculation is done in 64 bit and then shifted back to 32 bit.
	inline fixed44_20 mul20(fixed44_20 x, fixed44_20 y)
	{
		return (x * y) >> FRAC_BITS_20;
	}

	// divides 2 fixed point numbers, the result is fixed point.
	// 16.16 * FIXED_ONE overflows 32 bit, so the calculation is done in 64 bit but the result can be safely cast back to 32 bit.
	inline fixed44_20 div20(fixed44_20 num, fixed44_20 denom)
	{
		return (num << FRAC_BITS_20) / denom;
	}

	// truncates a 16.16 fixed point number, returns an int: x >> 16
	inline s32 floor20(fixed44_20 x)
	{
		return s32(x >> FRAC_BITS_20);
	}

	// computes a * b / c while keeping everything in 64 bits until the end.
	inline fixed44_20 fusedMulDiv20(fixed44_20 a, fixed44_20 b, fixed44_20 c)
	{
		return (a * b) / c;
	}
	
	// computes a * b * c while keeping everything in 64 bits until the end.
	inline fixed44_20 fusedMulMul20(fixed44_20 a, fixed44_20 b, fixed44_20 c)
	{
		return (a * b * c) >> (FRAC_BITS_20 + FRAC_BITS_20);
	}

	// rounds a 16.16 fixed point number, returns an int: (x + HALF_16) >> 16
	inline s32 round20(fixed44_20 x)
	{
		return s32((x + HALF_20) >> FRAC_BITS_20);
	}

	// converts an integer to a fixed point number: x << 16
	inline fixed44_20 intToFixed20(s32 x)
	{
		return fixed44_20(x) << FRAC_BITS_20;
	}

	inline fixed44_20 floatToFixed20(f32 x)
	{
		const f64 x64 = f64(x) * f64(FLOAT_SCALE_20);
		return fixed44_20(x64);
	}
}
