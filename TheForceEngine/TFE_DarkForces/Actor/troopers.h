#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Handles "trooper" enemies - Officers, Commandos, and Storm Troopers
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include "actor.h"

namespace TFE_DarkForces
{
	Logic* officer_setup(SecObject* obj, LogicSetupFunc* setupFunc, KEYWORD logicId);
}  // namespace TFE_DarkForces