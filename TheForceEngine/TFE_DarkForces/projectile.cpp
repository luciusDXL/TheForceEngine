#include "projectile.h"
#include "animLogic.h"
#include <TFE_Collision/collision.h>
#include <TFE_Level/robject.h>
#include <TFE_Memory/allocator.h>

using namespace TFE_Memory;
using namespace TFE_Level;

namespace TFE_DarkForces
{
	static const Tick c_mortarGravityAccel = FIXED(120);	// 120.0 units / s^2

	static Allocator* s_projectiles;
	static Task* s_projectileTask;

	static JediModel* s_boltModel;
	static Wax*       s_plasmaProj;
	static WaxFrame*  s_repeaterProj;
	static Wax*       s_mortarProj;
	static Wax*       s_missileProj;
	static Wax*       s_cannonProj;
	static WaxFrame*  s_landmineFrame;

	static u32 s_stdProjCameraSnd;
	static u32 s_plasmaCameraSnd;
	static u32 s_missileLoopingSnd;

	void projectileLogicFunc();
	ProjectileHitType proj_handleMovement(ProjectileLogic* logic);
	u32 proj_move(ProjectileLogic* logic);
	u32 proj_getHitObj(ProjectileLogic* logic);

	ProjectileHitType stdProjectileUpdateFunc(ProjectileLogic* logic);
	ProjectileHitType landMineUpdateFunc(ProjectileLogic* logic);
	ProjectileHitType mortarUpdateFunc(ProjectileLogic* logic);

