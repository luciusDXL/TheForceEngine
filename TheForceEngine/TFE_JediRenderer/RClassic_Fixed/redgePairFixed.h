#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "../redgePair.h"
#include "../rmath.h"

namespace TFE_JediRenderer
{
	namespace RClassic_Fixed
	{
		void edgePair_setup(s32 length, s32 x0, fixed16_16 dyFloor_dx, fixed16_16 yFloor1, fixed16_16 yFloor, fixed16_16 dyCeil_dx, fixed16_16 yCeil, fixed16_16 yCeil1, EdgePair* flat);
	}
}
