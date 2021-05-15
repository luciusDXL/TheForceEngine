#include "projectile.h"
#include "animLogic.h"
#include <TFE_Collision/collision.h>
#include <TFE_InfSystem/infSystem.h>
#include <TFE_Level/robject.h>
#include <TFE_Level/rwall.h>
#include <TFE_Memory/allocator.h>

using namespace TFE_InfSystem;
using namespace TFE_Memory;
using namespace TFE_Level;
using namespace TFE_Collision;

namespace TFE_DarkForces
{
	//////////////////////////////////////////////////////////////
	// Structures and Constants
	//////////////////////////////////////////////////////////////
	static const Tick c_mortarGravityAccel = FIXED(120);	// 120.0 units / s^2

	//////////////////////////////////////////////////////////////
	// Internal State
	//////////////////////////////////////////////////////////////
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

	static RSector*   s_projStartSector[16];
	static RSector*   s_projPath[16];
	static s32        s_projIter;
	static fixed16_16 s_projNextPosX;
	static fixed16_16 s_projNextPosY;
	static fixed16_16 s_projNextPosZ;
	static u32        s_hitWater;
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
	u32 proj_move(ProjectileLogic* logic);
	u32 proj_getHitObj(ProjectileLogic* logic);

	ProjectileHitType stdProjectileUpdateFunc(ProjectileLogic* logic);
	ProjectileHitType landMineUpdateFunc(ProjectileLogic* logic);
	ProjectileHitType mortarUpdateFunc(ProjectileLogic* logic);

	//////////////////////////////////////////////////////////////
	// API Implementation
	//////////////////////////////////////////////////////////////
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

	//////////////////////////////////////////////////////////////
	// Internal Implementation
	//////////////////////////////////////////////////////////////
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
		logic->vel.y += mul16(c_mortarGravityAccel, dt);

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
		SecObject* obj = logic->obj;
		u32 envHit = proj_move(logic);
		u32 objHit = proj_getHitObj(logic);
		if (objHit)
		{
			obj->posWS.y = s_colObjAdjY;
			fixed16_16 dz = s_colObjAdjZ - obj->posWS.z;
			fixed16_16 dx = s_colObjAdjX - obj->posWS.x;
			RSector* newSector = collision_moveObj(obj, dx, dz);

			if (logic->u78 == -1)
			{
				// TODO
			}

			if (logic->type == PROJ_PUNCH)
			{
				spawnHitEffect(logic->hitEffectId, obj->sector, s_colObjAdjX, s_colObjAdjY, s_colObjAdjZ, logic->u6c);
			}
			s_hitWallFlag = 2;
			if (s_colHitObj->entityFlags & ETFLAG_PROXIMITY)
			{
				// TODO
			}
			if (logic->dmg)
			{
				s_infMsgEntity = logic;
				inf_sendObjMessage(s_colHitObj, IMSG_DAMAGE, 0);
			}

			if (s_hitWallFlag > 4)
			{
				return PHIT_SOLID;
			}

			if (s_hitWallFlag <= 1)	// s_projSector->flags1 & SEC_FLAGS1_NOWALL_DRAW
			{
				return PHIT_SKY;
			}
			else if (s_hitWallFlag <= 3 || s_hitWallFlag > 4)  // !(s_projSector->flags1 & SEC_FLAGS1_NOWALL_DRAW)
			{
				return PHIT_SOLID;
			}
			else if (s_hitWallFlag == 4)
			{
				// TODO - I don't think this code will ever be hit.
			}
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
				else if (s_hitWallFlag == 4)
				{
					// TODO - I don't think this code will ever be hit.
				}
			}
			if (s_hitWater == 0)
			{
				return PHIT_SOLID;
			}
			return PHIT_WATER;
		}

		// Hit nothing.
		obj->posWS.y += logic->delta.y;
		collision_moveObj(obj, logic->delta.x, logic->delta.z);
		return PHIT_NONE;
	}

	u32 proj_move(ProjectileLogic* logic)
	{
		SecObject* obj = logic->obj;
		RSector* sector = obj->sector;

		s_hitWall = nullptr;
		s_hitWater = 0;
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

				if (logic->u78 == -1)
				{
					// TODO
				}
				if (s_projSector->flags1 & SEC_FLAGS1_MAG_SEAL)
				{
					// TODO
				}

				// Wall Hit!
				s_hitWall = wall;
				return 0xffffffff;
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
				return 0;
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
				s_hitWater = 0xffffffff;
				return 0xffffffff;
			}
		}
		else if (hitCeil)
		{
			SecObject* obj = logic->obj;
			// fraction of the path where the path hits the floor.
			s32 u = div16(y0CeilHeight - obj->posWS.y, logic->delta.y);

			s_projNextPosX = obj->posWS.x + mul16(logic->delta.x, u);
			s_projNextPosY = y0CeilHeight;
			s_projNextPosZ = obj->posWS.z + mul16(logic->delta.z, u);
		}

		if (logic->type == PROJ_THERMAL_DET)
		{
			// TODO
		}
		if (logic->u78 == -1)	//Mine
		{
			// TODO
			return 0;
		}
		if (s_projSector->flags1 & SEC_FLAGS1_MAG_SEAL)
		{
			// TODO
		}

		// Hit floor or ceiling.
		return 0xffffffff;
	}
		
	u32 proj_getHitObj(ProjectileLogic* logic)
	{
		SecObject* obj = logic->obj;
		if (!logic->speed)
		{
			return 0;
		}

		fixed16_16 move = mul16(logic->speed, s_deltaTime);
		CollisionInterval interval =
		{
			obj->posWS.x,
			s_projNextPosX,
			obj->posWS.y,
			s_projNextPosY,
			obj->posWS.z,
			s_projNextPosZ,
			move,
			logic->dir.x,
			logic->dir.z
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
					logic->hitEffectId = HEFFECT_CONCUSSION;
				}
				return 0xffffffff;
			}
		}
		return 0;
	}

}  // TFE_DarkForces