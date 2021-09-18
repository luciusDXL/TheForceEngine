#include "projectile.h"
#include "animLogic.h"
#include "random.h"
#include "player.h"
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_Jedi/Collision/collision.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/InfSystem/message.h>
#include <TFE_Jedi/Task/task.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/Sound/soundSystem.h>

using namespace TFE_Jedi;

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
		HOMING_MISSILE_EXPLODE_RNG = FIXED(8),	 // The approximate distance when a homing missile might explode early.
		MAX_HOMING_ANGULAR_VEL     = 9102,       // Maximum angle change per second: ~200 degrees / second.
		HOMING_ANGULAR_ACCEL       = 1820,       // Rate at which homing speed increases in angular units / second: ~40 degrees / second.
		HOMING_PITCH_ZERO_MIN      = 2047,       // Range of pitch angles that are mapped to zero degrees.
		HOMING_PITCH_ZERO_MAX      = 14335,      // Range of pitch angles that are mapped to zero degrees.
	};

	//////////////////////////////////////////////////////////////
	// Internal State
	//////////////////////////////////////////////////////////////
	static Allocator* s_projectiles = nullptr;

	static JediModel* s_boltModel;
	static JediModel* s_greenBoltModel;
	static Wax*       s_plasmaProj;
	static WaxFrame*  s_repeaterProj;
	static Wax*       s_mortarProj;
	static Wax*       s_missileProj;
	static Wax*       s_cannonProj;
	static Wax*       s_probeProj;
	static Wax*       s_homingMissileProj;
	static Wax*       s_bobafetBall;
	static WaxFrame*  s_landmineWpnFrame;
	static WaxFrame*  s_landmineFrame;
	static WaxFrame*  s_thermalDetProj;

	static SoundSourceID s_stdProjCameraSnd       = NULL_SOUND;
	static SoundSourceID s_stdProjReflectSnd      = NULL_SOUND;
	static SoundSourceID s_thermalDetReflectSnd   = NULL_SOUND;
	static SoundSourceID s_plasmaCameraSnd        = NULL_SOUND;
	static SoundSourceID s_plasmaReflectSnd       = NULL_SOUND;
	static SoundSourceID s_missileLoopingSnd      = NULL_SOUND;
	static SoundSourceID s_landMineTriggerSnd     = NULL_SOUND;
	static SoundSourceID s_bobaBallCameraSnd      = NULL_SOUND;
	static SoundSourceID s_homingMissileFlightSnd = NULL_SOUND;

	static RSector*   s_projStartSector[16];
	static RSector*   s_projPath[16];
	static RSector*   s_projSector;
	static s32        s_projIter;
	static fixed16_16 s_projNextPosX;
	static fixed16_16 s_projNextPosY;
	static fixed16_16 s_projNextPosZ;
	static JBool      s_hitWater;
	static RWall*     s_hitWall;
	static u32        s_hitWallFlag;
	
	// Projectile specific collisions
	static RSector*   s_colObjSector;
	static SecObject* s_colHitObj;
	static fixed16_16 s_colObjAdjX;
	static fixed16_16 s_colObjAdjY;
	static fixed16_16 s_colObjAdjZ;

	// Task
	static Task* s_projectileTask = nullptr;

	//////////////////////////////////////////////////////////////
	// Forward Declarations
	//////////////////////////////////////////////////////////////
	void projectileLogicCleanupFunc(Logic* logic);
	void proj_setTransform(ProjectileLogic* logic, angle14_32 pitch, angle14_32 yaw);
	JBool proj_move(ProjectileLogic* logic);
	JBool proj_getHitObj(ProjectileLogic* logic);
		
	ProjectileHitType stdProjectileUpdateFunc(ProjectileLogic* logic);
	ProjectileHitType landMineUpdateFunc(ProjectileLogic* logic);
	ProjectileHitType arcingProjectileUpdateFunc(ProjectileLogic* logic);
	ProjectileHitType homingMissileProjectileUpdateFunc(ProjectileLogic* logic);

	void projectileTaskFunc(s32 id);

	//////////////////////////////////////////////////////////////
	// API Implementation
	//////////////////////////////////////////////////////////////
	void projectile_startup()
	{
		s_boltModel         = TFE_Model_Jedi::get("wrbolt.3do");
		s_thermalDetProj    = TFE_Sprite_Jedi::getFrame("wdet.fme");
		s_repeaterProj      = TFE_Sprite_Jedi::getFrame("bullet.fme");
		s_plasmaProj        = TFE_Sprite_Jedi::getWax("wemiss.wax");
		s_mortarProj        = TFE_Sprite_Jedi::getWax("wshell.wax");
		s_landmineWpnFrame  = TFE_Sprite_Jedi::getFrame("wmine.fme");
		s_cannonProj        = TFE_Sprite_Jedi::getWax("wplasma.wax");
		s_missileProj       = TFE_Sprite_Jedi::getWax("wmsl.wax");
		s_landmineFrame     = TFE_Sprite_Jedi::getFrame("wlmine.fme");
		s_greenBoltModel    = TFE_Model_Jedi::get("wgbolt.3do");
		s_probeProj         = TFE_Sprite_Jedi::getWax("widball.wax");
		s_homingMissileProj = TFE_Sprite_Jedi::getWax("wdt3msl.wax");
		s_bobafetBall       = TFE_Sprite_Jedi::getWax("bobaball.wax");

		s_stdProjReflectSnd      = sound_Load("boltref1.voc");
		s_thermalDetReflectSnd   = sound_Load("thermal1.voc");
		s_plasmaReflectSnd       = sound_Load("bigrefl1.voc");
		s_stdProjCameraSnd       = sound_Load("lasrby.voc");
		s_plasmaCameraSnd        = sound_Load("emisby.voc");
		s_missileLoopingSnd      = sound_Load("rocket-1.voc");
		s_homingMissileFlightSnd = sound_Load("tracker.voc");
		s_bobaBallCameraSnd      = sound_Load("fireball.voc");
		s_landMineTriggerSnd     = sound_Load("beep-10.voc");
	}

	void projectile_createTask()
	{
		s_projectiles = allocator_create(sizeof(ProjectileLogic));
		s_projectileTask = createTask("projectiles", projectileTaskFunc);
	}

	// TODO: Move projectile data to an external file to avoid hardcoding it for TFE.
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

		obj_addLogic(projObj, (Logic*)projLogic, s_projectileTask, projectileLogicCleanupFunc);
		
		switch (type)
		{
			case PROJ_PUNCH:
			{
				spirit_setData(projObj);

				projLogic->type = PROJ_PUNCH;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = FIXED(6);
				projLogic->falloffAmt = 0;
				projLogic->projForce = 1310;
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

				projLogic->projForce = 655;
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

				projLogic->projForce = 655;
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
				projObj->worldWidth = 0;

				projLogic->flags |= PROJFLAG_EXPLODE;
				projLogic->type = PROJ_THERMAL_DET;
				projLogic->updateFunc = arcingProjectileUpdateFunc;
				projLogic->dmg = 0;
				projLogic->minDmg = ONE_16;
				projLogic->falloffAmt = 0;
				projLogic->dmgFalloffDelta = 0;

				projLogic->projForce = 6553;
				projLogic->speed = FIXED(80);
				projLogic->horzBounciness = 29491;	// 0.45
				projLogic->vertBounciness = 58327;  // 0.89
				projLogic->bounceCnt = -1;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->hitEffectId = HEFFECT_THERMDET_EXP;
				projLogic->duration = s_curTick + 436;	// ~3 seconds
				projLogic->cameraPassSnd = NULL_SOUND;
				projLogic->reflectSnd = s_thermalDetReflectSnd;
			} break;
			case PROJ_REPEATER:
			{
				if (s_repeaterProj)
				{
					frame_setData(projObj, s_repeaterProj);
				}
				projObj->flags |= OBJ_FLAG_FULLBRIGHT;
				projObj->worldWidth = 0;

				projLogic->type = PROJ_REPEATER;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = FIXED(10);
				projLogic->minDmg = ONE_16;
				projLogic->falloffAmt = 19660;	// 0.3 damage
				projLogic->dmgFalloffDelta = 19660;	// 135 seconds, this probably should have been 0.3 seconds, or 43

				projLogic->projForce = 1966;
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
					sprite_setData(projObj, s_plasmaProj);
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

				projLogic->projForce = 3276;
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
					sprite_setData(projObj, s_mortarProj);
				}
				// Setup the looping wax animation.
				obj_setSpriteAnim(projObj);
				projObj->worldWidth = 0;

				projLogic->type = PROJ_MORTAR;
				projLogic->updateFunc = arcingProjectileUpdateFunc;
				projLogic->dmg = 0;	// Damage is set to 0 for some reason.
				projLogic->falloffAmt = 0;		// No falloff
				projLogic->dmgFalloffDelta = 0;

				projLogic->projForce = 13107;
				projLogic->speed = FIXED(110);
				projLogic->horzBounciness = 26214;	// 0.4
				projLogic->vertBounciness = 39321;	// 0.6
				projLogic->bounceCnt = 0;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->hitEffectId = HEFFECT_MORTAR_EXP;
				projLogic->duration = s_curTick + 582;	// ~4 seconds -> ~440 units
			} break;
			case PROJ_LAND_MINE:
			{
				if (s_landmineWpnFrame)
				{
					frame_setData(projObj, s_landmineWpnFrame);
				}
				projObj->entityFlags |= ETFLAG_LANDMINE_WPN;
				projObj->flags |= OBJ_FLAG_HAS_COLLISION;
				projObj->worldWidth = 0;

				projLogic->type = PROJ_LAND_MINE;
				projLogic->updateFunc = arcingProjectileUpdateFunc;
				projLogic->dmg = 0;
				projLogic->falloffAmt = 0;
				projLogic->dmgFalloffDelta = 0;

				projLogic->projForce = 13107;
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
					frame_setData(projObj, s_landmineWpnFrame);
				}
				projObj->entityFlags |= ETFLAG_LANDMINE_WPN;
				projObj->flags |= OBJ_FLAG_HAS_COLLISION;
				projObj->worldWidth = 0;

				projLogic->type = PROJ_LAND_MINE_PROX;
				projLogic->updateFunc = arcingProjectileUpdateFunc;
				projLogic->dmg = 0;
				projLogic->falloffAmt = 0;
				projLogic->dmgFalloffDelta = 0;

				projLogic->projForce = 13107;
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
				projLogic->projForce = 13107;
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
				spirit_setData(projObj);
				projObj->worldWidth = 0;

				projLogic->type = PROJ_CONCUSSION;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = 0;
				projLogic->falloffAmt = 0;
				projLogic->dmgFalloffDelta = 0;

				projLogic->projForce = 3932;
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
					sprite_setData(projObj, s_cannonProj);
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

				projLogic->projForce = 4032;
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
					sprite_setData(projObj, s_missileProj);
				}
				// Setup the looping wax animation.
				obj_setSpriteAnim(projObj);
				projObj->worldWidth = 0;

				projLogic->type = PROJ_MISSILE;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = 0;	// Damage is set to 0 for some reason.
				projLogic->falloffAmt = 0;		// No falloff
				projLogic->dmgFalloffDelta = 0;

				projLogic->projForce = ONE_16;
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

				projLogic->projForce = 1966;
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

				projLogic->projForce = 655;
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
				spirit_setData(projObj);

				projLogic->type = PROJ_EXP_BARREL;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = 0;
				projLogic->falloffAmt = 0;
				projLogic->dmgFalloffDelta = 0;

				projLogic->projForce = 0;
				projLogic->speed = 0;
				projLogic->horzBounciness = 0;
				projLogic->vertBounciness = 0;
				projLogic->bounceCnt = 0;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->flags |= PROJFLAG_EXPLODE;
				projLogic->hitEffectId = HEFFECT_EXP_BARREL;
				projLogic->duration = s_curTick;
			} break;
			case PROJ_HOMING_MISSILE:
			{
				if (s_homingMissileProj)
				{
					sprite_setData(projObj, s_homingMissileProj);
				}
				obj_setSpriteAnim(projObj);
				projObj->flags |= OBJ_FLAG_ENEMY;

				projLogic->flags |= PROJFLAG_EXPLODE;
				projLogic->type = PROJ_HOMING_MISSILE;
				projLogic->updateFunc = homingMissileProjectileUpdateFunc;
				projLogic->dmg = 0;
				projLogic->minDmg = ONE_16;
				projLogic->falloffAmt = 0;
				projLogic->dmgFalloffDelta = ONE_16;

				projLogic->projForce = ONE_16;
				projLogic->speed = FIXED(58);
				projLogic->horzBounciness = 58982;	// 0.9
				projLogic->vertBounciness = 58982;
				projLogic->bounceCnt = 0;
				projLogic->homingAngleSpd = 455;	// Starting homing rate = 10 degrees / second
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->flightSndSource = s_homingMissileFlightSnd;
				projLogic->hitEffectId = HEFFECT_MISSILE_EXP;
				projLogic->duration = s_curTick + 1456;
				projLogic->reflectSnd = 0;
			} break;
			case PROJ_PROBE_PROJ:
			{
				if (s_probeProj)
				{
					sprite_setData(projObj, s_probeProj);
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

				projLogic->projForce = 2621;
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
			case PROJ_BOBAFET_BALL:
			{
				if (s_bobafetBall)
				{
					sprite_setData(projObj, s_bobafetBall);
				}
				// Setup the looping wax animation.
				obj_setSpriteAnim(projObj);
				projObj->worldWidth = 0;

				projLogic->type = PROJ_BOBAFET_BALL;
				projLogic->updateFunc = stdProjectileUpdateFunc;
				projLogic->dmg = 0;
				projLogic->falloffAmt = 0;

				projLogic->projForce = ONE_16;
				projLogic->speed = FIXED(90);
				projLogic->horzBounciness = 58982;	// 0.9
				projLogic->vertBounciness = 58982;
				projLogic->bounceCnt = 0;
				projLogic->reflectEffectId = HEFFECT_NONE;
				projLogic->hitEffectId = HEFFECT_EXP_25;
				projLogic->duration = s_curTick + 1456;
				projLogic->cameraPassSnd = s_bobaBallCameraSnd;
			} break;
		}

		projObj->posWS.x = x;
		projObj->posWS.y = y;
		projObj->posWS.z = z;
		sector_addObject(sector, projObj);
		task_makeActive(s_projectileTask);

		return (Logic*)projLogic;
	}
		
	void projectileTaskFunc(s32 id)
	{
		struct LocalContext
		{
			ProjectileLogic* projLogic;
		};
		task_begin_ctx;

		while (id != -1)
		{
			taskCtx->projLogic = nullptr;
			task_yield(TASK_NO_DELAY);

			// Keep looping until a projectile becomes active.
			// Note: this task will need to be made active by something else to wake up.
			while (!taskCtx->projLogic)
			{
				taskCtx->projLogic = (ProjectileLogic*)allocator_getHead(s_projectiles);
				if (!taskCtx->projLogic)
				{
					task_yield(TASK_SLEEP);
				}
			}

			taskCtx->projLogic = (ProjectileLogic*)allocator_getHead(s_projectiles);
			while (taskCtx->projLogic)
			{
				ProjectileLogic* projLogic = taskCtx->projLogic;

				SecObject* obj = projLogic->logic.obj;
				ProjectileHitType projHitType = PHIT_NONE;
				Tick curTick = s_curTick;

				// The projectile is still active.
				if (curTick < projLogic->duration)
				{
					// Handle damage falloff.
					if (projLogic->falloffAmt && curTick > projLogic->nextFalloffTick)
					{
						projLogic->dmg -= projLogic->falloffAmt;
						if (projLogic->dmg < projLogic->minDmg)
						{
							projLogic->nextFalloffTick = 0xffffffff;	// This basically sets the falloff to "infinity" so it never enters this block again.
							projLogic->dmg = projLogic->minDmg;
						}
						else
						{
							projLogic->nextFalloffTick += projLogic->dmgFalloffDelta;
						}
					}
				}
				else  // The projectile has reached the end of its life.
				{
					const ProjectileType type = projLogic->type;
					// Note that proximity landmines that have reached end of life don't explode right away, but instead check for valid object proximity.
					if (type == PROJ_LAND_MINE_PLACED)	// Pre-placed landmines.
					{
						const fixed16_16 approxDist = distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, obj->posWS.x, obj->posWS.z);
						if (approxDist <= PL_LANDMINE_TRIGGER_RADIUS)
						{
							if (s_playerObject->posWS.y >= obj->posWS.y - PL_LANDMINE_TRIGGER_RADIUS && s_playerObject->posWS.y <= obj->posWS.y + PL_LANDMINE_TRIGGER_RADIUS &&
								collision_isAnyObjectInRange(obj->sector, PL_LANDMINE_TRIGGER_RADIUS, obj->posWS, obj, ETFLAG_PLAYER))
							{
								projLogic->type = PROJ_LAND_MINE;
								projLogic->duration = s_curTick + 87;  // The landmine will explode in 0.6 seconds.
								playSound3D_oneshot(s_landMineTriggerSnd, obj->posWS);
							}
						}
						else
						{
							// subtracts ~dist/64 ticks from the current time.
							// which ensures it will check here again next tick, but it was going to do that anyway...
							projLogic->duration = s_curTick - floor16(mul16(approxDist / 64, 9545318));	// 9545318 = 145.65
						}
					}
					else if (type == PROJ_LAND_MINE_PROX)	// Player placed proximity landmine (secondary fire).
					{
						if (collision_isAnyObjectInRange(obj->sector, WP_LANDMINE_TRIGGER_RADIUS, obj->posWS, obj, ETFLAG_PLAYER | ETFLAG_AI_ACTOR))
						{
							projLogic->type = PROJ_LAND_MINE;
							projLogic->duration = s_curTick + TICKS_PER_SECOND;	// The landmine will explode in 1 second.
							playSound3D_oneshot(s_landMineTriggerSnd, obj->posWS);
						}
					}
					else
					{
						projHitType = PHIT_OUT_OF_RANGE;
					}
				}  // if (curTick < projLogic->duration)

				if (projHitType == PHIT_NONE)
				{
					RSector* sector = obj->sector;
					InfLink* link = (InfLink*)allocator_getHead(sector->infLink);
					if (link && link->type == LTYPE_SECTOR && inf_isOnMovingFloor(obj, link->elev, sector))
					{
						fixed16_16 res;
						vec3_fixed vel;
						inf_getMovingElevatorVelocity(link->elev, &vel, &res);
						projLogic->vel.y = vel.y;
						projLogic->vel.x += vel.x;
						projLogic->vel.z += vel.z;
					}
					if (projLogic->updateFunc)
					{
						projHitType = projLogic->updateFunc(projLogic);
					}
				}

				// Play a looping sound as the projectile travels, this updates its position if already playing.
				if (projLogic->flightSndSource)
				{
					projLogic->flightSndId = playSound3D_looping(projLogic->flightSndSource, projLogic->flightSndId, obj->posWS);
				}
				// Play a sound as the object passes near the camera.
				if (projLogic->cameraPassSnd && (projLogic->flags & PROJFLAG_CAMERA_PASS_SOUND))
				{
					fixed16_16 dy = TFE_Jedi::abs(obj->posWS.y - s_eyePos.y);
					fixed16_16 approxDist = dy + distApprox(s_eyePos.x, s_eyePos.z, obj->posWS.x, obj->posWS.z);
					if (approxDist < CAMERA_SOUND_RANGE)
					{
						playSound3D_oneshot(projLogic->cameraPassSnd, obj->posWS);
						projLogic->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
					}
				}
				else if (projLogic->cameraPassSnd && projLogic->flightSndId)
				{
					stopSound(projLogic->flightSndId);
					projLogic->flightSndId = NULL_SOUND;
				}
				// Finally handle the hit itself (this includes going out of range).
				if (projHitType != PHIT_NONE)
				{
					handleProjectileHit(projLogic, projHitType);
				}
				taskCtx->projLogic = (ProjectileLogic*)allocator_getNext(s_projectiles);
			}  // while (taskCtx->projLogic)
		}  // while (id != -1)

		task_end;
	}

	void triggerLandMine(ProjectileLogic* projLogic, Tick delay)
	{
		projLogic->type = PROJ_LAND_MINE;
		projLogic->duration = s_curTick + delay;

		SecObject* obj = projLogic->logic.obj;
		playSound3D_oneshot(s_landMineTriggerSnd, obj->posWS);
	}

	//////////////////////////////////////////////////////////////
	// Internal Implementation
	//////////////////////////////////////////////////////////////
	// This function never actually gets called because the Projectile system handles the cleanup.
	void projectileLogicCleanupFunc(Logic* logic)
	{
		allocator_addRef(s_projectiles);
		deleteLogicAndObject(logic);
		allocator_deleteItem(s_projectiles, logic);
		// Bug: allocator_release() is never called
	}

	// The "standard" update function - projectiles travel in a straight line with a fixed velocity.
	ProjectileHitType stdProjectileUpdateFunc(ProjectileLogic* projLogic)
	{
		// Calculate how much the projectile moves this timeslice.
		const fixed16_16 dt = s_deltaTime;
		projLogic->delta.x = mul16(projLogic->vel.x, dt);
		projLogic->delta.y = mul16(projLogic->vel.y, dt);
		projLogic->delta.z = mul16(projLogic->vel.z, dt);

		return proj_handleMovement(projLogic);
	}

	// Landmines do not move but can fall if off of the ground.
	ProjectileHitType landMineUpdateFunc(ProjectileLogic* projLogic)
	{
		SecObject* obj = projLogic->logic.obj;
		fixed16_16 ceilHeight, floorHeight;
		sector_getObjFloorAndCeilHeight(obj->sector, obj->posWS.y, &floorHeight, &ceilHeight);

		// Gravity - fall if the landmine is above the ground.
		if (obj->posWS.y < floorHeight)
		{
			const fixed16_16 dt = s_deltaTime;
			projLogic->vel.y  += mul16(PROJ_GRAVITY_ACCEL, dt);
			projLogic->delta.y = mul16(projLogic->vel.y, dt);
			return proj_handleMovement(projLogic);
		}
		return PHIT_NONE;
	}
		
	// Projectiles, such as Thermal Detonators and Mortars fly in an arc.
	// This is done by giving them an initial velocity and then modifying that velocity
	// over time based on gravity.
	ProjectileHitType arcingProjectileUpdateFunc(ProjectileLogic* projLogic)
	{
		const fixed16_16 dt = s_deltaTime;
		// The projectile arcs due to gravity, accel = 120.0 units / s^2
		projLogic->vel.y += mul16(PROJ_GRAVITY_ACCEL, dt);
		// Get the frame delta from the velocity and delta time.
		projLogic->delta.x = mul16(projLogic->vel.x, dt);
		projLogic->delta.y = mul16(projLogic->vel.y, dt);
		projLogic->delta.z = mul16(projLogic->vel.z, dt);

		// Arcing projectiles cannot move straight up or down.
		const fixed16_16 horzSpeed = vec2Length(projLogic->vel.x, projLogic->vel.z);
		projLogic->dir.y = (horzSpeed) ? div16(projLogic->vel.y, horzSpeed) : 0;

		return proj_handleMovement(projLogic);
	}

	ProjectileHitType homingMissileProjectileUpdateFunc(ProjectileLogic* projLogic)
	{
		SecObject* missileObj = projLogic->logic.obj;
		SecObject* targetObj  = s_playerObject;
		fixed16_16 dt = s_deltaTime;

		angle14_32 homingAngleSpd = projLogic->homingAngleSpd & 0xffff;
		// Target near the head/upper chest.
		fixed16_16 yTarget = targetObj->posWS.y - targetObj->worldHeight + ONE_16;
		angle14_32 homingAngleDelta = mul16(homingAngleSpd, dt);

		// Calculate the horizontal turn amount (change in yaw).
		fixed16_16 dx = targetObj->posWS.x - missileObj->posWS.x;
		fixed16_16 dz = targetObj->posWS.z - missileObj->posWS.z;
		angle14_32 hAngle = vec2ToAngle(dx, dz);
		angle14_32 hAngleDiff = getAngleDifference(missileObj->yaw, hAngle);
		if (hAngleDiff > homingAngleDelta)
		{
			hAngleDiff = homingAngleDelta;
		}
		else if (hAngleDiff < -homingAngleDelta)
		{
			hAngleDiff = -homingAngleDelta;
		}
		// Turn toward the target (yaw).
		missileObj->yaw += hAngleDiff;

		// Calculate the vertical turn amount (change in pitch).
		fixed16_16 cosYaw, sinYaw;
		sinCosFixed(missileObj->yaw, &sinYaw, &cosYaw);

		fixed16_16 hDir = mul16(sinYaw, dx) + mul16(cosYaw, dz);
		fixed16_16 dy = missileObj->posWS.y - yTarget;
		angle14_32 vAngle = vec2ToAngle(dy, hDir);
		if (vAngle > HOMING_PITCH_ZERO_MIN && vAngle < HOMING_PITCH_ZERO_MAX)
		{
			vAngle = 0;
		}
		angle14_32 vAngleDiff = getAngleDifference(missileObj->pitch, homingAngleDelta);
		if (vAngleDiff > homingAngleDelta)
		{
			vAngleDiff = homingAngleDelta;
		}
		else if (vAngleDiff < -homingAngleDelta)
		{
			vAngleDiff = -homingAngleDelta;
		}
		// Turn toward the target (pitch).
		missileObj->pitch += vAngleDiff;

		// The missile will continue homing at a faster rate, up until the maximum.
		if (projLogic->homingAngleSpd < MAX_HOMING_ANGULAR_VEL)
		{
			projLogic->homingAngleSpd += mul16(HOMING_ANGULAR_ACCEL, dt);
		}

		// Set the projectile transform based on the new yaw and pitch.
		proj_setTransform(projLogic, missileObj->pitch, missileObj->yaw);

		// Finally calculate the per-frame movement from the velocity computed in proj_setTransform().
		projLogic->delta.x = mul16(projLogic->vel.x, dt);
		projLogic->delta.y = mul16(projLogic->vel.y, dt);
		projLogic->delta.z = mul16(projLogic->vel.z, dt);

		// Then apply movement and handle the 'explode early' special case.
		ProjectileHitType hitType = proj_handleMovement(projLogic);
		if (hitType == PHIT_NONE)
		{
			// There is a chance that the missile will explode early, once it gets within a certain range.
			fixed16_16 approxDist = distApprox(missileObj->posWS.x, missileObj->posWS.z, targetObj->posWS.x, targetObj->posWS.z);
			if (approxDist < HOMING_MISSILE_EXPLODE_RNG && (s_curTick & 1))
			{
				hitType = PHIT_SOLID;
			}
		}
		return hitType;
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

	void proj_setYawPitch(ProjectileLogic* projLogic, fixed16_16 sinPitch, fixed16_16 cosPitch, fixed16_16 sinYaw, fixed16_16 cosYaw)
	{
		projLogic->dir.x = sinYaw;
		projLogic->dir.z = cosYaw;
		projLogic->vel.x = mul16(mul16(sinYaw, projLogic->speed), cosPitch);
		projLogic->vel.z = mul16(mul16(cosYaw, projLogic->speed), cosPitch);
		projLogic->vel.y = -mul16(sinPitch, projLogic->speed);

		// Projectiles cannot point straight up or down.
		const fixed16_16 horzSpeed = vec2Length(projLogic->vel.x, projLogic->vel.z);
		projLogic->dir.y = (horzSpeed) ? div16(projLogic->vel.y, horzSpeed) : 0;

		SecObject* obj = projLogic->logic.obj;
		if (obj->type == OBJ_TYPE_3D)
		{
			proj_computeTransform3D(obj, sinPitch, cosPitch, sinYaw, cosYaw, s_deltaTime);
		}
	}

	void proj_setTransform(ProjectileLogic* projLogic, angle14_32 pitch, angle14_32 yaw)
	{
		SecObject* obj = projLogic->logic.obj;
		obj->roll  = 0;
		obj->pitch = pitch;
		obj->yaw   = yaw;

		fixed16_16 cosYaw, sinYaw;
		fixed16_16 cosPitch, sinPitch;
		sinCosFixed(yaw,   &sinYaw,   &cosYaw);
		sinCosFixed(pitch, &sinPitch, &cosPitch);

		proj_setYawPitch(projLogic, sinPitch, cosPitch, sinYaw, cosYaw);
	}

	// Originally this code was inlined in the final DOS code, but I am splitting it out to reduce redundant code.
	// This code adds some variation to the final yaw and pitch of a projectile after reflection if
	// projLogic->reflVariation is non-zero.
	void handleReflectVariation(ProjectileLogic* projLogic, SecObject* obj)
	{
		if (!projLogic->reflVariation) { return; }

		// Random variation between [-projLogic->reflVariation, projLogic->reflVariation]
		obj->yaw   += random(2 * projLogic->reflVariation) - projLogic->reflVariation;
		obj->pitch += random(2 * projLogic->reflVariation) - projLogic->reflVariation;
	}

	// Handle projectile movement.
	// Returns 0 if it moved without hitting anything.
	ProjectileHitType proj_handleMovement(ProjectileLogic* projLogic)
	{
		SecObject* obj = projLogic->logic.obj;
		JBool envHit = proj_move(projLogic);
		JBool objHit = proj_getHitObj(projLogic);
		if (objHit)
		{
			obj->posWS.y = s_colObjAdjY;
			fixed16_16 dz = s_colObjAdjZ - obj->posWS.z;
			fixed16_16 dx = s_colObjAdjX - obj->posWS.x;
			RSector* newSector = collision_moveObj(obj, dx, dz);

			if (projLogic->bounceCnt == -1)
			{
				projLogic->speed = mul16(projLogic->speed, projLogic->horzBounciness);
				angle14_32 offsetYaw = obj->yaw + 4096;
				// getAngleDifference() will always return 4096, then we add yaw + 4096 = yaw + 8192 = 180 degrees.
				// This could have been accomplished with: obj->yaw = (obj->yaw + 8192) & 16383
				obj->yaw = (getAngleDifference(obj->yaw, offsetYaw) + offsetYaw) & 16383;
				handleReflectVariation(projLogic, obj);

				projLogic->prevColObj = nullptr;
				projLogic->excludeObj = nullptr;
				proj_setTransform(projLogic, obj->pitch, obj->yaw);
				playSound3D_oneshot(projLogic->reflectSnd, obj->posWS);

				projLogic->flags |= PROJFLAG_CAMERA_PASS_SOUND;
				projLogic->delta.x = mul16(HALF_16, projLogic->dir.x);
				projLogic->delta.z = mul16(HALF_16, projLogic->dir.z);
				collision_moveObj(obj, projLogic->delta.x, projLogic->delta.z);

				projLogic->delta.x = 0;
				projLogic->delta.y = 0;
				projLogic->delta.z = 0;

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

			if (projLogic->type == PROJ_PUNCH)
			{
				vec3_fixed effectPos = { s_colObjAdjX, s_colObjAdjY, s_colObjAdjZ };
				spawnHitEffect(projLogic->hitEffectId, obj->sector, effectPos, projLogic->excludeObj);
			}
			s_hitWallFlag = 2;
			if (s_colHitObj->entityFlags & ETFLAG_PROJECTILE)
			{
				s_hitWallFlag = 0;
				Logic** objLogicPtr = (Logic**)allocator_getHead((Allocator*)s_colHitObj->logic);
				if (objLogicPtr)
				{
					ProjectileLogic* objLogic = (ProjectileLogic*)*objLogicPtr;
					// PROJ_HOMING_MISSILE can be destroyed by shooting it with a different type of projectile.
					if (projLogic->type != objLogic->type && objLogic->type == PROJ_HOMING_MISSILE)
					{
						s_hitWallFlag = 2;
						objLogic->duration = 0;
					}
				}
			}
			if (projLogic->dmg)
			{
				s_msgEntity = projLogic;
				message_sendToObj(s_colHitObj, MSG_DAMAGE, nullptr);
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
				inf_wallAndMirrorSendMessageAtPos(s_hitWall, projLogic->logic.obj, INF_EVENT_SHOOT_LINE, paramPos, s_projNextPosY);

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
		obj->posWS.y += projLogic->delta.y;
		collision_moveObj(obj, projLogic->delta.x, projLogic->delta.z);
		return PHIT_NONE;
	}

	// Return JTRUE if something was hit during movement.
	JBool proj_move(ProjectileLogic* projLogic)
	{
		SecObject* obj = projLogic->logic.obj;
		RSector* sector = obj->sector;

		s_hitWall = nullptr;
		s_hitWater = JFALSE;
		s_projSector = sector;
		s_projStartSector[0] = sector;
		s_projIter = 1;
		s_projNextPosX = obj->posWS.x + projLogic->delta.x;
		s_projNextPosY = obj->posWS.y + projLogic->delta.y;
		s_projNextPosZ = obj->posWS.z + projLogic->delta.z;

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
				fixed16_16 y1 = projLogic->type == PROJ_THERMAL_DET ? s_projNextPosY - obj->worldHeight : s_projNextPosY;
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
				bot = -SEC_SKY_HEIGHT;
				top =  SEC_SKY_HEIGHT;
			}

			collision_getHitPoint(&s_projNextPosX, &s_projNextPosZ);
			fixed16_16 len = vec2Length(s_projNextPosX - obj->posWS.x, s_projNextPosZ - obj->posWS.z);
			fixed16_16 yDelta = mul16(len, projLogic->dir.y);

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
			fixed16_16 nY1 = projLogic->type == PROJ_THERMAL_DET ? nextY - obj->worldHeight : nextY;
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
				fixed16_16 offsetX = mul16(offset, projLogic->dir.x);
				fixed16_16 offsetZ = mul16(offset, projLogic->dir.z);

				s_projNextPosY = nextY;
				s_projNextPosX -= offsetX;
				s_projNextPosZ -= offsetZ;

				projLogic->delta.x = s_projNextPosX - obj->posWS.x;
				projLogic->delta.y = s_projNextPosY - obj->posWS.y;
				projLogic->delta.z = s_projNextPosZ - obj->posWS.z;

				if (projLogic->bounceCnt == -1)
				{
					if (s_projNextPosY < s_projSector->ceilingHeight)
					{
						s_hitWall = wall;
						s_hitWallFlag = 1;
						return JTRUE;
					}
					projLogic->speed = mul16(projLogic->speed, projLogic->horzBounciness);
					obj->pitch = (getAngleDifference(obj->pitch, 0) & 16383);
					obj->yaw = (getAngleDifference(obj->yaw, wall->angle) + wall->angle) & 16383;
					handleReflectVariation(projLogic, obj);

					projLogic->prevColObj = nullptr;
					projLogic->excludeObj = nullptr;
					proj_setTransform(projLogic, obj->pitch, obj->yaw);
					playSound3D_oneshot(projLogic->reflectSnd, obj->posWS);

					projLogic->flags |= PROJFLAG_CAMERA_PASS_SOUND;
					obj->posWS.y = s_projNextPosY;
					collision_moveObj(obj, projLogic->delta.x, projLogic->delta.z);
					s_projSector = obj->sector;

					projLogic->delta.x = 0;
					projLogic->delta.y = 0;
					projLogic->delta.z = 0;

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
					s32 count = projLogic->bounceCnt;
					projLogic->bounceCnt--;
					if (count)
					{
						vec3_fixed effectPos = { s_projNextPosX, s_projNextPosY, s_projNextPosZ };
						spawnHitEffect(projLogic->reflectEffectId, s_projSector, effectPos, projLogic->excludeObj);

						SecObject* obj = projLogic->logic.obj;
						obj->yaw = (getAngleDifference(obj->yaw, wall->angle) + wall->angle) & 16383;
						handleReflectVariation(projLogic, obj);

						projLogic->prevColObj = nullptr;
						projLogic->excludeObj = nullptr;
						proj_setTransform(projLogic, obj->pitch, obj->yaw);
						playSound3D_oneshot(projLogic->reflectSnd, obj->posWS);

						projLogic->flags |= 1;
						obj->posWS.y = s_projNextPosY;
						collision_moveObj(obj, projLogic->delta.x, projLogic->delta.z);
						s_projSector = obj->sector;

						projLogic->delta.x = 0;
						projLogic->delta.y = 0;
						projLogic->delta.z = 0;
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
			SecObject* obj = projLogic->logic.obj;
			// fraction of the path where the path hits the floor.
			fixed16_16 u = div16(y0FloorHeight - obj->posWS.y, projLogic->delta.y);
			s_projNextPosX = obj->posWS.x + mul16(projLogic->delta.x, u);
			s_projNextPosY = y0FloorHeight;
			s_projNextPosZ = obj->posWS.z + mul16(projLogic->delta.z, u);

			if (s_projSector->secHeight > 0)  // Hit water.
			{
				s_hitWater = JTRUE;
				return JTRUE;
			}
		}
		else if (hitCeil)
		{
			SecObject* obj = projLogic->logic.obj;
			// fraction of the path where the path hits the floor.
			fixed16_16 u = div16(y0CeilHeight - obj->posWS.y, projLogic->delta.y);
			s_projNextPosX = obj->posWS.x + mul16(projLogic->delta.x, u);
			s_projNextPosY = y0CeilHeight;
			s_projNextPosZ = obj->posWS.z + mul16(projLogic->delta.z, u);
		}

		// If the thermal detonator is hitting the ground at a fast enough speed, play the reflect sound.
		if (projLogic->type == PROJ_THERMAL_DET && TFE_Jedi::abs(projLogic->speed) > FIXED(7))
		{
			playSound3D_oneshot(projLogic->reflectSnd, obj->posWS);
		}

		if (projLogic->bounceCnt == -1)
		{
			obj->pitch = -obj->pitch;
			projLogic->prevColObj = nullptr;
			projLogic->prevObj = nullptr;

			// Some projectiles can bounce when hitting the floor or ceiling.
			projLogic->speed = mul16(projLogic->speed, projLogic->vertBounciness);
			proj_setTransform(projLogic, obj->pitch, obj->yaw);

			obj->posWS.y = s_projNextPosY;
			collision_moveObj(obj, projLogic->delta.x, projLogic->delta.z);
			s_projSector = obj->sector;	// eax

			projLogic->delta.x = 0;
			projLogic->delta.y = 0;
			projLogic->delta.z = 0;
			return JFALSE;
		}
		else if (s_projSector->flags1 & SEC_FLAGS1_MAG_SEAL)
		{
			s32 count = projLogic->bounceCnt;
			projLogic->bounceCnt--;
			if (count > 0)
			{
				vec3_fixed effectPos = { s_projNextPosX, s_projNextPosY, s_projNextPosZ };
				spawnHitEffect(projLogic->reflectEffectId, s_projSector, effectPos, projLogic->excludeObj);
				// Hit the floor or ceiling simply negates the pitch to reflect.
				obj->pitch = -obj->pitch;
				handleReflectVariation(projLogic, obj);

				projLogic->prevColObj = nullptr;
				projLogic->excludeObj = nullptr;
				proj_setTransform(projLogic, obj->pitch, obj->yaw);
				playSound3D_oneshot(projLogic->reflectSnd, obj->posWS);

				obj->posWS.y = s_projNextPosY;
				collision_moveObj(obj, projLogic->delta.x, projLogic->delta.z);
				s_projSector = obj->sector;

				projLogic->delta.x = 0;
				projLogic->delta.y = 0;
				projLogic->delta.z = 0;
				return JFALSE;
			}
		}

		// Hit floor or ceiling.
		return JTRUE;
	}
	
	// return JTRUE if an object is hit.
	JBool proj_getHitObj(ProjectileLogic* projLogic)
	{
		SecObject* obj = projLogic->logic.obj;
		if (!projLogic->speed)
		{
			return JFALSE;
		}

		const fixed16_16 move = mul16(projLogic->speed, s_deltaTime);
		CollisionInterval interval =
		{
			obj->posWS.x,   // x0
			s_projNextPosX,	// x1
			obj->posWS.y,	// y0
			s_projNextPosY,	// y1
			obj->posWS.z,	// z0
			s_projNextPosZ,	// z1
			move,           // move during timestep
			projLogic->dir.x,	// dirX
			projLogic->dir.z	// dirZ
		};

		SecObject* hitObj = nullptr;
		while (!hitObj && s_projIter > 0)
		{
			s_projIter--;
			hitObj = collision_getObjectCollision(s_projStartSector[s_projIter], &interval, projLogic->prevColObj);
		}
		if (hitObj)
		{
			s_colHitObj = hitObj;
			s_colObjAdjX = obj->posWS.x + mul16(projLogic->dir.x, s_colObjOverlap);
			s_colObjAdjY = obj->posWS.y + mul16(projLogic->dir.y, s_colObjOverlap);
			s_colObjAdjZ = obj->posWS.z + mul16(projLogic->dir.z, s_colObjOverlap);
			s_colObjSector = collision_tryMove(obj->sector, obj->posWS.x, obj->posWS.z, s_colObjAdjX, s_colObjAdjZ);

			if (s_colObjSector)
			{
				s_colObjAdjY = min(max(s_colObjAdjY, s_colObjSector->ceilingHeight), s_colObjSector->floorHeight);
				if (projLogic->type == PROJ_CONCUSSION)
				{
					projLogic->hitEffectId = HEFFECT_CONCUSSION2;
				}
				return JTRUE;
			}
		}
		return JFALSE;
	}
		
	// Returns JTRUE if the hit was properly handled, otherwise returns JFALSE.
	JBool handleProjectileHit(ProjectileLogic* projLogic, ProjectileHitType hitType)
	{
		SecObject* obj  = projLogic->logic.obj;
		RSector* sector = obj->sector;
		switch (hitType)
		{
			case PHIT_SKY:
			{
				stopSound(projLogic->flightSndId);

				// Delete the projectile itself.
				allocator_addRef(s_projectiles);
					deleteLogicAndObject((Logic*)projLogic);
				allocator_release(s_projectiles);

				allocator_deleteItem(s_projectiles, projLogic);
				return JTRUE;
			} break;
			case PHIT_SOLID:
			{
				// Stop the projectiles in-flight sound.
				stopSound(projLogic->flightSndId);

				// Spawn the projectile hit effect.
				if (obj->posWS.y <= sector->floorHeight && obj->posWS.y >= sector->ceilingHeight && (projLogic->type != PROJ_PUNCH || hitType != PHIT_OUT_OF_RANGE))
				{
					spawnHitEffect(projLogic->hitEffectId, obj->sector, obj->posWS, projLogic->excludeObj);
				}

				// Delete the projectile itself.
				allocator_addRef(s_projectiles);
					deleteLogicAndObject((Logic*)projLogic);
				allocator_release(s_projectiles);
				allocator_deleteItem(s_projectiles, projLogic);
				return JTRUE;
			} break;
			case PHIT_OUT_OF_RANGE:
			{
				stopSound(projLogic->flightSndId);
				if (projLogic->flags & PROJFLAG_EXPLODE)
				{
					if (obj->posWS.y <= sector->floorHeight && obj->posWS.y >= sector->ceilingHeight && (projLogic->type != PROJ_PUNCH || hitType != PHIT_OUT_OF_RANGE))
					{
						spawnHitEffect(projLogic->hitEffectId, sector, obj->posWS, projLogic->excludeObj);
					}
				}

				// Delete the projectile itself.
				allocator_addRef(s_projectiles);
					deleteLogicAndObject((Logic*)projLogic);
				allocator_release(s_projectiles);
				allocator_deleteItem(s_projectiles, projLogic);
				return JTRUE;
			} break;
			case PHIT_WATER:
			{
				stopSound(projLogic->flightSndId);
				spawnHitEffect(HEFFECT_SPLASH, obj->sector, obj->posWS, projLogic->excludeObj);

				// Delete the projectile itself.
				allocator_addRef(s_projectiles);
					deleteLogicAndObject((Logic*)projLogic);
				allocator_release(s_projectiles);
				allocator_deleteItem(s_projectiles, projLogic);
				return JTRUE;
			} break;
		}

		return JFALSE;
	}

}  // TFE_DarkForces