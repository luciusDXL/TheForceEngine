#include "random.h"
#include <TFE_Jedi/Serialization/serialization.h>
#include <TFE_System/system.h>

namespace TFE_DarkForces
{
	// TODO(Core Game Loop Release): Figure out what this value is at program start up. The value here is from when the program was already running.
	// This should cause the random numbers to match up between TFE and DOS.
	static u32 s_seed = 0xf444bb3b;

	void random_serialize(Stream* stream)
	{
		SERIALIZE(SaveVersionInit, s_seed, 0xf444bb3b);
	}

	s32 getSeed()
	{
		return s_seed;
	}

	void setSeed(s32 seed)
	{
		s_seed = seed;
	}

	s32 random_next()
	{
		// Shift the seed by 1 (divide by two), and
		// xor the top 8 bits with b10100011 (163) if the starting seed is odd.
		// This has the effect of increasing the size of the seed if it gets too small due to the shifts, and increasing "randomness"
		if (s_seed & 1)
		{
			s_seed = (s_seed >> 1) ^ 0xa3000000;
		}
		else
		{
			s_seed = (s_seed >> 1);
		}
		return s32(s_seed);
	}

	// Generate a random value as: 
	// 1) random_next() or 
	// 2) fixed16_16(value) * fract(random_next())
	s32 random(s32 value)
	{
		s32 newValue = random_next();
		if (newValue > value || newValue < 0)
		{
			// Note the value is cast to fixed16_16 but is not actually converted.
			// This means that if the incoming value is not fixed point, the result will also not be fixed point.
			newValue = mul16(fixed16_16(value), fract16(newValue));
		}
		return newValue;
	}

	void random_seed(u32 seed)
	{
		s_seed = seed;
	}
}  // TFE_DarkForces