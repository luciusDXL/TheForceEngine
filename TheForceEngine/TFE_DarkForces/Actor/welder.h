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
	void welder_serialize(Logic*& logic, SecObject* obj, vpFile* stream);
	void welder_clear();
	void welder_exit();
	void welder_precache();
}  // namespace TFE_DarkForces