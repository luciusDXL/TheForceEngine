#pragma once
//////////////////////////////////////////////////////////////////////////////
// 44.20 Fixed point (64 bit), designed for inner scanline/column loops only.
//////////////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <math.h>
#include <assert.h>

namespace TFE_Jedi
{
	// Constants
	#define HALF_20 0x80000
	#define ONE_20  0x100000

	#define FRAC_BITS_20 20ll
	#define FLOAT_SCALE_20 1048576.0

	// Fixed point type
	typedef s64 fixed44_20;

	// multiplies 2 fixed point numbers, the result is fixed point.
	inline fixed44_20 mul20(fixed44_20 x, fixed44_20 y)
	{
		return (x * y) >> FRAC_BITS_20;
	}

	// divides 2 fixed point numbers, the result is fixed point.
	inline fixed44_20 div20(fixed44_20 num, fixed44_20 denom)
	{
		return (num << FRAC_BITS_20) / denom;
	}

	// truncates a 44.20 fixed point number, returns an int: x >> 20
	inline s32 floor20(fixed44_20 x)
	{
		return s32(x >> FRAC_BITS_20);
	}

	// computes a * b / c without intermediate operations.
	inline fixed44_20 fusedMulDiv20(fixed44_20 a, fixed44_20 b, fixed44_20 c)
	{
		return (a * b) / c;
	}

	// rounds a 44.20 fixed point number, returns an int: (x + HALF_20) >> 20
	inline s32 round20(fixed44_20 x)
	{
		return s32((x + HALF_20) >> FRAC_BITS_20);
	}

	// converts an integer to a fixed point number: x << 20
	inline fixed44_20 intToFixed20(s32 x)
	{
		return fixed44_20(x) << FRAC_BITS_20;
	}

	// converts a floating point value to 64-bit fixed point value.
	// note the conversion to double before scaling to avoid overflow.
	inline fixed44_20 floatToFixed20(f32 x)
	{
		const f64 x64 = f64(x) * FLOAT_SCALE_20;
		return fixed44_20(x64);
	}
}