	Logic* createProjectile(ProjectileType type, RSector* sector, fixed16_16 x, fixed16_16 y, fixed16_16 z, SecObject* obj)
	{
		ProjectileLogic* projLogic = (ProjectileLogic*)allocator_newItem(s_projectiles);
		SecObject* projObj = allocateObject();

		projObj->entityFlags |= ETFLAG_PROXIMITY;
		projObj->flags &= ~OBJ_FLAG_HAS_COLLISION;
		projObj->projectileLogic = projLogic;

		projLogic->prevColObj = nullptr;
		projLogic->u6c = nullptr;
		projLogic->u7c = 0;
		projLogic->cameraPassSnd = 0;
		projLogic->flightSndSource = 0;
		projLogic->flightSndId = 0;
		projLogic->u8c = 0;
		projLogic->flags = PROJFLAG_CAMERA_PASS_SOUND;
		// 'obj' gets deleted after this function is called so this is likely a bug.
		projLogic->prevObj = obj;
		projLogic->vel.x = 0;
		projLogic->vel.y = 0;
		projLogic->vel.z = 0;
		projLogic->minDmg = 0;

		obj_addLogic(projObj, (Logic*)projLogic, s_projectileTask, projectileLogicFunc);
		
		switch (type)
		{
			case PROJ_PUNCH:
			{
				setupObj_Spirit(projObj);

				projLogic->type = PROJ_PUNCH;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = FIXED(6);
				projLogic->falloffAmt = 0;
				projLogic->u30 = 1310;	// ~9
				projLogic->speed = FIXED(230);
				projLogic->u5c = 0;
				projLogic->u60 = 0;
				projLogic->u78 = 0;
				projLogic->u94 = -1;
				projLogic->hitEffectId = HEFFECT_NONE;
				projLogic->duration = s_curTick;
			} break;
			case PROJ_PISTOL_BOLT:
			{
				if (s_boltModel)
				{
					obj3d_setData(projObj, s_boltModel);
				}
				projObj->flags |= OBJ_FLAG_FULLBRIGHT;
				projObj->worldWidth = 0;

				projLogic->type = PROJ_PISTOL_BOLT;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = FIXED(10);
				projLogic->minDmg = ONE_16;
				projLogic->falloffAmt = ONE_16;
				projLogic->dmgFalloffDelta = 14;

				projLogic->u30 = 655;	// ~4.5 seconds
				projLogic->speed = FIXED(250);
				projLogic->u5c = ONE_16;
				projLogic->u60 = ONE_16;
				projLogic->u78 = 3;
				projLogic->u7c = 9;
				projLogic->nextFalloffTick = s_curTick + 14;	// ~0.1 seconds
				projLogic->u94 = 0;
				projLogic->hitEffectId = HEFFECT_SMALL_EXP;
				projLogic->duration = s_curTick + 436;	// ~3 seconds
				projLogic->cameraPassSnd = s_stdProjCameraSnd;
			} break;
			case PROJ_RIFLE_BOLT:
			{
				if (s_boltModel)
				{
					obj3d_setData(projObj, s_boltModel);
				}
				projObj->flags |= OBJ_FLAG_FULLBRIGHT;
				projObj->worldWidth = 0;

				projLogic->type = PROJ_RIFLE_BOLT;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = FIXED(10);
				projLogic->minDmg = ONE_16;
				projLogic->falloffAmt = 52428;	// 0.8 damage
				projLogic->dmgFalloffDelta = 14;

				projLogic->u30 = 655;	// ~4.5 seconds?
				projLogic->speed = FIXED(250);
				projLogic->u5c = ONE_16;
				projLogic->u60 = ONE_16;
				projLogic->u78 = 3;
				projLogic->u7c = 9;
				projLogic->nextFalloffTick = s_curTick + 14;	// ~0.1 seconds
				projLogic->u94 = 0;
				projLogic->hitEffectId = HEFFECT_SMALL_EXP;
				projLogic->duration = s_curTick + 582;	// ~4 seconds
				projLogic->cameraPassSnd = s_stdProjCameraSnd;
			} break;
			case PROJ_THERMAL_DET:
			{
				// TODO
			} break;
			case PROJ_REPEATER:
			{
				if (s_repeaterProj)
				{
					frame_setData(obj, s_repeaterProj);
				}
				projObj->flags |= OBJ_FLAG_FULLBRIGHT;
				projObj->worldWidth = 0;

				projLogic->type = PROJ_REPEATER;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = FIXED(10);
				projLogic->minDmg = ONE_16;
				projLogic->falloffAmt = 19660;	// 0.3 damage
				projLogic->dmgFalloffDelta = 19660;	// 135 seconds, this probably should have been 0.3 seconds, or 43

				projLogic->u30 = 1966;	// ~13.5 seconds?
				projLogic->speed = FIXED(270);
				projLogic->u5c = 0;
				projLogic->u60 = 0;
				projLogic->u78 = 3;
				projLogic->u7c = 9;
				projLogic->nextFalloffTick = s_curTick + 19660;	// ~135 seconds, should have been ~0.3 seconds.
				projLogic->u94 = -1;
				projLogic->hitEffectId = HEFFECT_REPEATER_EXP;
				projLogic->duration = s_curTick + 582;	// ~4 seconds
				projLogic->cameraPassSnd = s_stdProjCameraSnd;
			} break;
			case PROJ_PLASMA:
			{
				if (s_plasmaProj)
				{
					sprite_setData(obj, s_plasmaProj);
				}
				projObj->flags |= OBJ_FLAG_FULLBRIGHT;
				projObj->worldWidth = 0;
				// Setup the looping wax animation.
				obj_setSpriteAnim(projObj);

				projLogic->type = PROJ_PLASMA;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = FIXED(15);
				projLogic->minDmg = ONE_16;
				projLogic->falloffAmt = 39321;		// 0.6 damage
				projLogic->dmgFalloffDelta = 29;	// 0.2 seconds

				projLogic->u30 = 3276;	// ~22.6 seconds?
				projLogic->speed = FIXED(100);
				projLogic->u5c = 58982;	// 0.9
				projLogic->u60 = 58982;
				projLogic->u78 = 3;
				projLogic->u7c = 9;
				projLogic->nextFalloffTick = s_curTick + 29;	// ~0.2 second
				projLogic->u94 = -1;
				projLogic->hitEffectId = HEFFECT_PLASMA_EXP;
				projLogic->duration = s_curTick + 1456;	// ~10 seconds
				projLogic->cameraPassSnd = s_plasmaCameraSnd;
			} break;
			case PROJ_MORTAR:
			{
				if (s_mortarProj)
				{
					sprite_setData(obj, s_mortarProj);
				}
				// Setup the looping wax animation.
				obj_setSpriteAnim(projObj);

				projLogic->type = PROJ_MORTAR;
				projLogic->updateFunc = mortarUpdateFunc;
				projLogic->dmg = 0;	// Damage is set to 0 for some reason.
				projLogic->falloffAmt = 0;		// No falloff
				projLogic->dmgFalloffDelta = 0;

				projLogic->u30 = 13107;	// 0.2
				projLogic->speed = FIXED(110);
				projLogic->u5c = 26214;	// 0.4
				projLogic->u60 = 39321;	// 0.6
				projLogic->u78 = 0;
				projLogic->u94 = -1;
				projLogic->duration = s_curTick + 582;	// ~4 seconds -> ~440 units
			} break;
			case PROJ_LAND_MINE:
			{
				// TODO
			} break;
			case PROJ_LAND_MINE_PROX:
			{
				// TODO
			} break;
			case PROJ_LAND_MINE_PLACED:
			{
				if (s_landmineFrame)
				{
					frame_setData(projObj, s_landmineFrame);
				}
				projLogic->type = PROJ_LAND_MINE_PLACED;
				projObj->flags |= OBJ_FLAG_HAS_COLLISION;
				projObj->worldWidth = 0;

				projLogic->updateFunc = landMineUpdateFunc;
				projLogic->dmg = 0;
				projLogic->falloffAmt = 0;
				projLogic->u30 = 13107;
				projLogic->speed = 0;
				projLogic->u5c = 0;
				projLogic->u60 = 0;
				projLogic->u78 = -1;
				projLogic->duration = 0;
				projLogic->u94 = -1;
				projLogic->flags |= 2;
				projLogic->hitEffectId = HEFFECT_LARGE_EXP;
			} break;
			case PROJ_CONCUSSION:
			{
				// TODO
			} break;
			case PROJ_CANNON:
			{
				if (s_cannonProj)
				{
					sprite_setData(obj, s_cannonProj);
				}
				projObj->flags |= OBJ_FLAG_FULLBRIGHT;
				projObj->worldWidth = 0;
				// Setup the looping wax animation.
				obj_setSpriteAnim(projObj);

				projLogic->type = PROJ_CANNON;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = FIXED(30);
				projLogic->falloffAmt = 0;		// No falloff
				projLogic->dmgFalloffDelta = 0;

				projLogic->u30 = 4032;	// ~27.8 seconds?
				projLogic->speed = FIXED(100);
				projLogic->u5c = 58982;	// 0.9
				projLogic->u60 = 58982;
				projLogic->u78 = 3;
				projLogic->u7c = 18;
				projLogic->u94 = -1;
				projLogic->hitEffectId = HEFFECT_CANNON_EXP;
				projLogic->duration = s_curTick + 582;	// ~4 seconds -> ~400 units
				projLogic->cameraPassSnd = s_plasmaCameraSnd;
			} break;
			case PROJ_MISSILE:
			{
				if (s_missileProj)
				{
					sprite_setData(obj, s_missileProj);
				}
				// Setup the looping wax animation.
				obj_setSpriteAnim(projObj);

				projLogic->type = PROJ_MISSILE;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = 0;	// Damage is set to 0 for some reason.
				projLogic->falloffAmt = 0;		// No falloff
				projLogic->dmgFalloffDelta = 0;

				projLogic->u30 = ONE_16;
				projLogic->speed = FIXED(74);
				projLogic->u5c = 58982;	// 0.9
				projLogic->u60 = 58982;
				projLogic->u78 = 0;
				projLogic->u94 = -1;
				projLogic->hitEffectId = HEFFECT_MISSILE_EXP;
				projLogic->duration = s_curTick + 1019;	// ~7 seconds -> ~518 units
				projLogic->flightSndSource = s_missileLoopingSnd;
			} break;
			case PROJ_13:
			{
				// TODO
			} break;
			case PROJ_14:
			{
				// TODO
			} break;
			case PROJ_15:
			{
				// TODO
			} break;
			case PROJ_16:
			{
				// TODO
			} break;
			case PROJ_17:
			{
				// TODO
			} break;
			case PROJ_18:
			{
				// TODO
			} break;
		}

		projObj->posWS.x = x;
		projObj->posWS.y = y;
		projObj->posWS.z = z;
		sector_addObject(sector, projObj);

		return (Logic*)projLogic;
	}

