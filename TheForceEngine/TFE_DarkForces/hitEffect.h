#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Hit Effect - 
// Hit effects play a one-shot animation when a projectile hits
// something solid.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include "sound.h"

enum HitEffectID
{
	HEFFECT_NONE        = -1,   // no visible effect.
	HEFFECT_SMALL_EXP    = 0,	// small "puff" - blaster weapons.
	HEFFECT_THERMDET_EXP = 1,	// thermal detonator explosion.
	HEFFECT_PLASMA_EXP   = 2,	// plasma "puff".
	HEFFECT_MORTAR_EXP   = 3,	// mortar explosion
	HEFFECT_CONCUSSION   = 4,   // concussion - first stage.
	HEFFECT_CONCUSSION2  = 5,	// concussion - second stage.
	HEFFECT_MISSILE_EXP  = 6,	// missile explosion.
	HEFFECT_MISSILE_WEAK = 7,	// weaker version of the missle explosion.
	HEFFECT_PUNCH		 = 8,	// punch
	HEFFECT_CANNON_EXP   = 9,	// cannon "puff".
	HEFFECT_REPEATER_EXP = 10,	// repeater "puff".
	HEFFECT_LARGE_EXP    = 11,	// large explosion such as land mine.
	HEFFECT_EXP_BARREL   = 12,	// exploding barrel.
	HEFFECT_EXP_INVIS    = 13,	// an explosion that makes no sound and has no visual effect.
	HEFFECT_SPLASH       = 14,	// water splash
	HEFFECT_EXP_35		 = 15,  // medium explosion, 35 damage.
	HEFFECT_EXP_NO_DMG   = 16,	// medium explosion, no damage.
	HEFFECT_EXP_25		 = 17,	// medium explosion, 25 damage.
	HEFFECT_COUNT,
};

struct EffectData
{
	HitEffectID type;
	Wax* spriteData;
	fixed16_16 force;
	fixed16_16 damage;
	fixed16_16 explosiveRange;
	fixed16_16 wakeupRange;
	SoundSourceId soundEffect;
};

namespace TFE_DarkForces
{
	extern vec3_fixed s_explodePos;
	extern EffectData* s_curEffectData;

	void hitEffect_startup();
	void hitEffect_createTask();
	void hitEffect_clearState();

	// Serialization
	void hitEffect_serializeTasks(Stream* stream);

	// Spawn a new hit effect at location (x,y,z) in 'sector'.
	// The ExcludeObj field is used to avoid effecting a specific object during wakeup or explosions.
	void spawnHitEffect(HitEffectID hitEffectId, RSector* sector, vec3_fixed pos, SecObject* excludeObj);
	void hitEffectExplodeFunc(SecObject* obj);
}  // namespace TFE_DarkForces