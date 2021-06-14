#include "projectile.h"
#include "animLogic.h"
#include "random.h"
#include "player.h"
#include <TFE_Collision/collision.h>
#include <TFE_InfSystem/infSystem.h>
#include <TFE_Level/robject.h>
#include <TFE_Level/rwall.h>
#include <TFE_Memory/allocator.h>

using namespace TFE_InfSystem;
using namespace TFE_JediSound;
using namespace TFE_Level;
using namespace TFE_Memory;
using namespace TFE_Collision;

namespace TFE_DarkForces
{
	//////////////////////////////////////////////////////////////
	// Structures and Constants
	//////////////////////////////////////////////////////////////
	enum ProjectileConstants
	{
		PROJ_GRAVITY_ACCEL         = FIXED(120), // Gravity acceleration = 120.0 units / s^2
		PL_LANDMINE_TRIGGER_RADIUS = FIXED(20),	 // Pre-placed landmine trigger radius.
		WP_LANDMINE_TRIGGER_RADIUS = FIXED(15),	 // Player Weapon landmine trigger radius.
		CAMERA_SOUND_RANGE         = FIXED(15),  // At this distance, the projectile plays its "near camera" sound.
	};

	//////////////////////////////////////////////////////////////
	// Internal State
	//////////////////////////////////////////////////////////////
	static Allocator* s_projectiles;
	static Task* s_projectileTask;

	static JediModel* s_boltModel;
	static JediModel* s_greenBoltModel;
	static Wax*       s_plasmaProj;
	static WaxFrame*  s_repeaterProj;
	static Wax*       s_mortarProj;
	static Wax*       s_missileProj;
	static Wax*       s_cannonProj;
	static Wax*       s_probeProj;
	static WaxFrame*  s_landmineWpnFrame;
	static WaxFrame*  s_landmineFrame;
	static WaxFrame*  s_thermalDetProj;

	static SoundSourceID s_stdProjCameraSnd     = NULL_SOUND;
	static SoundSourceID s_stdProjReflectSnd    = NULL_SOUND;
	static SoundSourceID s_thermalDetReflectSnd = NULL_SOUND;
	static SoundSourceID s_plasmaCameraSnd      = NULL_SOUND;
	static SoundSourceID s_plasmaReflectSnd     = NULL_SOUND;
	static SoundSourceID s_missileLoopingSnd    = NULL_SOUND;
	static SoundSourceID s_landMineTriggerSnd   = NULL_SOUND;

	static RSector*   s_projStartSector[16];
	static RSector*   s_projPath[16];
	static s32        s_projIter;
	static fixed16_16 s_projNextPosX;
	static fixed16_16 s_projNextPosY;
	static fixed16_16 s_projNextPosZ;
	static JBool      s_hitWater;
	static RWall*     s_hitWall;
	static u32        s_hitWallFlag;
	static RSector*   s_projSector;

	// Projectile specific collisions
	static RSector*   s_colObjSector;
	static SecObject* s_colHitObj;
	static fixed16_16 s_colObjAdjX;
	static fixed16_16 s_colObjAdjY;
	static fixed16_16 s_colObjAdjZ;

	//////////////////////////////////////////////////////////////
	// Forward Declarations
	//////////////////////////////////////////////////////////////
	void projectileLogicFunc();
	ProjectileHitType proj_handleMovement(ProjectileLogic* logic);
	JBool proj_move(ProjectileLogic* logic);
	JBool proj_getHitObj(ProjectileLogic* logic);
	JBool handleProjectileHit(ProjectileLogic* logic, ProjectileHitType hitType);

	ProjectileHitType stdProjectileUpdateFunc(ProjectileLogic* logic);
	ProjectileHitType landMineUpdateFunc(ProjectileLogic* logic);
	ProjectileHitType arcingProjectileUpdateFunc(ProjectileLogic* logic);

