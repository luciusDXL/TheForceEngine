#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Projectile - 
// The projectile Logic handles the creation, update and effect of
// projectiles in the world. The same system is used by both player
// and enemy projectiles.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Level/rsector.h>
#include <TFE_Level/robject.h>
#include "logic.h"
#include "hitEffect.h"
#include "time.h"

namespace TFE_DarkForces
{
	enum ProjectileType
	{
		PROJ_PUNCH = 0,
		PROJ_PISTOL_BOLT,
		PROJ_RIFLE_BOLT,
		PROJ_THERMAL_DET,
		PROJ_REPEATER,
		PROJ_PLASMA,
		PROJ_MORTAR,
		PROJ_LAND_MINE,
		PROJ_LAND_MINE_PROX,
		PROJ_LAND_MINE_PLACED,
		PROJ_CONCUSSION,
		PROJ_CANNON,
		PROJ_MISSILE,
		PROJ_13,
		PROJ_14,
		PROJ_15,
		PROJ_16,
		PROJ_17,
		PROJ_18,
		PROJ_COUNT
	};

	enum ProjectileHitType
	{
		PHIT_SKY          = -1,
		PHIT_NONE         = 0,
		PHIT_SOLID        = 4,
		PHIT_OUT_OF_RANGE = 5,
		PHIT_WATER        = 6,
	};

	struct ProjectileLogic;
	typedef ProjectileHitType(*ProjectileFunc)(ProjectileLogic*);

	enum ProjectileFlags
	{
		PROJFLAG_CAMERA_PASS_SOUND = FLAG_BIT(0),
	};

	struct ProjectileLogic
	{
		RSector* sector;
		s32 u04;
		SecObject* obj;
		Logic** parent;
		Task* task;
		LogicFunc func;

		ProjectileType type;
		// Damage & Damage falloff.
		fixed16_16 dmg;         // Base damage
		fixed16_16 falloffAmt;  // Amount to reduce damage when falloff occurs.
		Tick nextFalloffTick;   // Next tick that falloff occurs.
		Tick dmgFalloffDelta;   // Ticks between falloff changes.
		fixed16_16 minDmg;      // Minimum damage

		s32 u30;
		vec3_fixed delta;       // Movement amount in the current timeslice in units.
		vec3_fixed vel;         // Projectile velocity in units / second.
		fixed16_16 speed;       // Projectile speed in units / second.
		vec3_fixed dir;         // Projectile facing direction.
		s32 u5c;
		s32 u60;
		SecObject* prevColObj;
		SecObject* prevObj;
		SecObject* u6c;
		u32 duration;           // How long the projectile continues to move before going out of range (and being destroyed).
		s32 u74;
		s32 u78;
		s32 u7c;
		u32 flightSndId;        // Looping sound instance played while the projectile moves.
		u32 flightSndSource;    // Source looping sound.
		u32 cameraPassSnd;      // Sound effect played when the projectile passes near the camera.
		s32 u8c;
		ProjectileFunc updateFunc;// Projectile update function, this determines how the projectile moves.
		s32 u94;
		HitEffectID hitEffectId;  // The effect to play when the projectile hits a solid surface.
		u32 flags;
		s32 ua0;
		s32 ua4;
	};

	Logic* createProjectile(ProjectileType type, RSector* sector, fixed16_16 x, fixed16_16 y, fixed16_16 z, SecObject* obj);
}  // namespace TFE_DarkForces