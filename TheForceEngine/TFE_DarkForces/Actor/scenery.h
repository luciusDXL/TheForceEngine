#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Handles destroyable scenery.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include "actor.h"

namespace TFE_DarkForces
{
	Logic* scenery_setup(SecObject* obj, LogicSetupFunc* setupFunc);
}  // namespace TFE_DarkForces