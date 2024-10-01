#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Handles the main flying enemies - Probe and Interrogation Droids.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include "actor.h"

namespace TFE_DarkForces
{
	ThinkerModule* actor_createFlyingModule(Logic* logic);
	Logic* intDroid_setup(SecObject* obj, LogicSetupFunc* setupFunc);
	Logic* probeDroid_setup(SecObject* obj, LogicSetupFunc* setupFunc);
	Logic* remote_setup(SecObject* obj, LogicSetupFunc* setupFunc);
}  // namespace TFE_DarkForces