#include "random.h"

namespace TFE_DarkForces
{
	u32 s_seed;

	// This divides s_seed by 2 - 
	// If it is odd, then it applies a xor to the top 8 bits:
	// b:     10100011
	// 0x7a = 01111010
	// 0xd9 = 11011001
	u32 random_next()
	{
		if (s_seed & 1)
		{
			// This leaves the lower 24 bits alone.
			// And then modifies the top.
			// This is only applied if s_seed is not cleanly divisible by 2.
			s_seed = (s_seed >> 1) ^ 0xa3000000;
			return;
		}
		s_seed = (s_seed >> 1);
		return s_seed;
	}

	// Generate a random value as: 
	// 1) random_next() or 
	// 2) fixed16_16(value) * random_next()
	s32 random(u32 value)
	{
		u32 newValue = random_next();
		if (newValue > value)
		{
			fixed16_16 fractNew = fixed16_16(newValue & 0xffff);
			newValue = mul16(fixed16_16(value), fractNew);
		}
		return s32(newValue);
	}
}  // TFE_DarkForces