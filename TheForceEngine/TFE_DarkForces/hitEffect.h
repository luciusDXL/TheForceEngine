#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Hit Effect - 
// Hit effects play a one-shot animation when a projectile hits
// something solid.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Level/rsector.h>
#include <TFE_Level/robject.h>

namespace TFE_DarkForces
{
	enum HitEffectID
	{
		HEFFECT_NONE         = -1,  // no hit effect
		HEFFECT_SMALL_EXP    = 0,   // used by most weapons
		HEFFECT_PLASMA_EXP   = 2,   // plasma
		HEFFECT_CONCUSSION   = 5,   // concussion
		HEFFECT_MISSILE_EXP  = 6,   // missile
		HEFFECT_CANNON_EXP   = 9,   // cannon
		HEFFECT_REPEATER_EXP = 10,  // repeater
		HEFFECT_LARGE_EXP    = 11,  // large explosion such as land mine.
	};
}  // namespace TFE_DarkForces