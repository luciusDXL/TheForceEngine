#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Handles the Phase Two Dark Trooper AI.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include "actor.h"

namespace TFE_DarkForces
{
	void phaseTwo_exit();
	void phaseTwo_precache();
	Logic* phaseTwo_setup(SecObject* obj, LogicSetupFunc* setupFunc);
	void phaseTwo_serialize(Logic*& logic, SecObject* obj, Stream* stream);
}  // namespace TFE_DarkForces