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
	void resetTurretNum();
	void turret_exit();
	void turret_precache();
	Logic* turret_setup(SecObject* obj, LogicSetupFunc* setupFunc);
	void turret_serialize(Logic*& logic, SecObject* obj, Stream* stream);
}  // namespace TFE_DarkForces