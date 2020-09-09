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
	namespace RClassic_Float
	{
		void edgePair_setup(s32 length, s32 x0, f32 dyFloor_dx, f32 yFloor1, f32 yFloor, f32 dyCeil_dx, f32 yCeil, f32 yCeil1, EdgePair* flat);
	}
}
