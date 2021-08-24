#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Random number generator
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include "logic.h"
#include "time.h"

namespace TFE_DarkForces
{
	// Generate a random value as: fixed16_16(value) * { random between 0.0 and 1.0 }
	s32 random(u32 value);

	extern u32 s_seed;
}  // namespace TFE_DarkForces