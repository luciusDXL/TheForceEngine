#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Handles the Sewer Creature AI (Dianoga)
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include "actor.h"

namespace TFE_DarkForces
{
	Logic* sewerCreature_setup(SecObject* obj, LogicSetupFunc* setupFunc);
}  // namespace TFE_DarkForces