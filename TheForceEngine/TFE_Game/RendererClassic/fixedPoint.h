#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <math.h>

#define HALF_16 0x8000
#define ONE_16 0x10000

namespace FixedPoint
{
	// multiplies 2 fixed point numbers, the result is fixed point.
	// 16.16 * 16.16 overflows 32 bit, so the calculation is done in 64 bit and then shifted back to 32 bit.
	inline s32 mul16(s32 x, s32 y)
	{
		const s64 x64 = s64(x);
		const s32 y64 = s64(y);

		return s32((x64 * y64) >> 16ll);
	}

	// divides 2 fixed point numbers, the result is fixed point.
	// 16.16 * FIXED_ONE overflows 32 bit, so the calculation is done in 64 bit but the result can be safely cast back to 32 bit.
	inline s32 div16(s32 num, s32 denom)
	{
		const s64 num64 = s64(num);
		const s64 den64 = s64(denom);

		return s32((num64 << 16ll) / den64);
	}

	// truncates a 16.16 fixed point number, returns an int: x >> 16
	inline s32 floor16(s32 x)
	{
		return x >> 16;
	}

	// rounds a 16.16 fixed point number, returns an int: (x + HALF_16) >> 16
	inline s32 round16(s32 x)
	{
		return (x + HALF_16) >> 16;
	}

	// converts an integer to a fixed point number: x << 16
	inline s32 intToFixed16(s32 x)
	{
		return x << 16;
	}

	inline s32 fixed16to12(s32 x)
	{
		return x >> 4;
	}
}
