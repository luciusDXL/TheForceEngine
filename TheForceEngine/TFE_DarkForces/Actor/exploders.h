#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Handles exploding actors - Exploding Barrels, and pre-placed
// Landmines.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include "actor.h"

namespace TFE_DarkForces
{
	void barrel_setup(SecObject* obj, LogicSetupFunc* setupFunc);
	void landmine_setup(SecObject* obj, LogicSetupFunc* setupFunc);
}  // namespace TFE_DarkForces