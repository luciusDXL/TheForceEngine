#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "fixedPoint.h"
#include "rmath.h"

namespace RClassicLighting
{
	const u8* computeLighting(fixed16 depth, s32 lightOffset);
}