#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include "../redgePair.h"

namespace TFE_Jedi
{
	namespace RClassic_Float
	{
		void edgePair_setup(s32 length, s32 x0, fixed16_16 dyFloor_dx, fixed16_16 yFloor1, fixed16_16 yFloor, fixed16_16 dyCeil_dx, fixed16_16 yCeil, fixed16_16 yCeil1, EdgePairFixed* flat);
	}
}
