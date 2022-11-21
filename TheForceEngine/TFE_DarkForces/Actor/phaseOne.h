#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Handles the Phase One Dark Trooper AI.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include "actor.h"

namespace TFE_DarkForces
{
	void phaseOne_exit();
	void phaseOne_precache();
	Logic* phaseOne_setup(SecObject* obj, LogicSetupFunc* setupFunc);
	void phaseOne_serialize(Logic*& logic, SecObject* obj, Stream* stream);
}  // namespace TFE_DarkForces