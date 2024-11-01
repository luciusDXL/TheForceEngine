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
	enum PhaseOneStates
	{
		P1STATE_DEFAULT = 0,
		P1STATE_CHARGE,
		P1STATE_WANDER,
		P1STATE_ATTACK,
		P1STATE_DYING,
		P1STATE_SEARCH,
	};

	void phaseOne_exit();
	void phaseOne_precache();
	Logic* phaseOne_setup(SecObject* obj, LogicSetupFunc* setupFunc);
	void phaseOne_serialize(Logic*& logic, SecObject* obj, Stream* stream);
}  // namespace TFE_DarkForces