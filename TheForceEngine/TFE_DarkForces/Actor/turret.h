#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Handles the Ceiling Turret AI.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include "actor.h"

namespace TFE_DarkForces
{
	void turret_exit();
	Logic* turret_setup(SecObject* obj, LogicSetupFunc* setupFunc);
}  // namespace TFE_DarkForces