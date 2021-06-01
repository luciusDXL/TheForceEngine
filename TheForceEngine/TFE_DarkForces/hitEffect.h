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
		HEFFECT_NONE        = -1,   // no visible effect.
		HEFFECT_SMALL_EXP    = 0,   // small "puff" - blaster weapons.
		HEFFECT_THERMDET_EXP = 1,   // thermal detonator explosion.
		HEFFECT_PLASMA_EXP   = 2,   // plasma "puff".
		HEFFECT_CONCUSSION   = 4,   // concussion - first stage.
		HEFFECT_CONCUSSION2  = 5,   // concussion - second stage.
		HEFFECT_MISSILE_EXP  = 6,   // missile explosion.
		HEFFECT_CANNON_EXP   = 9,   // cannon "puff".
		HEFFECT_REPEATER_EXP = 10,  // repeater "puff".
		HEFFECT_LARGE_EXP    = 11,  // large explosion such as land mine.
		HEFFECT_EXP_BARREL   = 12,  // exploding barrel.
		HEFFECT_SPLASH       = 14,  // water splash
		HEFFECT_COUNT,
	};

	// Spawn a new hit effect at location (x,y,z) in 'sector'.
	// The ExcludeObj field is used to avoid effecting a specific object during wakeup or explosions.
	void spawnHitEffect(HitEffectID hitEffectId, RSector* sector, fixed16_16 x, fixed16_16 y, fixed16_16 z, SecObject* excludeObj);

	// Logic update function, handles the update of all projectiles.
	void hitEffectLogicFunc();

	// Called when a specific type of hit effect has finished animating.
	void hitEffectFinished();
}  // namespace TFE_DarkForces