	//////////////////////////////////////////////////////////////
	// API Implementation
	//////////////////////////////////////////////////////////////
	Logic* createProjectile(ProjectileType type, RSector* sector, fixed16_16 x, fixed16_16 y, fixed16_16 z, SecObject* obj)
	{
		ProjectileLogic* projLogic = (ProjectileLogic*)allocator_newItem(s_projectiles);
		SecObject* projObj = allocateObject();

		projObj->entityFlags |= ETFLAG_PROJECTILE;
		projObj->flags &= ~OBJ_FLAG_HAS_COLLISION;
		projObj->projectileLogic = projLogic;

		projLogic->prevColObj = nullptr;
		projLogic->excludeObj = nullptr;
		projLogic->reflVariation   = 0;
		projLogic->cameraPassSnd   = NULL_SOUND;
		projLogic->flightSndSource = NULL_SOUND;
		projLogic->flightSndId     = NULL_SOUND;
		projLogic->reflectSnd      = NULL_SOUND;
		projLogic->flags = PROJFLAG_CAMERA_PASS_SOUND;
		projLogic->prevObj = obj;  // 'obj' gets deleted after this function is called so this is likely a bug.
		projLogic->vel.x   = 0;
		projLogic->vel.y   = 0;
		projLogic->vel.z   = 0;
		projLogic->minDmg  = 0;

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
				projLogic->horzBounciness = 0;
				projLogic->vertBounciness = 0;
				projLogic->bounceCnt = 0;
				projLogic->reflectEffectId = HEFFECT_NONE;
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
				projLogic->horzBounciness = ONE_16;
				projLogic->vertBounciness = ONE_16;
				projLogic->bounceCnt = 3;
				projLogic->reflVariation = 9;
				projLogic->nextFalloffTick = s_curTick + 14;	// ~0.1 seconds
				projLogic->reflectEffectId = HEFFECT_SMALL_EXP;
				projLogic->hitEffectId = HEFFECT_SMALL_EXP;
				projLogic->duration = s_curTick + 436;	// ~3 seconds
				projLogic->cameraPassSnd = s_stdProjCameraSnd;
				projLogic->reflectSnd = s_stdProjReflectSnd;
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
				projLogic->horzBounciness = ONE_16;
				projLogic->vertBounciness = ONE_16;
				projLogic->bounceCnt = 3;
				projLogic->reflVariation = 9;
				projLogic->nextFalloffTick = s_curTick + 14;	// ~0.1 seconds
				projLogic->reflectEffectId = HEFFECT_SMALL_EXP;
				projLogic->hitEffectId = HEFFECT_SMALL_EXP;
				projLogic->duration = s_curTick + 582;	// ~4 seconds
				projLogic->cameraPassSnd = s_stdProjCameraSnd;
				projLogic->reflectSnd = s_stdProjReflectSnd;
			} break;
			case PROJ_THERMAL_DET:
			{
				if (s_thermalDetProj)
				{
					frame_setData(projObj, s_thermalDetProj);
				}
				projObj->flags |= OBJ_FLAG_HAS_COLLISION;

				projLogic->flags |= PROJFLAG_EXPLODE;
				projLogic->type = PROJ_THERMAL_DET;
				projLogic->updateFunc = arcingProjectileUpdateFunc;
				projLogic->dmg = 0;
				projLogic->minDmg = ONE_16;
				projLogic->falloffAmt = 0;
				projLogic->dmgFalloffDelta = 0;

				projLogic->u30 = 6553;	// ~45.2 seconds?
				projLogic->speed = FIXED(80);
				projLogic->horzBounciness = 29491;	// 0.45
				projLogic->vertBounciness = 58327;  // 0.89
				projLogic->bounceCnt = -1;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->hitEffectId = HEFFECT_THERMDET_EXP;
				projLogic->duration = s_curTick + 436;	// ~3 seconds
				projLogic->cameraPassSnd = s_stdProjCameraSnd;
				projLogic->reflectSnd = s_thermalDetReflectSnd;
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
				projLogic->horzBounciness = 0;
				projLogic->vertBounciness = 0;
				projLogic->bounceCnt = 3;
				projLogic->reflVariation = 9;
				projLogic->nextFalloffTick = s_curTick + 19660;	// ~135 seconds, should have been ~0.3 seconds.
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->hitEffectId = HEFFECT_REPEATER_EXP;
				projLogic->duration = s_curTick + 582;	// ~4 seconds
				projLogic->cameraPassSnd = s_stdProjCameraSnd;
				projLogic->reflectSnd = s_stdProjReflectSnd;
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
				projLogic->horzBounciness = 58982;	// 0.9
				projLogic->vertBounciness = 58982;
				projLogic->bounceCnt = 3;
				projLogic->reflVariation = 9;
				projLogic->nextFalloffTick = s_curTick + 29;	// ~0.2 second
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->hitEffectId = HEFFECT_PLASMA_EXP;
				projLogic->duration = s_curTick + 1456;	// ~10 seconds
				projLogic->cameraPassSnd = s_plasmaCameraSnd;
				projLogic->reflectSnd = s_plasmaReflectSnd;
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
				projLogic->updateFunc = arcingProjectileUpdateFunc;
				projLogic->dmg = 0;	// Damage is set to 0 for some reason.
				projLogic->falloffAmt = 0;		// No falloff
				projLogic->dmgFalloffDelta = 0;

				projLogic->u30 = 13107;	// 0.2
				projLogic->speed = FIXED(110);
				projLogic->horzBounciness = 26214;	// 0.4
				projLogic->vertBounciness = 39321;	// 0.6
				projLogic->bounceCnt = 0;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->duration = s_curTick + 582;	// ~4 seconds -> ~440 units
			} break;
			case PROJ_LAND_MINE:
			{
				if (s_landmineWpnFrame)
				{
					frame_setData(obj, s_landmineWpnFrame);
				}
				projObj->entityFlags |= ETFLAG_LANDMINE_WPN;
				projObj->flags |= OBJ_FLAG_HAS_COLLISION;

				projLogic->type = PROJ_LAND_MINE;
				projLogic->updateFunc = arcingProjectileUpdateFunc;
				projLogic->dmg = 0;
				projLogic->falloffAmt = 0;
				projLogic->dmgFalloffDelta = 0;

				projLogic->u30 = 13107;
				projLogic->speed = 0;
				projLogic->horzBounciness = 0;
				projLogic->vertBounciness = 0;
				projLogic->bounceCnt = -1;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->flags |= PROJFLAG_EXPLODE;
				projLogic->hitEffectId = HEFFECT_LARGE_EXP;
				projLogic->duration = s_curTick + 436;	// ~3 seconds
			} break;
			case PROJ_LAND_MINE_PROX:
			{
				if (s_landmineWpnFrame)
				{
					frame_setData(obj, s_landmineWpnFrame);
				}
				projObj->entityFlags |= ETFLAG_LANDMINE_WPN;
				projObj->flags |= OBJ_FLAG_HAS_COLLISION;

				projLogic->type = PROJ_LAND_MINE_PROX;
				projLogic->updateFunc = arcingProjectileUpdateFunc;
				projLogic->dmg = 0;
				projLogic->falloffAmt = 0;
				projLogic->dmgFalloffDelta = 0;

				projLogic->u30 = 13107;
				projLogic->speed = 0;
				projLogic->horzBounciness = 0;
				projLogic->vertBounciness = 0;
				projLogic->bounceCnt = -1;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->flags |= PROJFLAG_EXPLODE;
				projLogic->hitEffectId = HEFFECT_LARGE_EXP;
				projLogic->duration = s_curTick + 436;	// ~3 seconds
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
				projLogic->horzBounciness = 0;
				projLogic->vertBounciness = 0;
				projLogic->bounceCnt = -1;
				projLogic->duration = 0;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->flags |= PROJFLAG_EXPLODE;
				projLogic->hitEffectId = HEFFECT_LARGE_EXP;
			} break;
			case PROJ_CONCUSSION:
			{
				setupObj_Spirit(projObj);
				projLogic->type = PROJ_CONCUSSION;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = 0;
				projLogic->falloffAmt = 0;
				projLogic->dmgFalloffDelta = 0;

				projLogic->u30 = 3932;	// ~0.6 seconds?
				projLogic->speed = FIXED(190);
				projLogic->horzBounciness = 58982;	// 0.9
				projLogic->vertBounciness = 58982;
				projLogic->bounceCnt = 0;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->hitEffectId = HEFFECT_CONCUSSION;
				projLogic->duration = s_curTick + 728;	// ~5 seconds
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
				projLogic->horzBounciness = 58982;	// 0.9
				projLogic->vertBounciness = 58982;
				projLogic->bounceCnt = 3;
				projLogic->reflVariation = 18;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->hitEffectId = HEFFECT_CANNON_EXP;
				projLogic->duration = s_curTick + 582;	// ~4 seconds -> ~400 units
				projLogic->cameraPassSnd = s_plasmaCameraSnd;
				projLogic->reflectSnd = s_plasmaReflectSnd;
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
				projLogic->horzBounciness = 58982;	// 0.9
				projLogic->vertBounciness = 58982;
				projLogic->bounceCnt = 0;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->hitEffectId = HEFFECT_MISSILE_EXP;
				projLogic->duration = s_curTick + 1019;	// ~7 seconds -> ~518 units
				projLogic->flightSndSource = s_missileLoopingSnd;
			} break;
			case PROJ_TURRET_BOLT:
			{
				if (s_greenBoltModel)
				{
					obj3d_setData(projObj, s_greenBoltModel);
				}
				projObj->flags |= OBJ_FLAG_FULLBRIGHT;
				projObj->worldWidth = 0;

				projLogic->type = PROJ_TURRET_BOLT;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = FIXED(17);
				projLogic->minDmg = ONE_16;
				projLogic->falloffAmt = ONE_16;
				projLogic->dmgFalloffDelta = 14;

				projLogic->u30 = 1966;	// ~13.55 seconds?
				projLogic->speed = FIXED(300);
				projLogic->horzBounciness = ONE_16;
				projLogic->vertBounciness = ONE_16;
				projLogic->bounceCnt = 3;
				projLogic->reflVariation = 9;
				projLogic->nextFalloffTick = s_curTick + 14;	// ~0.1 seconds
				projLogic->reflectEffectId = HEFFECT_SMALL_EXP;
				projLogic->hitEffectId = HEFFECT_SMALL_EXP;
				projLogic->duration = s_curTick + 436;	// ~3 seconds
				projLogic->cameraPassSnd = s_stdProjCameraSnd;
				projLogic->reflectSnd = s_stdProjReflectSnd;
			} break;
			case PROJ_REMOTE_BOLT:
			{
				if (s_greenBoltModel)
				{
					obj3d_setData(projObj, s_greenBoltModel);
				}
				projObj->flags |= OBJ_FLAG_FULLBRIGHT;
				projObj->worldWidth = 0;

				projLogic->type = PROJ_REMOTE_BOLT;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = ONE_16;
				projLogic->minDmg = 0;
				projLogic->falloffAmt = ONE_16;
				projLogic->dmgFalloffDelta = 14;

				projLogic->u30 = 655;	// ~4.5 seconds?
				projLogic->speed = FIXED(300);
				projLogic->horzBounciness = ONE_16;
				projLogic->vertBounciness = ONE_16;
				projLogic->bounceCnt = 3;
				projLogic->reflVariation = 9;
				projLogic->nextFalloffTick = s_curTick + 14;	// ~0.1 seconds
				projLogic->reflectEffectId = HEFFECT_SMALL_EXP;
				projLogic->hitEffectId = HEFFECT_SMALL_EXP;
				projLogic->duration = s_curTick + 436;	// ~3 seconds
				projLogic->cameraPassSnd = s_stdProjCameraSnd;
				projLogic->reflectSnd = s_stdProjReflectSnd;
			} break;
			case PROJ_EXP_BARREL:
			{
				setupObj_Spirit(projObj);

				projLogic->type = PROJ_EXP_BARREL;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = 0;
				projLogic->falloffAmt = 0;
				projLogic->dmgFalloffDelta = 0;

				projLogic->u30 = 0;
				projLogic->speed = 0;
				projLogic->horzBounciness = 0;
				projLogic->vertBounciness = 0;
				projLogic->bounceCnt = 0;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->flags |= PROJFLAG_EXPLODE;
				projLogic->hitEffectId = HEFFECT_EXP_BARREL;
				projLogic->duration = s_curTick;
			} break;
			case PROJ_16:
			{
				// TODO
			} break;
			case PROJ_PROBE_PROJ:
			{
				if (s_probeProj)
				{
					sprite_setData(obj, s_probeProj);
				}
				// 1f3398:
				projObj->flags |= OBJ_FLAG_FULLBRIGHT;
				projObj->worldWidth = 0;
				// Setup the looping wax animation.
				obj_setSpriteAnim(projObj);

				projLogic->type = PROJ_PROBE_PROJ;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = FIXED(10);
				projLogic->minDmg = ONE_16;
				projLogic->falloffAmt = ONE_16;		// 1.0 damage
				projLogic->dmgFalloffDelta = 14;

				projLogic->u30 = 2621;	// ~18.08 seconds?
				projLogic->speed = FIXED(100);
				projLogic->horzBounciness = 58982;	// 0.9
				projLogic->vertBounciness = 58982;
				projLogic->bounceCnt = 3;
				projLogic->reflVariation = 9;
				projLogic->nextFalloffTick = s_curTick + 14;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->hitEffectId = HEFFECT_PLASMA_EXP;
				projLogic->duration = s_curTick + 1165;	// ~8 seconds
				projLogic->cameraPassSnd = s_plasmaCameraSnd;
				projLogic->reflectSnd = s_plasmaReflectSnd;
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
		ProjectileLogic* logic = (ProjectileLogic*)allocator_getHead(s_projectiles);
		while (logic)
		{
			SecObject* obj = logic->obj;
			ProjectileHitType projHitType = PHIT_NONE;
			Tick curTick = s_curTick;

			// The projectile is still active.
			if (curTick < logic->duration)
			{
				// Handle damage falloff.
				if (logic->falloffAmt && curTick > logic->nextFalloffTick)
				{
					logic->dmg -= logic->falloffAmt;
					if (logic->dmg < logic->minDmg)
					{
						logic->nextFalloffTick = 0xffffffff;	// This basically sets the falloff to "infinity" so it never enters this block again.
						logic->dmg = logic->minDmg;
					}
					else
					{
						logic->nextFalloffTick += logic->dmgFalloffDelta;
					}
				}
			}
			else  // The projectile has reached the end of its life.
			{
				const ProjectileType type = logic->type;
				// Note that proximity landmines that have reached end of life don't explode right away, but instead check for valid object proximity.
				if (type == PROJ_LAND_MINE_PLACED)	// Pre-placed landmines.
				{
					const fixed16_16 approxDist = distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, obj->posWS.x, obj->posWS.z);
					if (approxDist <= PL_LANDMINE_TRIGGER_RADIUS)
					{
						if (s_playerObject->posWS.y >= obj->posWS.y - PL_LANDMINE_TRIGGER_RADIUS && s_playerObject->posWS.y <= obj->posWS.y + PL_LANDMINE_TRIGGER_RADIUS &&
							collision_isAnyObjectInRange(obj->sector, PL_LANDMINE_TRIGGER_RADIUS, obj->posWS, obj, ETFLAG_PLAYER))
						{
							logic->type = PROJ_LAND_MINE;
							logic->duration = s_curTick + 87;  // The landmine will explode in 0.6 seconds.
							playSound3D_oneshot(s_landMineTriggerSnd, obj->posWS);
						}
					}
					else
					{
						// subtracts ~dist/64 ticks from the current time.
						// which ensures it will check here again next tick, but it was going to do that anyway...
						logic->duration = s_curTick - floor16(mul16(approxDist / 64, 9545318));	// 9545318 = 145.65
					}
				}
				else if (type == PROJ_LAND_MINE_PROX)	// Player placed proximity landmine (secondary fire).
				{
					if (collision_isAnyObjectInRange(obj->sector, WP_LANDMINE_TRIGGER_RADIUS, obj->posWS, obj, ETFLAG_PLAYER | ETFLAG_AI_ACTOR))
					{
						logic->type = PROJ_LAND_MINE;
						logic->duration = s_curTick + TICKS_PER_SECOND;	// The landmine will explode in 1 second.
						playSound3D_oneshot(s_landMineTriggerSnd, obj->posWS);
					}
				}
				else
				{
					projHitType = PHIT_OUT_OF_RANGE;
				}
			}

			if (projHitType == PHIT_NONE)
			{
				RSector* sector = obj->sector;
				InfLink* link = (InfLink*)allocator_getHead(sector->infLink);
				LinkType linkType = link->type;
				if (link && linkType == LTYPE_SECTOR && inf_isOnMovingFloor(obj, link->elev, sector))
				{
					fixed16_16 res;
					vec3_fixed vel;
					inf_getMovingElevatorVelocity(link->elev, &vel, &res);
					logic->vel.y = vel.y;
					logic->vel.x += vel.x;
					logic->vel.z += vel.z;
				}
				if (logic->updateFunc)
				{
					projHitType = logic->updateFunc(logic);
				}
			}

			// Play a looping sound as the projectile travels, this updates its position if already playing.
			if (logic->flightSndSource)
			{
				logic->flightSndId = playSound3D_looping(logic->flightSndSource, logic->flightSndId, obj->posWS);
			}
			// Play a sound as the object passes near the camera.
			if (logic->cameraPassSnd && (logic->flags & PROJFLAG_CAMERA_PASS_SOUND))
			{
				fixed16_16 dy = TFE_CoreMath::abs(obj->posWS.y - s_eyePos.y);
				fixed16_16 approxDist = dy + distApprox(s_eyePos.x, s_eyePos.z, obj->posWS.x, obj->posWS.z);
				if (approxDist < CAMERA_SOUND_RANGE)
				{
					playSound3D_oneshot(logic->cameraPassSnd, obj->posWS);
					logic->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
				}
			}
			else if (logic->cameraPassSnd && logic->flightSndId)
			{
				stopSound(logic->flightSndId);
				logic->flightSndId = NULL_SOUND;
			}
			// Finally handle the hit itself (this includes going out of range).
			if (projHitType != PHIT_NONE)
			{
				handleProjectileHit(logic, projHitType);
			}

			logic = (ProjectileLogic*)allocator_getNext(s_projectiles);
		}
	}

	void triggerLandMine(ProjectileLogic* logic, Tick delay)
	{
		logic->type = PROJ_LAND_MINE;
		logic->duration = s_curTick + delay;

		SecObject* obj = logic->obj;
		playSound3D_oneshot(s_landMineTriggerSnd, obj->posWS);
	}

	//////////////////////////////////////////////////////////////
	// Internal Implementation
	//////////////////////////////////////////////////////////////
	// The "standard" update function - projectiles travel in a straight line with a fixed velocity.
	ProjectileHitType stdProjectileUpdateFunc(ProjectileLogic* logic)
	{
		// Calculate how much the projectile moves this timeslice.
		const fixed16_16 dt = s_deltaTime;
		logic->delta.x = mul16(logic->vel.x, dt);
		logic->delta.y = mul16(logic->vel.y, dt);
		logic->delta.z = mul16(logic->vel.z, dt);

		return proj_handleMovement(logic);
	}

	// Landmines do not move but can fall if off of the ground.
	ProjectileHitType landMineUpdateFunc(ProjectileLogic* logic)
	{
		SecObject* obj = logic->obj;
		fixed16_16 ceilHeight, floorHeight;
		sector_getObjFloorAndCeilHeight(obj->sector, obj->posWS.y, &floorHeight, &ceilHeight);

		// Gravity - fall if the landmine is above the ground.
		if (obj->posWS.y < floorHeight)
		{
			const fixed16_16 dt = s_deltaTime;
			logic->vel.y  += mul16(PROJ_GRAVITY_ACCEL, dt);
			logic->delta.y = mul16(logic->vel.y, dt);
			return proj_handleMovement(logic);
		}
		return PHIT_NONE;
	}
		
	// Projectiles, such as Thermal Detonators and Mortars fly in an arc.
	// This is done by giving them an initial velocity and then modifying that velocity
	// over time based on gravity.
	ProjectileHitType arcingProjectileUpdateFunc(ProjectileLogic* logic)
	{
		const fixed16_16 dt = s_deltaTime;
		// The projectile arcs due to gravity, accel = 120.0 units / s^2
		logic->vel.y += mul16(PROJ_GRAVITY_ACCEL, dt);
		// Get the frame delta from the velocity and delta time.
		logic->delta.x = mul16(logic->vel.x, dt);
		logic->delta.y = mul16(logic->vel.y, dt);
		logic->delta.z = mul16(logic->vel.z, dt);

		// Note this gives a skewed Y direction. It should really be: velY / vec3Length(velX, velY, velZ)
		// It also means the projectile cannot travel straight up or down.
		const fixed16_16 horzSpeed = vec2Length(logic->vel.x, logic->vel.z);
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

	void proj_computeTransform3D(SecObject* obj, fixed16_16 sinPitch, fixed16_16 cosPitch, fixed16_16 sinYaw, fixed16_16 cosYaw, fixed16_16 dt)
	{
		fixed16_16* transform = obj->transform;
		transform[0] = cosYaw;
		transform[1] = mul16(sinPitch, sinYaw);
		transform[2] = mul16(mul16(cosPitch, sinYaw), dt);

		transform[3] = 0;
		transform[4] = cosPitch;
		transform[5] = -mul16(sinPitch, dt);

		transform[6] = -sinYaw;
		transform[7] = mul16(sinPitch, cosYaw);
		transform[8] = mul16(mul16(cosPitch, cosYaw), dt);
	}

	void proj_setYawPitch(ProjectileLogic* logic, fixed16_16 sinPitch, fixed16_16 cosPitch, fixed16_16 sinYaw, fixed16_16 cosYaw)
	{
		logic->dir.x = sinYaw;
		logic->dir.z = cosYaw;
		logic->vel.x = mul16(mul16(sinYaw, logic->speed), cosPitch);
		logic->vel.z = mul16(mul16(cosYaw, logic->speed), cosPitch);
		logic->vel.y = -mul16(sinPitch, logic->speed);

		logic->dir.y = 0;
		fixed16_16 horzSpeed = vec2Length(logic->vel.x, logic->vel.z);
		if (horzSpeed)
		{
			logic->dir.y = div16(logic->vel.y, horzSpeed);
		}

		SecObject* obj = logic->obj;
		if (obj->type == OBJ_TYPE_3D)
		{
			proj_computeTransform3D(obj, sinPitch, cosPitch, sinYaw, cosYaw, s_deltaTime);
		}
	}

	void proj_setTransform(ProjectileLogic* logic, angle14_32 pitch, angle14_32 yaw)
	{
		SecObject* obj = logic->obj;
		obj->roll  = 0;
		obj->pitch = pitch;
		obj->yaw   = yaw;

		fixed16_16 cosYaw, sinYaw;
		fixed16_16 cosPitch, sinPitch;
		sinCosFixed(yaw,   &sinYaw,   &cosYaw);
		sinCosFixed(pitch, &sinPitch, &cosPitch);

		proj_setYawPitch(logic, sinPitch, cosPitch, sinYaw, cosYaw);
	}

	// Originally this code was inlined in the final DOS code, but I am splitting it out to reduce redundant code.
	// This code adds some variation to the final yaw and pitch of a projectile after reflection if
	// logic->reflVariation is non-zero.
	void handleReflectVariation(ProjectileLogic* logic, SecObject* obj)
	{
		if (!logic->reflVariation) { return; }

		// Random variation between [-logic->reflVariation, logic->reflVariation]
		obj->yaw   += random(2 * logic->reflVariation) - logic->reflVariation;
		obj->pitch += random(2 * logic->reflVariation) - logic->reflVariation;
	}

	// Handle projectile movement.
	// Returns 0 if it moved without hitting anything.
	ProjectileHitType proj_handleMovement(ProjectileLogic* logic)
	{
		SecObject* obj = logic->obj;
		JBool envHit = proj_move(logic);
		JBool objHit = proj_getHitObj(logic);
		if (objHit)
		{
			obj->posWS.y = s_colObjAdjY;
			fixed16_16 dz = s_colObjAdjZ - obj->posWS.z;
			fixed16_16 dx = s_colObjAdjX - obj->posWS.x;
			RSector* newSector = collision_moveObj(obj, dx, dz);

			if (logic->bounceCnt == -1)
			{
				logic->speed = mul16(logic->speed, logic->horzBounciness);	// eax
				angle14_32 offsetYaw = obj->yaw + 4096;
				// getAngleDifference() will always return 4096, then we add yaw + 4096 = yaw + 8192 = 180 degrees.
				// This could have been accomplished with: obj->yaw = (obj->yaw + 8192) & 16383
				obj->yaw = (getAngleDifference(obj->yaw, offsetYaw) + offsetYaw) & 16383;
				handleReflectVariation(logic, obj);

				logic->prevColObj = nullptr;
				logic->excludeObj = nullptr;
				proj_setTransform(logic, obj->pitch, obj->yaw);
				playSound3D_oneshot(logic->reflectSnd, obj->posWS);

				logic->flags |= PROJFLAG_CAMERA_PASS_SOUND;
				logic->delta.x = mul16(HALF_16, logic->dir.x);
				logic->delta.z = mul16(HALF_16, logic->dir.z);
				collision_moveObj(obj, logic->delta.x, logic->delta.z);

				logic->delta.x = 0;
				logic->delta.y = 0;
				logic->delta.z = 0;

				if (!envHit)
				{
					return PHIT_NONE;
				}

				obj->posWS.y = s_projNextPosY;
				collision_moveObj(obj, s_projNextPosX - obj->posWS.x, s_projNextPosZ - obj->posWS.z);
				if (!s_hitWall)
				{
					return (s_hitWater) ? PHIT_WATER : PHIT_SOLID;
				}
				s_hitWallFlag = (s_projSector->flags1 & SEC_FLAGS1_NOWALL_DRAW) ? 1 : 2;

				vec2_fixed* w0 = s_hitWall->w0;
				fixed16_16 len = vec2Length(s_projNextPosX - w0->x, s_projNextPosZ - w0->z);
				inf_wallAndMirrorSendMessageAtPos(s_hitWall, obj, INF_EVENT_SHOOT_LINE, len, s_projNextPosY);
				return (s_hitWallFlag == 1) ? PHIT_SKY : PHIT_SOLID;
			}

			if (logic->type == PROJ_PUNCH)
			{
				vec3_fixed effectPos = { s_colObjAdjX, s_colObjAdjY, s_colObjAdjZ };
				spawnHitEffect(logic->hitEffectId, obj->sector, effectPos, logic->excludeObj);
			}
			s_hitWallFlag = 2;
			if (s_colHitObj->entityFlags & ETFLAG_PROJECTILE)
			{
				s_hitWallFlag = 0;
				Logic** objLogicPtr = (Logic**)allocator_getHead((Allocator*)s_colHitObj->logic);
				if (objLogicPtr)
				{
					ProjectileLogic* objLogic = (ProjectileLogic*)*objLogicPtr;
					// PROJ_16 can be shot which will cause it to immediately stop (explode?)
					if (logic->type != objLogic->type && objLogic->type == PROJ_16)
					{
						s_hitWallFlag = 2;
						objLogic->duration = 0;
					}
				}
			}
			if (logic->dmg)
			{
				s_infMsgEntity = logic;
				inf_sendObjMessage(s_colHitObj, IMSG_DAMAGE, nullptr);
			}

			if (s_hitWallFlag <= 1)	// s_projSector->flags1 & SEC_FLAGS1_NOWALL_DRAW
			{
				return PHIT_SKY;
			}
			else if (s_hitWallFlag <= 3 || s_hitWallFlag > 4)  // !(s_projSector->flags1 & SEC_FLAGS1_NOWALL_DRAW)
			{
				return PHIT_SOLID;
			}
			assert(s_hitWallFlag != 4);	// This shouldn't be true, but there is code in the original exe even though the flag should never be set to this value.
		}
		else if (envHit)
		{
			obj->posWS.y = s_projNextPosY;
			fixed16_16 dz = s_projNextPosZ - obj->posWS.z;
			fixed16_16 dx = s_projNextPosX - obj->posWS.x;
			RSector* newSector = collision_moveObj(obj, dx, dz);

			if (s_hitWall)
			{
				s_hitWallFlag = (s_projSector->flags1 & SEC_FLAGS1_NOWALL_DRAW) ? 1 : 2;

				vec2_fixed* w0 = s_hitWall->w0;
				fixed16_16 paramPos = vec2Length(s_projNextPosX - w0->x, s_projNextPosZ - w0->z);
				inf_wallAndMirrorSendMessageAtPos(s_hitWall, logic->obj, INF_EVENT_SHOOT_LINE, paramPos, s_projNextPosY);

				if (s_hitWallFlag <= 1)	// s_projSector->flags1 & SEC_FLAGS1_NOWALL_DRAW
				{
					return PHIT_SKY;
				}
				else if (s_hitWallFlag <= 3 || s_hitWallFlag > 4)  // !(s_projSector->flags1 & SEC_FLAGS1_NOWALL_DRAW)
				{
					return PHIT_SOLID;
				}
				assert(s_hitWallFlag != 4);	// This shouldn't be true, but there is code in the original exe even though the flag should never be set to this value.
			}
			return s_hitWater ? PHIT_WATER : PHIT_SOLID;
		}

		// Hit nothing.
		obj->posWS.y += logic->delta.y;
		collision_moveObj(obj, logic->delta.x, logic->delta.z);
		return PHIT_NONE;
	}

	// Return JTRUE if something was hit during movement.
	JBool proj_move(ProjectileLogic* logic)
	{
		SecObject* obj = logic->obj;
		RSector* sector = obj->sector;

		s_hitWall = nullptr;
		s_hitWater = JFALSE;
		s_projSector = sector;
		s_projStartSector[0] = sector;
		s_projIter = 1;
		s_projNextPosX = obj->posWS.x + logic->delta.x;
		s_projNextPosY = obj->posWS.y + logic->delta.y;
		s_projNextPosZ = obj->posWS.z + logic->delta.z;

		fixed16_16 y0CeilHeight;
		fixed16_16 y0FloorHeight;
		bool hitFloor = false, hitCeil = false;

		RWall* wall = collision_wallCollisionFromPath(s_projSector, obj->posWS.x, obj->posWS.z, s_projNextPosX, s_projNextPosZ);
		while (wall)
		{
			fixed16_16 bot, top;
			if (wall->nextSector && !(wall->flags3 & WF3_CANNOT_FIRE_THROUGH))
			{
				// Determine the opening in the wall.
				wall_getOpeningHeightRange(wall, &top, &bot);

				// The check if the projectile trivially passes through.
				fixed16_16 y0 = s_projNextPosY;
				fixed16_16 y1 = logic->type == PROJ_THERMAL_DET ? s_projNextPosY - obj->worldHeight : s_projNextPosY;
				if (obj->posWS.y <= bot && obj->posWS.y >= top && y0 <= bot && y1 >= top)
				{
					s_projIter++;
					s_projSector = wall->nextSector;
					s_projPath[s_projIter] = s_projSector;
					wall = collision_pathWallCollision(s_projSector);
					continue;
				}
			}
			else
			{
				// There is no opening in the wall.
				bot = -FIXED(100);
				top =  FIXED(100);
			}

			collision_getHitPoint(&s_projNextPosX, &s_projNextPosZ);
			fixed16_16 len = vec2Length(s_projNextPosX - obj->posWS.x, s_projNextPosZ - obj->posWS.z);
			fixed16_16 yDelta = mul16(len, logic->dir.y);

			fixed16_16 y0 = obj->posWS.y;
			fixed16_16 y1 = obj->posWS.y + yDelta;
			sector_getObjFloorAndCeilHeight(s_projSector, y0, &y0FloorHeight, &y0CeilHeight);
			// In the case of water, the real floor height is the actual floor.
			if (s_projSector->secHeight > 0)
			{
				y0FloorHeight = s_projSector->floorHeight;
			}

			fixed16_16 nextY = y1;
			if (y1 > y0FloorHeight)
			{
				nextY = y0FloorHeight;
			}
			else if (y1 < y0CeilHeight)
			{
				nextY = y0CeilHeight;
			}

			fixed16_16 nY0 = nextY;
			fixed16_16 nY1 = logic->type == PROJ_THERMAL_DET ? nextY - obj->worldHeight : nextY;
			if (nY0 <= bot && nY1 >= top)
			{
				if (y1 >= y0FloorHeight)
				{
					hitFloor = true;
					break;
				}
				if (y1 <= y0CeilHeight)
				{
					hitCeil = true;
					break;
				}
			}
			else  // Wall Hit!
			{
				fixed16_16 offset = ONE_16 + HALF_16;
				fixed16_16 offsetX = mul16(offset, logic->dir.x);
				fixed16_16 offsetZ = mul16(offset, logic->dir.z);

				s_projNextPosY = nextY;
				s_projNextPosX -= offsetX;
				s_projNextPosZ -= offsetZ;

				logic->delta.x = s_projNextPosX - obj->posWS.x;
				logic->delta.y = s_projNextPosY - obj->posWS.y;
				logic->delta.z = s_projNextPosZ - obj->posWS.z;

				if (logic->bounceCnt == -1)
				{
					if (s_projNextPosY < s_projSector->ceilingHeight)
					{
						s_hitWall = wall;
						s_hitWallFlag = 1;
						return JTRUE;
					}
					logic->speed = mul16(logic->speed, logic->horzBounciness);
					obj->pitch = (getAngleDifference(obj->pitch, 0) & 16383);
					obj->yaw = (getAngleDifference(obj->yaw, wall->angle) + wall->angle) & 16383;
					handleReflectVariation(logic, obj);

					logic->prevColObj = nullptr;
					logic->excludeObj = nullptr;
					proj_setTransform(logic, obj->pitch, obj->yaw);
					playSound3D_oneshot(logic->reflectSnd, obj->posWS);

					logic->flags |= PROJFLAG_CAMERA_PASS_SOUND;
					obj->posWS.y = s_projNextPosY;
					collision_moveObj(obj, logic->delta.x, logic->delta.z);
					s_projSector = obj->sector;

					logic->delta.x = 0;
					logic->delta.y = 0;
					logic->delta.z = 0;

					return JFALSE;
				}
				else if (s_projSector->flags1 & SEC_FLAGS1_MAG_SEAL)
				{
					if (s_projNextPosY < s_projSector->ceilingHeight)
					{
						s_hitWall = wall;
						s_hitWallFlag = 1;
						return JTRUE;
					}
					s32 count = logic->bounceCnt;
					logic->bounceCnt--;
					if (count)
					{
						vec3_fixed effectPos = { s_projNextPosX, s_projNextPosY, s_projNextPosZ };
						spawnHitEffect(logic->reflectEffectId, s_projSector, effectPos, logic->excludeObj);

						SecObject* obj = logic->obj;
						obj->yaw = (getAngleDifference(obj->yaw, wall->angle) + wall->angle) & 16383;
						handleReflectVariation(logic, obj);

						logic->prevColObj = nullptr;
						logic->excludeObj = nullptr;
						proj_setTransform(logic, obj->pitch, obj->yaw);

						playSound3D_oneshot(logic->reflectSnd, obj->posWS);

						logic->flags |= 1;
						obj->posWS.y = s_projNextPosY;
						collision_moveObj(obj, logic->delta.x, logic->delta.z);
						s_projSector = obj->sector;

						logic->delta.x = 0;
						logic->delta.y = 0;
						logic->delta.z = 0;
						return JFALSE;
					}
				}

				// Wall Hit!
				s_hitWall = wall;
				return JTRUE;
			}

			s_projIter++;
			s_projSector = wall->nextSector;
			s_projPath[s_projIter] = s_projSector;
			wall = collision_pathWallCollision(s_projSector);
		}

		if (!hitFloor && !hitCeil)
		{
			sector_getObjFloorAndCeilHeight(s_projSector, obj->posWS.y, &y0FloorHeight, &y0CeilHeight);
			if (s_projSector->secHeight > 0)
			{
				y0FloorHeight = s_projSector->floorHeight;
			}

			if (s_projNextPosY > y0FloorHeight)
			{
				hitFloor = true;
			}
			else if (s_projNextPosY < y0CeilHeight)
			{
				hitCeil = true;
			}
			else  // Nothing Hit!
			{
				// Projectile doesn't hit anything.
				return JFALSE;
			}
		}

		if (hitFloor)
		{
			SecObject* obj = logic->obj;
			// fraction of the path where the path hits the floor.
			fixed16_16 u = div16(y0FloorHeight - obj->posWS.y, logic->delta.y);
			s_projNextPosX = obj->posWS.x + mul16(logic->delta.x, u);
			s_projNextPosY = y0FloorHeight;
			s_projNextPosZ = obj->posWS.z + mul16(logic->delta.z, u);

			if (s_projSector->secHeight > 0)  // Hit water.
			{
				s_hitWater = JTRUE;
				return JTRUE;
			}
		}
		else if (hitCeil)
		{
			SecObject* obj = logic->obj;
			// fraction of the path where the path hits the floor.
			fixed16_16 u = div16(y0CeilHeight - obj->posWS.y, logic->delta.y);
			s_projNextPosX = obj->posWS.x + mul16(logic->delta.x, u);
			s_projNextPosY = y0CeilHeight;
			s_projNextPosZ = obj->posWS.z + mul16(logic->delta.z, u);
		}

		// If the thermal detonator is hitting the ground at a fast enough speed, play the reflect sound.
		if (logic->type == PROJ_THERMAL_DET && TFE_CoreMath::abs(logic->speed) > FIXED(7))
		{
			playSound3D_oneshot(logic->reflectSnd, obj->posWS);
		}

		if (logic->bounceCnt == -1)
		{
			obj->pitch = -obj->pitch;
			logic->prevColObj = nullptr;
			logic->prevObj = nullptr;

			// Some projectiles can bounce when hitting the floor or ceiling.
			logic->speed = mul16(logic->speed, logic->vertBounciness);
			proj_setTransform(logic, obj->pitch, obj->yaw);

			obj->posWS.y = s_projNextPosY;
			collision_moveObj(obj, logic->delta.x, logic->delta.z);
			s_projSector = obj->sector;	// eax

			logic->delta.x = 0;
			logic->delta.y = 0;
			logic->delta.z = 0;
			return JFALSE;
		}
		else if (s_projSector->flags1 & SEC_FLAGS1_MAG_SEAL)
		{
			s32 count = logic->bounceCnt;
			logic->bounceCnt--;
			if (count > 0)
			{
				vec3_fixed effectPos = { s_projNextPosX, s_projNextPosY, s_projNextPosZ };
				spawnHitEffect(logic->reflectEffectId, s_projSector, effectPos, logic->excludeObj);
				// Hit the floor or ceiling simply negates the pitch to reflect.
				obj->pitch = -obj->pitch;
				handleReflectVariation(logic, obj);

				logic->prevColObj = nullptr;
				logic->excludeObj = nullptr;
				proj_setTransform(logic, obj->pitch, obj->yaw);
				playSound3D_oneshot(logic->reflectSnd, obj->posWS);

				obj->posWS.y = s_projNextPosY;
				collision_moveObj(obj, logic->delta.x, logic->delta.z);
				s_projSector = obj->sector;

				logic->delta.x = 0;
				logic->delta.y = 0;
				logic->delta.z = 0;
				return JFALSE;
			}
		}

		// Hit floor or ceiling.
		return JTRUE;
	}
	
	// return JTRUE if an object is hit.
	JBool proj_getHitObj(ProjectileLogic* logic)
	{
		SecObject* obj = logic->obj;
		if (!logic->speed)
		{
			return JFALSE;
		}

		const fixed16_16 move = mul16(logic->speed, s_deltaTime);
		CollisionInterval interval =
		{
			obj->posWS.x,   // x0
			s_projNextPosX,	// x1
			obj->posWS.y,	// y0
			s_projNextPosY,	// y1
			obj->posWS.z,	// z0
			s_projNextPosZ,	// z1
			move,           // move during timestep
			logic->dir.x,	// dirX
			logic->dir.z	// dirZ
		};

		SecObject* hitObj = nullptr;
		while (!hitObj && s_projIter > 0)
		{
			s_projIter--;
			hitObj = collision_getObjectCollision(s_projStartSector[s_projIter], &interval, logic->prevColObj);
		}
		if (hitObj)
		{
			s_colHitObj = hitObj;
			s_colObjAdjX = obj->posWS.x + mul16(logic->dir.x, s_colObjOverlap);
			s_colObjAdjY = obj->posWS.y + mul16(logic->dir.y, s_colObjOverlap);
			s_colObjAdjZ = obj->posWS.z + mul16(logic->dir.z, s_colObjOverlap);
			s_colObjSector = collision_tryMove(obj->sector, obj->posWS.x, obj->posWS.z, s_colObjAdjX, s_colObjAdjZ);

			if (s_colObjSector)
			{
				s_colObjAdjY = min(max(s_colObjAdjY, s_colObjSector->ceilingHeight), s_colObjSector->floorHeight);
				if (logic->type == PROJ_CONCUSSION)
				{
					logic->hitEffectId = HEFFECT_CONCUSSION2;
				}
				return JTRUE;
			}
		}
		return JFALSE;
	}
		
	// Returns JTRUE if the hit was properly handled, otherwise returns JFALSE.
	JBool handleProjectileHit(ProjectileLogic* logic, ProjectileHitType hitType)
	{
		SecObject* obj  = logic->obj;
		RSector* sector = obj->sector;
		switch (hitType)
		{
			case PHIT_SKY:
			{
				stopSound(logic->flightSndId);

				// Delete the projectile itself.
				allocator_addRef(s_projectiles);
					deleteLogicAndObject((Logic*)logic);
				allocator_release(s_projectiles);

				allocator_deleteItem(s_projectiles, logic);
				return JTRUE;
			} break;
			case PHIT_SOLID:
			{
				// Stop the projectiles in-flight sound.
				stopSound(logic->flightSndId);

				// Spawn the projectile hit effect.
				if (obj->posWS.y <= sector->floorHeight && obj->posWS.y >= sector->ceilingHeight && (logic->type != PROJ_PUNCH || hitType != PHIT_OUT_OF_RANGE))
				{
					spawnHitEffect(logic->hitEffectId, obj->sector, obj->posWS, logic->excludeObj);
				}

				// Delete the projectile itself.
				allocator_addRef(s_projectiles);
					deleteLogicAndObject((Logic*)logic);
				allocator_release(s_projectiles);
				allocator_deleteItem(s_projectiles, logic);
				return JTRUE;
			} break;
			case PHIT_OUT_OF_RANGE:
			{
				stopSound(logic->flightSndId);
				if (logic->flags & PROJFLAG_EXPLODE)
				{
					if (obj->posWS.y <= sector->floorHeight && obj->posWS.y >= sector->ceilingHeight && (logic->type != PROJ_PUNCH || hitType != PHIT_OUT_OF_RANGE))
					{
						spawnHitEffect(logic->hitEffectId, sector, obj->posWS, logic->excludeObj);
					}
				}

				// Delete the projectile itself.
				allocator_addRef(s_projectiles);
					deleteLogicAndObject((Logic*)logic);
				allocator_release(s_projectiles);
				allocator_deleteItem(s_projectiles, logic);
				return JTRUE;
			} break;
			case PHIT_WATER:
			{
				stopSound(logic->flightSndId);
				spawnHitEffect(HEFFECT_SPLASH, obj->sector, obj->posWS, logic->excludeObj);

				// Delete the projectile itself.
				allocator_addRef(s_projectiles);
					deleteLogicAndObject((Logic*)logic);
				allocator_release(s_projectiles);
				allocator_deleteItem(s_projectiles, logic);
				return JTRUE;
			} break;
		}

		return JFALSE;
	}

}  // TFE_DarkForces