#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Handles the Mousebot AI.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include "actor.h"

namespace TFE_DarkForces
{
	Logic* mousebot_setup(SecObject* obj, LogicSetupFunc* setupFunc);
	void   mousebot_clear();
}  // namespace TFE_DarkForces