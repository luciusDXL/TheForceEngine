#pragma once
#include <TFE_System/types.h>

namespace TFE_JediRenderer
{
	#define MAX_SEG				384 // Maximum number of wall segments visible in the frame.
	#define MAX_ADJOIN_SEG		128 // Maximum number of adjoin segements visible in the frame.
	#define MAX_SPLIT_WALLS		 40 // Maximum number of times walls can be split in a sector.
	#define MAX_ADJOIN_DEPTH	 40 // Maximum number of adjoin depth - basically how many adjoins you can see through.
	#define LIGHT_SOURCE_LEVELS	128 // Number of levels in the light source (like the headlamp or weapon fire).
	#define LIGHT_LEVELS		 32 // Number of light levels, maximum = LIGHT_LEVELS - 1
	#define MAX_LIGHT_LEVEL (LIGHT_LEVELS-1)

	#define SPRITE_SCALE_FIXED (ONE_16 * 10)
	#define SPRITE_SCALE_FLOAT 10.0f
}