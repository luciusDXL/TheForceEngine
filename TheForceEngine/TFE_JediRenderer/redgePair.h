#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "rmath.h"

namespace TFE_JediRenderer
{
	// A pair of edges along a single surface.
	// This can represent either the top and bottom of walls for flat rendering or
	// the top and bottom edges of an adjoin.
	struct EdgePair
	{
		// Ceiling
		decimal yCeil0;
		decimal yCeil1;
		decimal dyCeil_dx;

		s32 yPixel_C0;
		s32 yPixel_C1;

		// Floor
		decimal yFloor0;
		decimal yFloor1;
		decimal dyFloor_dx;

		s32 yPixel_F0;
		s32 yPixel_F1;

		// Screen X parameters, [x0, x1] is inclusive screen pixels.
		s32 lengthInPixels;
		s32 x0;
		s32 x1;
	};
}
