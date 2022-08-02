#pragma once
#include <TFE_System/types.h>

namespace TFE_Jedi
{
	#define MAX_DRAWN_SPRITE_STORE 32 // Maximum number of drawn sprites stored for future reference (like autoaim).
	#define MAX_SEG				384 // Maximum number of wall segments visible in the frame.
	#define MAX_ADJOIN_SEG		128 // Maximum number of adjoin segements visible in the frame.
	#define MAX_SPLIT_WALLS		 40 // Maximum number of times walls can be split in a sector.
	#define MAX_ADJOIN_DEPTH	 40 // Maximum adjoin depth - basically how many adjoins you can see through.
	#define MAX_VIEW_OBJ_COUNT	128 // Maximum number of rendered objects in a single sector / view.
	#define LIGHT_SOURCE_LEVELS	128 // Number of levels in the light source (like the headlamp or weapon fire).
	#define LIGHT_LEVELS		 32 // Number of light levels, maximum = LIGHT_LEVELS - 1
	#define MAX_LIGHT_LEVEL (LIGHT_LEVELS-1)

	// Extended Max Limits
	#define MAX_SEG_EXT	         2048 // Maximum number of wall segments with extended limits, this allows for ~1 wall/pixel column @1080p like vanilla @ 320x200
	#define MAX_ADJOIN_SEG_EXT   1024 // Maximum number of adjoin segments with extended limits.
	#define MAX_ADJOIN_DEPTH_EXT 255  // Maximum adjoin recursion depth with extended limits.
}