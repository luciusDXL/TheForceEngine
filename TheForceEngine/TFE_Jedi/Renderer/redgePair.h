#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

namespace TFE_Jedi
{
	// A pair of edges along a single surface.
	// This can represent either the top and bottom of walls for flat rendering or
	// the top and bottom edges of an adjoin.
	struct EdgePair
	{
		// Ceiling
		fixed16_16 yCeil0;
		fixed16_16 yCeil1;
		fixed16_16 dyCeil_dx;

		s32 yPixel_C0;
		s32 yPixel_C1;

		// Floor
		fixed16_16 yFloor0;
		fixed16_16 yFloor1;
		fixed16_16 dyFloor_dx;

		s32 yPixel_F0;
		s32 yPixel_F1;

		// Screen X parameters, [x0, x1] is inclusive screen pixels.
		s32 lengthInPixels;
		s32 x0;
		s32 x1;
	};
}
