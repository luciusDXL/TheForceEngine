#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "redgePair.h"
#include "rmath.h"

namespace TFE_JediRenderer
{
	bool flat_buildScanlineCeiling(s32& i, s32 count, s32& x, s32 y, s32& left, s32& right, s32& scanlineLength, const EdgePair* edges);
	bool flat_buildScanlineFloor(s32& i, s32 count, s32& x, s32 y, s32& left, s32& right, s32& scanlineLength, const EdgePair* edges);
	void clipScanline(s32* left, s32* right, s32 y);
}