	void projectileLogicFunc()
	{
		// TODO
	}
		
	// The "standard" update function - projectiles travel in a straight line with a fixed velocity.
	ProjectileHitType stdProjectileUpdateFunc(ProjectileLogic* logic)
	{
		// Calculate how much the bolt moves this timeslice.
		fixed16_16 dt = s_deltaTime;
		logic->delta.x = mul16(logic->vel.x, dt);
		logic->delta.y = mul16(logic->vel.y, dt);
		logic->delta.z = mul16(logic->vel.z, dt);

		return proj_handleMovement(logic);
	}

	// Landmines do not move at all (well normally...)
	ProjectileHitType landMineUpdateFunc(ProjectileLogic* logic)
	{
		// TODO
		return PHIT_NONE;
	}
		
	// Mortars fly in an arc.
	ProjectileHitType mortarUpdateFunc(ProjectileLogic* logic)
	{
		const fixed16_16 dt = s_deltaTime;
		// The projectile arcs due to gravity, accel = 120.0 units / s^2
		logic->vel.y += mul16(c_mortarGravityAccel, dt);	// edx

		logic->delta.x = mul16(logic->vel.x, dt);
		logic->delta.y = mul16(logic->vel.y, dt);
		logic->delta.z = mul16(logic->vel.z, dt);

		// Note this gives a skewed Y direction. It should really be: velY / vec3Length(velX, velY, velZ)
		// It also means the projectile cannot travel straight up or down.
		fixed16_16 horzSpeed = vec2Length(logic->vel.x, logic->vel.z);
		if (horzSpeed)
		{
			logic->dir.y = div16(logic->vel.y, horzSpeed);
		}
		else
		{
			logic->dir.y = 0;
		}

		return proj_handleMovement(logic);
	}
		
	// Handle projectile movement.
	// Returns 0 if it moved without hitting anything.
	ProjectileHitType proj_handleMovement(ProjectileLogic* logic)
	{
		// TODO
		return PHIT_NONE;
	}

	u32 proj_move(ProjectileLogic* logic)
	{
		// TODO
		return 0;
	}

	u32 proj_getHitObj(ProjectileLogic* logic)
	{
		// TODO
		return 0;
	}

}  // TFE_DarkForces