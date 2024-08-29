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
	void bobaFett_exit();
	void bobaFett_precache();
	Logic* bobaFett_setup(SecObject* obj, LogicSetupFunc* setupFunc);
	void bobaFett_serialize(Logic*& logic, SecObject* obj, vpFile* stream);
}  // namespace TFE_DarkForces