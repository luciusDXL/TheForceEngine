#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "redgePair.h"
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

namespace TFE_Jedi
{
	bool flat_buildScanlineCeiling(s32& i, s32 count, s32& x, s32 y, s32& left, s32& right, s32& scanlineLength, const EdgePairFixed* edges);
	bool flat_buildScanlineFloor(s32& i, s32 count, s32& x, s32 y, s32& left, s32& right, s32& scanlineLength, const EdgePairFixed* edges);
	void clipScanline(s32* left, s32* right, s32 y);
}
