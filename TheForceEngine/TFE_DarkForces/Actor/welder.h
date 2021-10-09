#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Handles the Welder AI.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include "actor.h"

namespace TFE_DarkForces
{
	Logic* welder_setup(SecObject* obj, LogicSetupFunc* setupFunc);
}  // namespace TFE_DarkForces