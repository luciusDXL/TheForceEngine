#include <cstring>

#include "actor.h"
#include "../logic.h"
#include "aiActor.h"
#include <TFE_Game/igame.h>
#include <TFE_DarkForces/random.h>
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/pickup.h>
#include <TFE_DarkForces/player.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/InfSystem/message.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

using namespace TFE_Jedi;

struct Actor;
struct ActorHeader;

namespace TFE_DarkForces
{
	///////////////////////////////////////////
	// Internal State
	///////////////////////////////////////////
	struct ActorInternalState
	{
		Allocator* actorLogics;
		Task* actorTask;
		Task* actorPhysicsTask;
		JBool objCollisionEnabled;
	};
	static ActorInternalState s_istate = { 0 };
	static List* s_physicsActors = nullptr;

	///////////////////////////////////////////
	// Shared State
	///////////////////////////////////////////
	ActorState s_actorState = { 0 };
	SoundSourceID s_alertSndSrc[ALERT_COUNT];
	SoundSourceID s_officerAlertSndSrc[OFFICER_ALERT_COUNT];
	SoundSourceID s_stormAlertSndSrc[STORM_ALERT_COUNT];
	SoundSourceID s_agentSndSrc[AGENTSND_COUNT];

	///////////////////////////////////////////
	// Forward Declarations
	///////////////////////////////////////////
	void actorLogicTaskFunc(MessageType msg);
	void actorLogicMsgFunc(MessageType msg);
	void actorPhysicsTaskFunc(MessageType msg);
	void actorLogicCleanupFunc(Logic* logic);
	u32  actorLogicSetupFunc(Logic* logic, KEYWORD key);
	
	///////////////////////////////////////////
	// API Implementation
	///////////////////////////////////////////
	void actor_clearState()
	{
		memset(&s_istate, 0, sizeof(ActorInternalState));
		memset(&s_actorState, 0, sizeof(ActorState));
		s_istate.objCollisionEnabled = JTRUE;
		list_clear(s_physicsActors);
	}

	void actor_loadSounds()
	{
		s_alertSndSrc[ALERT_GAMOR]    = sound_Load("gamor-3.voc");
		s_alertSndSrc[ALERT_REEYEE]   = sound_Load("reeyee-1.voc");
		s_alertSndSrc[ALERT_BOSSK]    = sound_Load("bossk-1.voc");
		s_alertSndSrc[ALERT_CREATURE] = sound_Load("creatur1.voc");
		s_alertSndSrc[ALERT_PROBE]    = sound_Load("probe-1.voc");
		s_alertSndSrc[ALERT_INTDROID] = sound_Load("intalert.voc");

		s_officerAlertSndSrc[0] = sound_Load("ranofc02.voc");
		s_officerAlertSndSrc[1] = sound_Load("ranofc04.voc");
		s_officerAlertSndSrc[2] = sound_Load("ranofc05.voc");
		s_officerAlertSndSrc[3] = sound_Load("ranofc06.voc");

		s_stormAlertSndSrc[0] = sound_Load("ransto01.voc");
		s_stormAlertSndSrc[1] = sound_Load("ransto02.voc");
		s_stormAlertSndSrc[2] = sound_Load("ransto03.voc");
		s_stormAlertSndSrc[3] = sound_Load("ransto04.voc");
		s_stormAlertSndSrc[4] = sound_Load("ransto05.voc");
		s_stormAlertSndSrc[5] = sound_Load("ransto06.voc");
		s_stormAlertSndSrc[6] = sound_Load("ransto07.voc");
		s_stormAlertSndSrc[7] = sound_Load("ransto08.voc");

		// Attacks, hurt, death.
		s_agentSndSrc[AGENTSND_REMOTE_2]        = sound_Load("remote-2.voc");
		s_agentSndSrc[AGENTSND_AXE_1]           = sound_Load("axe-1.voc");
		s_agentSndSrc[AGENTSND_INTSTUN]         = sound_Load("intstun.voc");
		s_agentSndSrc[AGENTSND_PROBFIRE_11]     = sound_Load("probfir1.voc");
		s_agentSndSrc[AGENTSND_PROBFIRE_12]     = sound_Load("probfir1.voc");
		s_agentSndSrc[AGENTSND_CREATURE2]       = sound_Load("creatur2.voc");
		s_agentSndSrc[AGENTSND_PROBFIRE_13]     = sound_Load("probfir1.voc");
		s_agentSndSrc[AGENTSND_STORM_HURT]      = sound_Load("st-hrt-1.voc");
		s_agentSndSrc[AGENTSND_GAMOR_2]         = sound_Load("gamor-2.voc");
		s_agentSndSrc[AGENTSND_REEYEE_2]        = sound_Load("reeyee-2.voc");
		s_agentSndSrc[AGENTSND_BOSSK_3]         = sound_Load("bossk-3.voc");
		s_agentSndSrc[AGENTSND_CREATURE_HURT]   = sound_Load("creathrt.voc");
		s_agentSndSrc[AGENTSND_STORM_DIE]       = sound_Load("st-die-1.voc");
		s_agentSndSrc[AGENTSND_REEYEE_3]        = sound_Load("reeyee-3.voc");
		s_agentSndSrc[AGENTSND_BOSSK_DIE]       = sound_Load("bosskdie.voc");
		s_agentSndSrc[AGENTSND_GAMOR_1]         = sound_Load("gamor-1.voc");
		s_agentSndSrc[AGENTSND_CREATURE_DIE]    = sound_Load("creatdie.voc");
		s_agentSndSrc[AGENTSND_EEEK_3]          = sound_Load("eeek-3.voc");
		s_agentSndSrc[AGENTSND_SMALL_EXPLOSION] = sound_Load("ex-small.voc");
		s_agentSndSrc[AGENTSND_PROBE_ALM]       = sound_Load("probalm.voc");
		s_agentSndSrc[AGENTSND_TINY_EXPLOSION]  = sound_Load("ex-tiny1.voc");

		setSoundSourceVolume(s_agentSndSrc[AGENTSND_REMOTE_2], 40);
	}

	void actor_allocatePhysicsActorList()
	{
		s_physicsActors = list_allocate(sizeof(PhysicsActor*), 80);
	}

	void actor_addPhysicsActorToWorld(PhysicsActor* actor)
	{
		PhysicsActor** actorPtr = (PhysicsActor**)list_addItem(s_physicsActors);
		*actorPtr = actor;

		actor->vel = { 0, 0, 0 };
		actor->moveSndId = NULL_SOUND;
		actor->parent = actorPtr;
	}

	void actor_removePhysicsActorFromWorld(PhysicsActor* phyActor)
	{
		list_removeItem(s_physicsActors, phyActor->parent);
	}
		
	void actor_createTask()
	{
		s_istate.actorLogics = allocator_create(sizeof(ActorLogic));
		s_istate.actorTask = createSubTask("actor", actorLogicTaskFunc, actorLogicMsgFunc);
		s_istate.actorPhysicsTask = createSubTask("physics", actorPhysicsTaskFunc);
	}

	ActorLogic* actor_setupActorLogic(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorLogic* logic = (ActorLogic*)allocator_newItem(s_istate.actorLogics);
		memset(logic->aiActors, 0, sizeof(AiActor*) * 6);

		logic->actor = nullptr;
		logic->animTable = nullptr;
		logic->delay = 72;			// Delay in ticks
		logic->alertSndSrc = NULL_SOUND;
		logic->alertSndID = NULL_SOUND;
		logic->fov = 9557;			// ~210 degrees
		logic->nearDist = FIXED(20);
		logic->vel = { 0, 0, 0 };
		logic->freeTask = nullptr;
		logic->flags = 4;

		obj_addLogic(obj, (Logic*)logic, s_istate.actorTask, actorLogicCleanupFunc);
		if (setupFunc)
		{
			*setupFunc = actorLogicSetupFunc;
		}

		s_actorState.curLogic = (Logic*)logic;
		return logic;
	}

	JBool actorLogicSetupFunc(Logic* logic, KEYWORD key)
	{
		ActorLogic* actorLogic = (ActorLogic*)logic;
		Logic* res = nullptr;

		if (key == KW_PLUGIN)
		{
			KEYWORD pluginType = getKeywordIndex(s_objSeqArg1);
			KEYWORD thinkerType = getKeywordIndex(s_objSeqArg2);

			switch (pluginType)
			{
				case KW_MOVER:
				{
					// TODO
					// actorLogic->actor = actor_create(logic);
				} break;
				case KW_SHAKER:
				{
					// TODO
					// res = (Logic*)gameObj_create(logic);
				} break;
				case KW_THINKER:
				{
					if (thinkerType < KW_FOLLOW_Y)
					{
						// TODO
					}
					else if (thinkerType == KW_FOLLOW_Y)
					{
						// TODO
					}
					else if (thinkerType == KW_RANDOM_YAW)
					{
						// TODO
					}
				} break;
			}

			if (res)
			{
				for (s32 i = 0; i < ACTOR_MAX_AI; i++)
				{
					if (!actorLogic->aiActors[i])
					{
						actorLogic->aiActors[i] = (AiActor*)res;
						return JTRUE;
					}
				}
			}
		}
		return JFALSE;
	}

	void actor_initHeader(ActorHeader* header, Logic* logic)
	{
		header->func = nullptr;
		header->msgFunc = nullptr;
		header->u08 = 0;
		header->freeFunc = nullptr;
		header->nextTick = 0;
		header->obj = logic->obj;
	}

	fixed16_16 actor_initEnemy(ActorEnemy* enemyActor, Logic* logic)
	{
		actor_initHeader(&enemyActor->header, logic);

		enemyActor->target.speedRotation = 0;
		enemyActor->target.speed = 0;
		enemyActor->target.speedVert = 0;
		enemyActor->timing.delay = 218;
		enemyActor->timing.state0Delay = 218;
		enemyActor->timing.state2Delay = 0;
		enemyActor->timing.state4Delay = 291;
		enemyActor->timing.state1Delay = 43695;
		enemyActor->anim.frameRate = 5;
		enemyActor->anim.frameCount = ONE_16;
		enemyActor->anim.prevTick = 0;
		enemyActor->anim.state = 0;
		enemyActor->fireSpread = FIXED(30);
		enemyActor->state0NextTick = 0;
		enemyActor->fireOffset.x = 0;
		enemyActor->fireOffset.z = 0;

		enemyActor->target.flags &= 0xfffffff0;
		enemyActor->anim.flags |= 3;
		enemyActor->timing.nextTick = s_curTick + 0x4446;	// ~120 seconds

		SecObject* obj = enemyActor->header.obj;
		// world width and height was set from the sprite data.
		enemyActor->fireOffset.y = -((TFE_Jedi::abs(obj->worldHeight) >> 1) + ONE_16);

		enemyActor->projType = PROJ_RIFLE_BOLT;
		enemyActor->attackSecSndSrc = 0;
		enemyActor->attackPrimSndSrc = 0;
		enemyActor->meleeRange = 0;
		enemyActor->minDist = 0;
		enemyActor->maxDist = FIXED(160);
		enemyActor->meleeDmg = 0;
		enemyActor->ua4 = FIXED(230);
		enemyActor->attackFlags = 10;

		return enemyActor->fireOffset.y;
	}

	void actor_setDeathCollisionFlags()
	{
		ActorLogic* logic = (ActorLogic*)s_actorState.curLogic;
		Actor* actor = logic->actor;
		actor->collisionFlags |= 2;
		// Added to disable auto-aim when dying.
		logic->logic.obj->flags &= ~(OBJ_FLAG_HAS_COLLISION | OBJ_FLAG_ENEMY);
	}

	// Returns JTRUE if the object is on the floor, or JFALSE is not on the floor or moving too fast.
	JBool actor_onFloor()
	{
		ActorLogic* logic = (ActorLogic*)s_actorState.curLogic;
		SecObject* obj = logic->logic.obj;
		RSector* sector = obj->sector;

		if (sector->secHeight > 0 && obj->posWS.y > sector->floorHeight)
		{
			return JTRUE;
		}
		fixed16_16 absVelX = TFE_Jedi::abs(logic->vel.x);
		fixed16_16 absVelZ = TFE_Jedi::abs(logic->vel.z);
		if (absVelX >= FIXED(5) && absVelZ >= FIXED(5))
		{
			return JFALSE;
		}

		fixed16_16 ceilHeight, floorHeight;
		sector_getObjFloorAndCeilHeight(sector, obj->posWS.y, &floorHeight, &ceilHeight);
		return (obj->posWS.y < floorHeight) ? JFALSE : JTRUE;
	}

	JBool actor_arrivedAtTarget(ActorTarget* target, SecObject* obj)
	{
		if (target->flags & 1)
		{
			if (target->pos.x != obj->posWS.x || target->pos.z != obj->posWS.z)
			{
				return JFALSE;
			}
		}
		if (target->flags & 2)
		{
			if (target->pos.y != obj->posWS.y)
			{
				return JFALSE;
			}
		}
		if (target->flags & 4)
		{
			if (target->yaw != obj->yaw || target->pitch != obj->pitch || target->roll == obj->roll)
			{
				return JFALSE;
			}
		}
		return JTRUE;
	}

	angle14_32 actor_offsetTarget(fixed16_16* targetX, fixed16_16* targetZ, fixed16_16 targetOffset, fixed16_16 targetVariation, angle14_32 angle, angle14_32 angleVariation)
	{
		if (angleVariation)
		{
			angle += floor16(random(intToFixed16(angleVariation) * 2) - intToFixed16(angleVariation));
		}
		angle &= ANGLE_MASK;

		fixed16_16 dist;
		if (targetVariation)
		{
			dist = targetOffset + random(targetVariation * 2) - targetVariation;
		}
		else
		{
			dist = targetOffset;
		}

		fixed16_16 cosAngle, sinAngle;
		sinCosFixed(angle, &sinAngle, &cosAngle);
		*targetX += mul16(dist, sinAngle);
		*targetZ += mul16(dist, cosAngle);
		return angle;
	}

	// returns JTRUE on collision else JFALSE.
	JBool actor_handleSteps(Actor* actor, ActorTarget* target)
	{
		SecObject* obj = actor->header.obj;
		if (actor->physics.responseStep || actor->collisionWall)
		{
			if (!(actor->collisionFlags & 1))
			{
				RWall* wall = actor->physics.wall ? actor->physics.wall : actor->collisionWall;
				if (!wall)
				{
					return actor->physics.collidedObj ? JTRUE : JFALSE;
				}

				RSector* nextSector = wall->nextSector;
				if (nextSector)
				{
					RSector* sector = wall->sector;
					fixed16_16 floorHeight = min(nextSector->floorHeight, sector->floorHeight);
					fixed16_16 ceilHeight  = max(nextSector->ceilingHeight, sector->ceilingHeight);
					fixed16_16 gap = floorHeight - ceilHeight;
					fixed16_16 objHeight = obj->worldHeight + ONE_16;
					// If the object can fit.
					if (gap > objHeight)
					{
						target->pos.y = floorHeight - (TFE_Jedi::abs(gap) >> 1) + (TFE_Jedi::abs(obj->worldHeight) >> 1);
						target->flags |= 2;
						return JFALSE;
					}
				}
			}
			return JTRUE;
		}
		return JFALSE;
	}

	JBool actorLogic_isStopFlagSet()
	{
		ActorLogic* logic = (ActorLogic*)s_actorState.curLogic;
		return (logic->flags & 8) ? JTRUE : JFALSE;
	}

	void actor_changeDirFromCollision(Actor* actor, ActorTarget* target, Tick* prevColTick)
	{
		SecObject* obj = actor->header.obj;
		Tick delta = Tick(s_curTick - (*prevColTick));
		angle14_32 newAngle;
		if (delta < 145)
		{
			newAngle = random_next() & ANGLE_MASK;
		}
		else
		{
			angle14_32 rAngle = actor->physics.responseAngle + (s_curTick & 0xff) - 128;
			angle14_32 angleDiff = getAngleDifference(obj->yaw, rAngle) & 8191;
			if (angleDiff > 4095)
			{
				newAngle = actor->physics.responseAngle - 8191;
			}
			else
			{
				newAngle = actor->physics.responseAngle;
			}
			newAngle &= ANGLE_MASK;
		}

		fixed16_16 dirX, dirZ;
		sinCosFixed(newAngle, &dirX, &dirZ);
		target->pos.x = obj->posWS.x + (dirX << 7);
		target->pos.z = obj->posWS.z + (dirZ << 7);

		target->flags = (target->flags | 5) & 0xfffffffd;
		target->yaw   = newAngle;
		target->pitch = 0;
		target->roll  = 0;

		*prevColTick = s_curTick;
	}

	void actor_jumpToTarget(PhysicsActor* physicsActor, SecObject* obj, vec3_fixed target, fixed16_16 speed, angle14_32 angleOffset)
	{
		fixed16_16 dist = distApprox(obj->posWS.x, obj->posWS.z, target.x, target.z);
		fixed16_16 distOverSpeed = div16(dist, speed);
		fixed16_16 impulse = mul16(s_gravityAccel, distOverSpeed >> 1);

		fixed16_16 dy = obj->posWS.y - target.y;
		angle14_32 pitch = vec2ToAngle(dy, dist);
		angle14_32 finalPitch = (angleOffset + pitch + arcCosFixed(div16(impulse, speed), pitch)) & ANGLE_MASK;

		// This simplifies to: (dy/dist + 1.0) * speed
		fixed16_16 dyOverDistBySpeed = div16(dy + dist, distOverSpeed);

		fixed16_16 sinYaw, cosYaw;
		fixed16_16 sinPitch, cosPitch;
		sinCosFixed(obj->yaw, &sinYaw, &cosYaw);
		sinCosFixed(finalPitch, &sinPitch, &cosPitch);

		physicsActor->vel.x = mul16(sinYaw, dyOverDistBySpeed);
		physicsActor->vel.z = mul16(cosYaw, dyOverDistBySpeed);
		physicsActor->vel.y = -mul16(sinPitch, dyOverDistBySpeed);

		physicsActor->vel.x = mul16(cosPitch, physicsActor->vel.x);
		physicsActor->vel.z = mul16(cosPitch, physicsActor->vel.z);
	}

	void actor_leadTarget(ProjectileLogic* proj)
	{
		SecObject* projObj = proj->logic.obj;

		fixed16_16 dist = distApprox(projObj->posWS.x, projObj->posWS.z, s_playerObject->posWS.x, s_playerObject->posWS.z);
		fixed16_16 travelTime = div16(dist, proj->speed);
		vec3_fixed playerVel;
		player_getVelocity(&playerVel);

		vec3_fixed target =
		{
			s_playerObject->posWS.x + mul16(playerVel.x, travelTime),
			s_playerObject->posWS.y + mul16(playerVel.y, travelTime) + ONE_16 - s_playerObject->worldHeight,
			s_playerObject->posWS.z + mul16(playerVel.z, travelTime)
		};
		proj_aimAtTarget(proj, target);
	}

	void actor_removeRandomCorpse()
	{
		if (s_sectorCount <= 0)
		{
			return;
		}
		s32 randomSect = floor16(random(intToFixed16(s_sectorCount - 1)));
		RSector* sector;
		for (u32 attempt = 0; attempt < s_sectorCount; attempt++, randomSect++)
		{
			if (randomSect >= (s32)s_sectorCount)
			{
				sector = &s_sectors[0];
				randomSect = 0;
			}
			else
			{
				sector = &s_sectors[randomSect];
			}

			SecObject** objList = sector->objectList;
			for (s32 i = 0, idx = 0; i < sector->objectCount && idx < sector->objectCapacity; idx++)
			{
				SecObject* obj = objList[idx];
				if (obj)
				{
					if ((obj->entityFlags & ETFLAG_CORPSE) && !(obj->entityFlags & ETFLAG_CAN_WAKE) && !actor_canSeeObject(obj, s_playerObject))
					{
						freeObject(obj);
						return;
					}
					i++;
				}
			}
		}
	}

	void actor_handleBossDeath(PhysicsActor* physicsActor)
	{
		SecObject* obj = physicsActor->actor.header.obj;
		if (obj->flags & OBJ_FLAG_BOSS)
		{
			if (obj->entityFlags & ETFLAG_GENERAL_MOHC)
			{
				if (s_mohcSector)
				{
					message_sendToSector(s_mohcSector, nullptr, 0, MSG_TRIGGER);
				}
			}
			else if (s_bossSector)
			{
				message_sendToSector(s_bossSector, nullptr, 0, MSG_TRIGGER);
			}
		}
	}

	void actor_updatePlayerVisiblity(JBool playerVis, fixed16_16 posX, fixed16_16 posZ)
	{
		ActorLogic* logic = (ActorLogic*)s_actorState.curLogic;

		// Update player visibility flag.
		logic->flags &= ~8;
		logic->flags |= ((playerVis & 1) << 3);	// flag 8 = player visible.

		if (playerVis)
		{
			logic->lastPlayerPos.x = posX;
			logic->lastPlayerPos.z = posZ;
		}
	}

	ActorLogic* actor_getCurrentLogic()
	{
		return (ActorLogic*)s_actorState.curLogic;
	}

	JBool defaultAiFunc(AiActor* aiActor, Actor* actor)
	{
		ActorEnemy* enemy = &aiActor->enemy;
		SecObject* obj = enemy->header.obj;
		RSector* sector = obj->sector;

		if (!(enemy->anim.flags & 2))
		{
			if (obj->type == OBJ_TYPE_SPRITE)
			{
				actor_setCurAnimation(&enemy->anim);
			}
			actor->updateTargetFunc(actor, &enemy->target);
			return JFALSE;
		}

		if (aiActor->hp <= 0)
		{
			if (!actor_onFloor())
			{
				if (obj->type == OBJ_TYPE_SPRITE)
				{
					actor_setCurAnimation(&enemy->anim);
				}
				actor->updateTargetFunc(actor, &enemy->target);
				return JFALSE;
			}
			spawnHitEffect(aiActor->dieEffect, sector, obj->posWS, obj);

			// If the secHeight is <= 0, then it is not a water sector.
			if (sector->secHeight - 1 < 0)
			{
				if (aiActor->itemDropId != ITEM_NONE)
				{
					SecObject* item = item_create(aiActor->itemDropId);
					item->posWS = obj->posWS;
					sector_addObject(sector, item);

					CollisionInfo colInfo =
					{
						item,	// obj
						FIXED(2), 0, FIXED(2),	// offset
						ONE_16, FIXED(9999), ONE_16, 0,	// botOffset, yPos, height, u1c
						nullptr, 0, nullptr,
						item->worldWidth, 0,
						JFALSE,
						0, 0, 0, 0, 0
					};
					handleCollision(&colInfo);

					fixed16_16 ceilHeight, floorHeight;
					sector_getObjFloorAndCeilHeight(item->sector, item->posWS.y, &floorHeight, &ceilHeight);
					item->posWS.y = floorHeight;

					// Free the object if it winds in a water sector.
					if (item->sector->secHeight - 1 >= 0)
					{
						freeObject(item);
					}
				}
				s32 animIndex = actor_getAnimationIndex(4);
				if (animIndex != -1)
				{
					RSector* sector = obj->sector;
					SecObject* corpse = allocateObject();
					sprite_setData(corpse, obj->wax);
					corpse->frame = 0;
					corpse->anim = animIndex;
					corpse->posWS.x = obj->posWS.x;
					corpse->posWS.y = (sector->colSecHeight < obj->posWS.y) ? sector->colSecHeight : obj->posWS.y;
					corpse->posWS.z = obj->posWS.z;
					corpse->worldHeight = 0;
					corpse->worldWidth = 0;
					corpse->entityFlags |= ETFLAG_CORPSE;
					corpse->flags &= ~OBJ_FLAG_HAS_COLLISION;
					sector_addObject(obj->sector, corpse);
				}
			}
			actor_kill();
			return 0;
		}
		return 0xffffffff;
	}

	JBool defaultMsgFunc(s32 msg, AiActor* aiActor, Actor* actor)
	{
		ActorEnemy* enemy = &aiActor->enemy;
		SecObject* obj = enemy->header.obj;
		RSector* sector = obj->sector;

		if (msg == MSG_DAMAGE)
		{
			if (aiActor->hp > 0)
			{
				ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
				fixed16_16 dmg = proj->dmg;
				// Reduce damage by half if an enemy is shooting another enemy.
				if (proj->prevObj)
				{
					u32 aiActorProj = proj->prevObj->entityFlags & ETFLAG_AI_ACTOR;
					u32 aiActorCur = obj->entityFlags & ETFLAG_AI_ACTOR;
					if (aiActorProj == aiActorCur)
					{
						dmg = proj->dmg >> 1;
					}
				}
				aiActor->hp -= dmg;
				if (aiActor->stopOnHit)
				{
					enemy->target.flags |= 8;
				}

				LogicAnimation* anim = &enemy->anim;
				vec3_fixed pushVel;
				computeDamagePushVelocity(proj, &pushVel);
				if (aiActor->hp <= 0)
				{
					ActorLogic* logic = (ActorLogic*)s_actorState.curLogic;
					actor_addVelocity(pushVel.x*4, pushVel.y*2, pushVel.z*4);
					actor_setDeathCollisionFlags();
					stopSound(logic->alertSndID);
					playSound3D_oneshot(aiActor->dieSndSrc, obj->posWS);
					enemy->anim.flags |= 8;
					if (proj->type == PROJ_PUNCH && obj->type == OBJ_TYPE_SPRITE)
					{
						actor_setupAnimation(2, anim);
					}
					else if (obj->type == OBJ_TYPE_SPRITE)
					{
						actor_setupAnimation(3, anim);
					}
				}
				else
				{
					// Keep the y velocity from accumulating forever.
					ActorLogic* logic = (ActorLogic*)s_actorState.curLogic;
					if (logic->vel.y < 0)
					{
						pushVel.y = 0;
					}

					actor_addVelocity(pushVel.x*2, pushVel.y, pushVel.z*2);
					stopSound(aiActor->hurtSndID);
					aiActor->hurtSndID = playSound3D_oneshot(aiActor->hurtSndSrc, obj->posWS);
					if (obj->type == OBJ_TYPE_SPRITE)
					{
						actor_setupAnimation(12, anim);
					}
				}
			}

			if (obj->type == OBJ_TYPE_SPRITE)
			{
				actor_setCurAnimation(&enemy->anim);
			}
			actor->updateTargetFunc(actor, &enemy->target);
			return JFALSE;
		}
		else if (msg == MSG_EXPLOSION)
		{
			if (aiActor->hp > 0)
			{
				fixed16_16 dmg   = s_msgArg1;
				fixed16_16 force = s_msgArg2;
				aiActor->hp -= dmg;
				if (aiActor->stopOnHit)
				{
					enemy->target.flags |= 8;
				}

				vec3_fixed dir;
				vec3_fixed pos = { obj->posWS.x, obj->posWS.y - obj->worldHeight, obj->posWS.z };
				computeExplosionPushDir(&pos, &dir);

				vec3_fixed vel = { mul16(force, dir.x), mul16(force, dir.y), mul16(force, dir.z) };
				LogicAnimation* anim = &enemy->anim;
				ActorLogic* logic = (ActorLogic*)s_actorState.curLogic;
				if (aiActor->hp <= 0)
				{
					actor_addVelocity(vel.x, vel.y, vel.z);
					actor_setDeathCollisionFlags();
					stopSound(logic->alertSndID);
					playSound3D_oneshot(aiActor->dieSndSrc, obj->posWS);
					enemy->target.flags |= 8;
					if (obj->type == OBJ_TYPE_SPRITE)
					{
						actor_setupAnimation(3, anim);
					}
				}
				else
				{
					actor_addVelocity(vel.x>>1, vel.y>>1, vel.z>>1);
					stopSound(aiActor->hurtSndID);
					aiActor->hurtSndID = playSound3D_oneshot(aiActor->hurtSndSrc, obj->posWS);
					if (obj->type == OBJ_TYPE_SPRITE)
					{
						actor_setupAnimation(12, anim);
					}
				}
			}
			if (obj->type == OBJ_TYPE_SPRITE)
			{
				actor_setCurAnimation(&enemy->anim);
			}
			actor->updateTargetFunc(actor, &enemy->target);
			return JFALSE;
		}
		else if (msg == MSG_TERMINAL_VEL)
		{
			// TODO
		}
		else if (msg == MSG_REV_MOVE)
		{
			// TODO
		}

		return JFALSE;
	}
	
	AiActor* actor_createAiActor(Logic* logic)
	{
		AiActor* enemyActor = (AiActor*)level_alloc(sizeof(AiActor));
		memset(enemyActor, 0, sizeof(AiActor));

		ActorEnemy* enemy = &enemyActor->enemy;
		actor_initEnemy(enemy, logic);
		enemy->header.func  = defaultAiFunc;
		enemy->header.msgFunc = defaultMsgFunc;
		enemy->header.nextTick = 0xffffffff;

		// Default values.
		enemyActor->hp = FIXED(4);
		enemyActor->itemDropId = ITEM_NONE;
		enemyActor->hurtSndSrc = NULL_SOUND;
		enemyActor->dieSndSrc  = NULL_SOUND;
		enemyActor->hurtSndID  = NULL_SOUND;
		enemyActor->stopOnHit  = JTRUE;
		enemyActor->dieEffect  = HEFFECT_NONE;

		return enemyActor;
	}
		
	JBool defaultEnemyFunc(AiActor* aiActor, Actor* actor)
	{
		ActorEnemy* enemy = &aiActor->enemy;
		ActorLogic* logic = (ActorLogic*)s_actorState.curLogic;
		SecObject* obj = enemy->header.obj;
		LogicAnimation* anim = &enemy->anim;
		s32 state = enemy->anim.state;

		switch (state)
		{
			case 0:
			{
				if (enemy->anim.flags & 2)
				{
					enemy->anim.flags &= ~8;
					enemy->anim.state = 1;
					if (enemy->state0NextTick < s_curTick)
					{
						if (enemy->fireSpread)
						{
							enemy->fireSpread -= mul16(FIXED(2), s_deltaTime);
							if (enemy->fireSpread < FIXED(9))
							{
								enemy->fireSpread = FIXED(9);
							}
							enemy->state0NextTick = s_curTick + 145;
						}
						else
						{
							enemy->state0NextTick = 0xffffffff;
						}
					}
					enemy->target.flags &= ~(1 | 2 | 3);
					// Next AI update.
					return s_curTick + random(enemy->timing.delay);
				}
			} break;
			case 1:
			{
				// actor_handleFightMusic();
				if (s_playerDying)
				{
					if (s_reviveTick <= s_curTick)
					{
						enemy->anim.flags |= 2;
						enemy->anim.state = 0;
						return enemy->timing.delay;
					}
				}

				if (!actor_canSeeObjFromDist(obj, s_playerObject))
				{
					actor_updatePlayerVisiblity(JFALSE, 0, 0);
					enemy->anim.flags |= 2;
					enemy->anim.state = 0;
					if (enemy->timing.nextTick < s_curTick)
					{
						enemy->timing.delay = enemy->timing.state0Delay;
						actor_setupInitAnimation();
					}
					return enemy->timing.delay;
				}
				else
				{
					actor_updatePlayerVisiblity(JTRUE, s_eyePos.x, s_eyePos.z);
					enemy->timing.nextTick = s_curTick + enemy->timing.state1Delay;
					fixed16_16 dist = distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, obj->posWS.x, obj->posWS.z);
					fixed16_16 yDiff = TFE_Jedi::abs(obj->posWS.y - obj->worldHeight - s_eyePos.y);
					angle14_32 vertAngle = vec2ToAngle(yDiff, dist);

					fixed16_16 baseYDiff = TFE_Jedi::abs(s_playerObject->posWS.y - obj->posWS.y);
					dist += baseYDiff;

					if (vertAngle < 2275 && dist <= enemy->maxDist)	// ~50 degrees
					{
						if (enemy->attackFlags & 1)
						{
							if (dist <= enemy->meleeRange)
							{
								enemy->anim.state = 2;
								enemy->timing.delay = enemy->timing.state2Delay;
							}
							else if (!(enemy->attackFlags & 2) || dist < enemy->minDist)
							{
								enemy->anim.flags |= 2;
								enemy->anim.state = 0;
								return enemy->timing.delay;
							}
							else
							{
								enemy->anim.state = 4;
								enemy->timing.delay = enemy->timing.state4Delay;
							}
						}
						else
						{
							if (dist < enemy->minDist)
							{
								enemy->anim.flags |= 2;
								enemy->anim.state = 0;
								return enemy->timing.delay;
							}
							enemy->anim.state = 2;
							enemy->timing.delay = enemy->timing.state4Delay;
						}

						if (obj->type == OBJ_TYPE_SPRITE)
						{
							if (enemy->anim.state == 2)
							{
								actor_setupAnimation(1, &enemy->anim);
							}
							else
							{
								actor_setupAnimation(7, &enemy->anim);
							}
						}

						enemy->target.pos.x = obj->posWS.x;
						enemy->target.pos.z = obj->posWS.z;
						enemy->target.yaw   = vec2ToAngle(s_eyePos.x - obj->posWS.x, s_eyePos.z - obj->posWS.z);
						enemy->target.pitch = obj->pitch;
						enemy->target.roll  = obj->roll;
						enemy->target.flags |= (1 | 4);
					}
					else
					{
						enemy->anim.flags |= 2;
						enemy->anim.state = 0;
						return enemy->timing.delay;
					}
				}
			} break;
			case 2:
			{
				if (!(enemy->anim.flags & 2))
				{
					break;
				}

				if (enemy->attackFlags & 1)	// Melee attack!
				{
					enemy->anim.state = 3;
					fixed16_16 dy = TFE_Jedi::abs(obj->posWS.y - s_playerObject->posWS.y);
					fixed16_16 dist = dy + distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, obj->posWS.x, obj->posWS.z);
					if (dist < enemy->meleeRange)
					{
						playSound3D_oneshot(enemy->attackSecSndSrc, obj->posWS);
						player_applyDamage(enemy->meleeDmg, 0, JTRUE);
						if (enemy->attackFlags & 8)
						{
							obj->flags |= OBJ_FLAG_FULLBRIGHT;
						}
					}
					break;
				}

				// Ranged Attack!
				if (enemy->attackFlags & 8)
				{
					obj->flags |= OBJ_FLAG_FULLBRIGHT;
				}

				enemy->anim.state = 3;
				ProjectileLogic* proj = (ProjectileLogic*)createProjectile(enemy->projType, obj->sector, obj->posWS.x, enemy->fireOffset.y + obj->posWS.y, obj->posWS.z, obj);
				playSound3D_oneshot(enemy->attackPrimSndSrc, obj->posWS);

				proj->prevColObj = obj;
				proj->prevObj = obj;

				SecObject* projObj = proj->logic.obj;
				projObj->yaw = obj->yaw;
				
				// Handle x and z fire offset.
				if (enemy->fireOffset.x | enemy->fireOffset.z)
				{
					proj->delta.x = enemy->fireOffset.x;
					proj->delta.z = enemy->fireOffset.z;
					proj_handleMovement(proj);
				}

				// Aim at the target.
				vec3_fixed target = { s_eyePos.x, s_eyePos.y + ONE_16, s_eyePos.z };
				proj_aimAtTarget(proj, target);

				if (enemy->fireSpread)
				{
					proj->vel.x += random(enemy->fireSpread*2) - enemy->fireSpread;
					proj->vel.y += random(enemy->fireSpread*2) - enemy->fireSpread;
					proj->vel.z += random(enemy->fireSpread*2) - enemy->fireSpread;
				}
			} break;
			case 3:
			{
				if (enemy->attackFlags & 8)
				{
					obj->flags &= ~OBJ_FLAG_FULLBRIGHT;
				}

				if (obj->type == OBJ_TYPE_SPRITE)
				{
					actor_setupAnimation(6, anim);
				}
				enemy->anim.state = 0;
			} break;
			case 4:
			{
				if (!(enemy->anim.flags & 2))
				{
					break;
				}
				if (enemy->attackFlags & 8)
				{
					obj->flags |= OBJ_FLAG_FULLBRIGHT;
				}

				enemy->anim.state = 5;
				ProjectileLogic* proj = (ProjectileLogic*)createProjectile(enemy->projType, obj->sector, obj->posWS.x, enemy->fireOffset.y + obj->posWS.y, obj->posWS.z, obj);
				playSound3D_oneshot(enemy->attackPrimSndSrc, obj->posWS);
				proj->prevColObj = obj;
				proj->excludeObj = obj;

				SecObject* projObj = proj->logic.obj;
				projObj->yaw = obj->yaw;
				if (enemy->projType == PROJ_THERMAL_DET)
				{
					proj->bounceCnt = 0;
					proj->duration = 0xffffffff;
					vec3_fixed target = { s_playerObject->posWS.x, s_eyePos.y + ONE_16, s_playerObject->posWS.z };
					proj_aimArcing(proj, target, proj->speed);

					if (enemy->fireOffset.x | enemy->fireOffset.z)
					{
						proj->delta.x = enemy->fireOffset.x;
						proj->delta.z = enemy->fireOffset.z;
						proj_handleMovement(proj);
					}
				}
				else
				{
					if (enemy->fireOffset.x | enemy->fireOffset.z)
					{
						proj->delta.x = enemy->fireOffset.x;
						proj->delta.z = enemy->fireOffset.z;
						proj_handleMovement(proj);
					}
					vec3_fixed target = { s_eyePos.x, s_eyePos.y + ONE_16, s_eyePos.z };
					proj_aimAtTarget(proj, target);
					if (enemy->fireSpread)
					{
						proj->vel.x += random(enemy->fireSpread*2) - enemy->fireSpread;
						proj->vel.y += random(enemy->fireSpread*2) - enemy->fireSpread;
						proj->vel.z += random(enemy->fireSpread*2) - enemy->fireSpread;
					}
				}
			} break;
			case 5:
			{
				if (obj->type == OBJ_TYPE_SPRITE)
				{
					actor_setupAnimation(8, anim);
				}
				enemy->anim.state = 0;
			} break;
		}

		if (obj->type == OBJ_TYPE_SPRITE)
		{
			actor_setCurAnimation(&enemy->anim);
		}
		actor->updateTargetFunc(actor, &enemy->target);
		return enemy->timing.delay;
	}

	JBool defaultEnemyMsgFunc(s32 msg, AiActor* aiActor, Actor* actor)
	{
		ActorEnemy* enemy = &aiActor->enemy;
		ActorLogic* logic = (ActorLogic*)s_actorState.curLogic;
		if (msg == MSG_WAKEUP)
		{
			enemy->timing.nextTick = s_curTick + enemy->timing.state1Delay;
		}
		if (logic->flags & 1)
		{
			return enemy->timing.delay;
		}
		return 0xffffffff;
	}

	ActorEnemy* actor_createEnemyActor(Logic* logic)
	{
		ActorEnemy* enemyActor = (ActorEnemy*)level_alloc(sizeof(ActorEnemy));
		memset(enemyActor, 0, sizeof(ActorEnemy));

		actor_initEnemy(enemyActor, logic);
		enemyActor->header.func = defaultEnemyFunc;
		enemyActor->header.msgFunc = defaultEnemyMsgFunc;
		return enemyActor;
	}

	JBool defaultSimpleActorFunc(AiActor* aiActor, Actor* actor)
	{
		ActorSimple* actorSimple = (ActorSimple*)aiActor;
		SecObject* obj = actorSimple->header.obj;

		if (actorSimple->anim.state == 1)
		{
			ActorTarget* target = &actorSimple->target;
			JBool arrivedAtTarget = actor_arrivedAtTarget(target, obj);
			if (actorSimple->nextTick < s_curTick || arrivedAtTarget)
			{
				if (arrivedAtTarget)
				{
					actorSimple->playerLastSeen = 0xffffffff;
				}
				actorSimple->anim.state = 2;
			}
			else
			{
				if (actorLogic_isStopFlagSet())
				{
					if (actorSimple->playerLastSeen != 0xffffffff)
					{
						actorSimple->nextTick = 0;
						actorSimple->delay = actorSimple->startDelay;
						actorSimple->playerLastSeen = 0xffffffff;
					}
				}
				else
				{
					actorSimple->playerLastSeen = s_curTick + 0x1111;
				}

				ActorTarget* target = &actorSimple->target;
				if (actor_handleSteps(actor, target))
				{
					actor_changeDirFromCollision(actor, target, &actorSimple->prevColTick);
					if (!actorLogic_isStopFlagSet())
					{
						actorSimple->delay += 218;
						if (actorSimple->delay > 1456)
						{
							actorSimple->delay = 291;
						}
						actorSimple->nextTick = s_curTick + actorSimple->delay;
					}
				}
			}
		}
		else if (actorSimple->anim.state == 2)
		{
			ActorLogic* logic = actor_getCurrentLogic();
			fixed16_16 targetX, targetZ;
			if (actorSimple->playerLastSeen < s_curTick)
			{
				targetX = logic->lastPlayerPos.x;
				targetZ = logic->lastPlayerPos.z;
			}
			else
			{
				targetX = s_eyePos.x;
				targetZ = s_eyePos.z;
			}

			fixed16_16 targetOffset;
			if (!actorLogic_isStopFlagSet())
			{
				// Offset the target by |dx| / 4
				fixed16_16 dx = TFE_Jedi::abs(s_playerObject->posWS.x - obj->posWS.x);
				// Note: there is a bug in the original DOS code -
				// it was supposed to used dx or dz, whichever is *larger* - but both sides of the conditional
				// use dx.
				targetOffset = dx >> 2;
			}
			else
			{
				// Offset the target by the targetOffset.
				targetOffset = actorSimple->targetOffset;
			}

			fixed16_16 dx = obj->posWS.x - targetX;
			fixed16_16 dz = obj->posWS.z - targetZ;
			angle14_32 angle = vec2ToAngle(dx, dz);

			actorSimple->target.pos.x = targetX;
			actorSimple->target.pos.z = targetZ;
			actorSimple->target.flags = (actorSimple->target.flags | 1) & 0xfffffffd;
			actor_offsetTarget(&actorSimple->target.pos.x, &actorSimple->target.pos.z, targetOffset, actorSimple->targetVariation, angle, actorSimple->approachVariation);

			dx = actorSimple->target.pos.x - obj->posWS.x;
			dz = actorSimple->target.pos.z - obj->posWS.z;
			actorSimple->target.pitch = 0;
			actorSimple->target.roll = 0;
			actorSimple->target.yaw = vec2ToAngle(dx, dz);
			actorSimple->target.flags |= 4;

			if (!(logic->flags & 2))
			{
				if (obj->type == OBJ_TYPE_SPRITE)
				{
					actor_setupAnimation(0, &actorSimple->anim);
				}
				logic->flags |= 2;
			}
			actorSimple->anim.state = 1;
			actorSimple->nextTick = s_curTick + actorSimple->delay;

			if (obj->entityFlags & ETFLAG_REMOTE)
			{
				if (!(logic->flags & 1) && (logic->flags & 2))
				{
					playSound3D_oneshot(s_agentSndSrc[AGENTSND_REMOTE_2], obj->posWS);
				}
			}
		}

		if (obj->type == OBJ_TYPE_SPRITE)
		{
			actor_setCurAnimation(&actorSimple->anim);
		}
		actor->updateTargetFunc(actor, &actorSimple->target);
		return 0;
	}

	ActorSimple* actor_createSimpleActor(Logic* logic)
	{
		ActorSimple* actor = (ActorSimple*)level_alloc(sizeof(ActorSimple));
		memset(actor, 0, sizeof(ActorSimple));

		actor->target.speedRotation = 0;
		actor->target.speed = FIXED(4);
		actor->target.speedVert = FIXED(10);
		actor->u3c = 72;
		actor->nextTick = 0;
		actor->playerLastSeen = 0xffffffff;
		actor->anim.state = 2;
		actor->delay = 728;	// ~5 seconds between decision points.
		actor->anim.frameRate = 5;
		actor->anim.frameCount = ONE_16;

		actor->anim.prevTick = 0;
		actor->prevColTick = 0;
		actor->target.flags = 0;
		actor->anim.flags = 0;

		actor_initHeader(&actor->header, logic);
		actor->header.func = defaultSimpleActorFunc;
		actor->targetOffset = FIXED(3);
		actor->targetVariation = 0;
		actor->approachVariation = 4096;	// 90 degrees.

		return actor;
	}

	void actor_setupInitAnimation()
	{
		ActorLogic* logic = (ActorLogic*)s_actorState.curLogic;
		logic->flags = (logic->flags | 1) & 0xfd;
		logic->nextTick = s_curTick + logic->delay;

		SecObject* obj = logic->logic.obj;
		obj->anim = actor_getAnimationIndex(5);
		obj->frame = 0;
	}

	void actorLogic_addActor(ActorLogic* logic, AiActor* aiActor, SubActorType type)
	{
		if (!aiActor) { return; }
		for (s32 i = 0; i < ACTOR_MAX_AI; i++)
		{
			if (!logic->aiActors[i])
			{
				logic->aiActors[i] = aiActor;
				logic->type[i] = type;
				return;
			}
		}
	}

	JBool actor_handleMovementAndCollision(Actor* actor)
	{
		SecObject* obj = actor->header.obj;
		vec3_fixed desiredMove = { 0, 0, 0 };
		vec3_fixed move = { 0, 0, 0 };

		actor->collisionWall = nullptr;
		if (!(actor->target.flags & 8))
		{
			if (actor->target.flags & 1)
			{
				desiredMove.x = actor->target.pos.x - obj->posWS.x;
				desiredMove.z = actor->target.pos.z - obj->posWS.z;
			}
			if (!(actor->collisionFlags & 1))
			{
				if (actor->target.flags & 2)
				{
					desiredMove.y = actor->target.pos.y - obj->posWS.y;
				}
			}
			move.z = move.y = move.x = 0;
			if (desiredMove.x | desiredMove.z)
			{
				fixed16_16 dirZ, dirX;
				fixed16_16 frameMove = mul16(actor->target.speed, s_deltaTime);
				computeDirAndLength(desiredMove.x, desiredMove.z, &dirX, &dirZ);

				if (desiredMove.x && dirX)
				{
					fixed16_16 deltaX = mul16(frameMove, dirX);
					fixed16_16 absDx = (desiredMove.x < 0) ? -desiredMove.x : desiredMove.x;
					move.x = clamp(deltaX, -absDx, absDx);
				}
				if (desiredMove.z && dirZ)
				{
					fixed16_16 deltaZ = mul16(frameMove, dirZ);
					fixed16_16 absDz = (desiredMove.z < 0) ? -desiredMove.z : desiredMove.z;
					move.z = clamp(deltaZ, -absDz, absDz);
				}
			}
			if (desiredMove.y)
			{
				fixed16_16 deltaY = mul16(actor->target.speedVert, s_deltaTime);
				if (desiredMove.y < 0) { deltaY = -deltaY; }

				fixed16_16 absDy = (desiredMove.y < 0) ? -desiredMove.y : desiredMove.y;
				move.y = clamp(deltaY, -absDy, absDy);
			}
		}

		if (actor->delta.y)
		{
			move.y += actor->delta.y;
		}
		CollisionInfo* physics = &actor->physics;
		fixed16_16 dirX, dirZ;
		if (move.x | move.y | move.z)
		{
			physics->offsetX = move.x;
			physics->offsetY = move.y;
			physics->offsetZ = move.z;
			handleCollision(physics);

			if (physics->responseStep)
			{
				RWall* wall = physics->wall;
				if (wall)
				{
					RSector* nextSector = wall->nextSector;
					RSector* triggerSector = (nextSector) ? nextSector : wall->sector;
					if (obj->entityFlags & ETFLAG_SMART_OBJ)
					{
						message_sendToSector(triggerSector, obj, 0, MSG_TRIGGER);
					}
				}
				// Handles a single collision response + resolution step.
				if (actor->collisionFlags & 4)
				{
					actor->collisionWall = wall;
					dirX = physics->responseDir.x;
					dirZ = physics->responseDir.z;
					handleCollisionResponseSimple(dirX, dirZ, &physics->offsetX, &physics->offsetZ);
					handleCollision(physics);
				}
			}
		}

		// Apply the per-frame delta computed from the actor's velocity.
		if (actor->delta.x | actor->delta.z)
		{
			physics->flags |= 1;
			physics->offsetX = actor->delta.x;
			physics->offsetY = 0;
			physics->offsetZ = actor->delta.z;
			handleCollision(physics);

			// Handles a single collision response + resolution step from velocity delta.
			if ((actor->collisionFlags & 4) && physics->responseStep)
			{
				actor->collisionWall = physics->wall;
				dirX = physics->responseDir.x;
				dirZ = physics->responseDir.z;
				handleCollisionResponseSimple(dirX, dirZ, &physics->offsetX, &physics->offsetZ);
				handleCollision(physics);
			}
		}

		return JTRUE;
	}

	angle14_32 computeAngleChange(angle14_32 desiredChange, angle14_32 maxChange)
	{
		if (!desiredChange) { return 0; }
		angle14_32 change = desiredChange;
		if (change > maxChange)
		{
			change = maxChange;
		}
		else
		{
			change = -maxChange;
			if (change <= desiredChange)
			{
				change = desiredChange;
			}
		}
		return change;
	}

	void actor_applyTransform(Actor* actor)
	{
		SecObject* obj = actor->header.obj;
		if (actor->target.flags & 8)
		{
			return;
		}

		const angle14_32 speedRotation = actor->target.speedRotation & 0xffff;
		if (speedRotation == 0)
		{
			obj->pitch = actor->target.pitch;
			obj->yaw   = actor->target.yaw;
			obj->roll  = actor->target.roll;
			if (obj->type & OBJ_TYPE_3D)
			{
				obj3d_computeTransform(obj);
			}
		}
		else
		{
			const angle14_32 pitchDiff = getAngleDifference(obj->pitch, actor->target.pitch);
			const angle14_32 yawDiff   = getAngleDifference(obj->yaw,   actor->target.yaw);
			const angle14_32 rollDiff  = getAngleDifference(obj->roll,  actor->target.roll);
			if (yawDiff | pitchDiff | rollDiff)
			{
				const fixed16_16 angularSpd = mul16(intToFixed16(speedRotation), s_deltaTime);
				const angle14_32 angleChange = floor16(angularSpd);
				obj->yaw = (obj->yaw + computeAngleChange(yawDiff, angleChange)) & ANGLE_MASK;

				if (obj->type & OBJ_TYPE_3D)
				{
					obj->pitch = (obj->pitch + computeAngleChange(pitchDiff, angleChange)) & ANGLE_MASK;
					obj->roll  = (obj->roll  + computeAngleChange(rollDiff,  angleChange)) & ANGLE_MASK;
					obj3d_computeTransform(obj);
				}
			}
		}
	}

	JBool defaultActorFunc(AiActor* aiActor, Actor* baseActor)
	{
		// This is really a regular actor...
		Actor* actor = (Actor*)aiActor;
		actor->physics.wall = nullptr;
		actor->physics.u24 = 0;

		if (actor->target.flags & 4)
		{
			actor_applyTransform(actor);
		}

		if ((actor->target.flags & 1) || (actor->target.flags & 2) || actor->delta.x || actor->delta.y || actor->delta.z)
		{
			actor_handleMovementAndCollision(actor);
		}
		actor->target.flags &= ~(1 | 2 | 4);
		return JFALSE;
	}

	// Updates the actor target with the passed in target based on the flags.
	JBool defaultUpdateTargetFunc(Actor* actor, ActorTarget* target)
	{
		if (target->flags & 8)
		{
			actor->target.flags |= 8;
		}
		else
		{
			actor->target.flags &= ~8;
			if (target->flags & 1)
			{
				actor->target.pos.x = target->pos.x;
				actor->target.pos.z = target->pos.z;
				actor->target.speed = target->speed;
				actor->target.flags |= 1;
			}
			if (target->flags & 2)
			{
				actor->target.pos.y = target->pos.y;
				actor->target.speedVert = target->speedVert;
				actor->target.flags |= 2;
			}
			if (target->flags & 4)
			{
				actor->target.pitch = target->pitch;
				actor->target.yaw   = target->yaw;
				actor->target.roll  = target->roll;
				actor->target.speedRotation = target->speedRotation;
				actor->target.flags |= 4;
			}
		}
		return JFALSE;
	}

	void actor_setupSmartObj(Actor* actor)
	{
		SecObject* obj = actor->header.obj;

		actor->physics.botOffset = 0x38000;	// 3.5
		actor->physics.yPos = FIXED(4);
		actor->physics.height = obj->worldHeight;
		actor->physics.width = obj->worldWidth;
		actor->physics.responseStep = JFALSE;
		actor->target.speed = ONE_16;
		actor->target.speedVert = ONE_16;
		actor->target.speedRotation = FIXED(45);
		actor->target.flags &= 0xf0;
		actor->delta = { 0, 0, 0 };
		actor->collisionWall = nullptr;
		actor->u9c = 0;

		actor->collisionFlags = (actor->collisionFlags | 3) & 0xfffffffb;
		obj->entityFlags |= ETFLAG_SMART_OBJ;
	}

	JBool actor_canSeeObject(SecObject* actorObj, SecObject* obj)
	{
		vec3_fixed p0 = { actorObj->posWS.x, actorObj->posWS.y - actorObj->worldHeight, actorObj->posWS.z };
		vec3_fixed p1 = { obj->posWS.x, obj->posWS.y, obj->posWS.z };
		if (collision_canHitObject(actorObj->sector, obj->sector, p0, p1, 0))
		{
			return JTRUE;
		}
		if (s_collision_wallHit)
		{
			return JFALSE;
		}

		vec3_fixed p2 = { obj->posWS.x, obj->posWS.y - obj->worldHeight, obj->posWS.z };
		return collision_canHitObject(actorObj->sector, obj->sector, p0, p2, 0);
	}
	   
	JBool actor_canSeeObjFromDist(SecObject* actorObj, SecObject* obj)
	{
		fixed16_16 approxDist = distApprox(actorObj->posWS.x, actorObj->posWS.z, obj->posWS.x, obj->posWS.z);
		// Crouching makes the target harder to see.
		if (obj->worldHeight < FIXED(3))
		{
			approxDist *= 2;
		}
		if (!s_headlampActive)
		{
			RSector* sector1 = obj->sector;
			fixed16_16 ambient = sector1->ambient;
			if (ambient > FIXED(22))
			{
				ambient = FIXED(31);
			}
			// Adds up to 248 units to the approximate distande: 248 when ambient = 0, 0 when ambient = 31
			approxDist += FIXED(248) - (ambient << 3);
		}
		else
		{
			approxDist -= s_baseAtten * 2;
		}

		if (approxDist < FIXED(256))
		{
			// Since random() is unsigned, the real visible range is [200, 256) because of the conditional above.
			fixed16_16 rndDist = random(FIXED(256)) + FIXED(200);
			if (approxDist < rndDist)
			{
				return actor_canSeeObject(actorObj, obj);
			}
		}
		return JFALSE;
	}

	JBool actor_isObjectVisible(SecObject* actorObj, SecObject* obj, angle14_32 fov, fixed16_16 closeDist)
	{
		fixed16_16 approxDist = distApprox(actorObj->posWS.x, actorObj->posWS.z, obj->posWS.x, obj->posWS.z);
		if (approxDist <= closeDist)
		{
			return actor_canSeeObjFromDist(actorObj, obj);
		}

		fixed16_16 dx = obj->posWS.x - actorObj->posWS.x;
		fixed16_16 dz = obj->posWS.z - actorObj->posWS.z;
		angle14_32 yaw0 = actorObj->yaw;
		angle14_32 obj0To1Angle = vec2ToAngle(dx, dz);
		angle14_32 angleDiff = getAngleDifference(obj0To1Angle, yaw0);
		angle14_32 right = fov >> 1;
		angle14_32 left = -(fov >> 1);
		if (angleDiff > left && angleDiff < right)
		{
			return actor_canSeeObjFromDist(actorObj, obj);
		}
		return JFALSE;
	}

	Actor* actor_create(Logic* logic)
	{
		Actor* actor = (Actor*)level_alloc(sizeof(Actor));
		memset(actor, 0, sizeof(Actor));

		actor_initHeader(&actor->header, logic);
		actor_setupSmartObj(actor);

		actor->header.func = defaultActorFunc;
		actor->header.freeFunc = nullptr;
		actor->updateTargetFunc = defaultUpdateTargetFunc;
		// Overwrites width even though it was set in actor_setupSmartObj()
		actor->physics.width = 0x18000;	// 1.5 units
		actor->physics.wall = nullptr;
		actor->physics.u24 = 0;
		actor->physics.obj = actor->header.obj;

		return actor;
	}

	void actorLogicCleanupFunc(Logic* logic)
	{
		ActorLogic* actorLogic = (ActorLogic*)logic;
		for (s32 i = 0; i < ACTOR_MAX_AI; i++)
		{
			AiActor* aiActor = actorLogic->aiActors[ACTOR_MAX_AI - 1 - i];
			if (aiActor)
			{
				ActorHeader* header = &aiActor->enemy.header;
				if (header->freeFunc)
				{
					header->freeFunc(header);
				}
				else
				{
					level_free(header);
				}
			}
		}

		if (actorLogic->freeTask)
		{
			s_msgEntity = actorLogic->logic.obj;
			task_runAndReturn(actorLogic->freeTask, MSG_FREE);
		}

		Actor* actor = actorLogic->actor;
		if (actor && actor->header.freeFunc)
		{
			actor->header.freeFunc(&actor->header);
		}
		deleteLogicAndObject((Logic*)actorLogic);
		allocator_deleteItem(s_istate.actorLogics, actorLogic);
	}
	
	JBool actor_advanceAnimation(LogicAnimation* anim, SecObject* obj)
	{
		if (!anim->prevTick)
		{
			anim->prevTick = s_frameTicks[anim->frameRate];
			anim->flags &= ~AFLAG_READY;

			obj->frame = floor16(anim->startFrame);
			anim->frame = anim->startFrame;
		}
		else
		{
			anim->frame += s_frameTicks[anim->frameRate] - anim->prevTick;
			anim->prevTick = s_frameTicks[anim->frameRate];

			fixed16_16 endFrame = anim->startFrame + anim->frameCount;
			if (anim->frame >= endFrame)
			{
				if (anim->flags & AFLAG_PLAYED)
				{
					endFrame -= ONE_16;
					anim->frame = endFrame;
					obj->frame = floor16(endFrame);
					return JTRUE;
				}

				while (anim->frame >= endFrame)
				{
					anim->frame -= anim->frameCount;
				}
			}
			obj->frame = floor16(anim->frame);
		}
		return JFALSE;
	}

	// Get the animation index for the current 'action' from s_curLogic.
	s32 actor_getAnimationIndex(s32 action)
	{
		ActorLogic* logic = (ActorLogic*)s_actorState.curLogic;
		SecObject* obj = logic->logic.obj;
		if ((obj->type & OBJ_TYPE_SPRITE) && logic->animTable)
		{
			return logic->animTable[action];
		}
		return -1;
	}

	void actor_setupAnimation(s32 animIdx, LogicAnimation* aiAnim)
	{
		ActorLogic* actorLogic = (ActorLogic*)s_actorState.curLogic;
		const s32* animTable = actorLogic->animTable;
		SecObject* obj = actorLogic->logic.obj;

		aiAnim->flags |= AFLAG_READY;
		if (obj->type & OBJ_TYPE_SPRITE)
		{
			aiAnim->prevTick = 0;
			aiAnim->animId = animTable[animIdx];
			aiAnim->startFrame = 0;
			aiAnim->frame = 0;
			if (aiAnim->animId != -1)
			{
				assert(aiAnim->animId < obj->wax->animCount);
				WaxAnim* anim = WAX_AnimPtr(obj->wax, aiAnim->animId);
				assert(anim);

				aiAnim->frameCount = intToFixed16(anim->frameCount);
				aiAnim->frameRate  = anim->frameRate;
				if (anim->frameRate >= 12)
				{
					aiAnim->frameRate = 12;
				}
				aiAnim->flags &= ~AFLAG_READY;
			}
		}
	}

	void actor_setupAnimation2(SecObject* obj, s32 animId, LogicAnimation* anim)
	{
		anim->flags |= 2;
		if (obj->type == OBJ_TYPE_SPRITE)
		{
			anim->prevTick = 0;
			anim->animId = animId;
			anim->startFrame = 0;
			if (animId != -1)
			{
				assert(animId < obj->wax->animCount);
				WaxAnim* waxAnim = WAX_AnimPtr(obj->wax, animId);
				assert(waxAnim);

				anim->frameCount = intToFixed16(waxAnim->frameCount);
				anim->frameRate = waxAnim->frameRate;
				if (anim->frameRate >= 12)
				{
					anim->frameRate = 12;
				}
				anim->flags &= ~2;
			}
		}
	}

	void actor_setAnimFrameRange(LogicAnimation* anim, s32 startFrame, s32 endFrame)
	{
		anim->frameCount = intToFixed16(endFrame - startFrame + 1);
		anim->startFrame = intToFixed16(startFrame);
	}

	void actor_setCurAnimation(LogicAnimation* aiAnim)
	{
		if (aiAnim->animId != -1)
		{
			s_actorState.curAnimation = aiAnim;
		}
	}

	JBool actor_canDie(PhysicsActor* phyActor)
	{
		SecObject* obj = phyActor->actor.header.obj;
		RSector* sector = obj->sector;
		if (sector->secHeight <= 0 && obj->posWS.y <= sector->floorHeight)
		{
			if (TFE_Jedi::abs(phyActor->vel.x) >= FIXED(5) || TFE_Jedi::abs(phyActor->vel.z) >= FIXED(5))
			{
				return JFALSE;
			}

			fixed16_16 ceilHeight, floorHeight;
			sector_getObjFloorAndCeilHeight(sector, obj->posWS.y, &floorHeight, &ceilHeight);
			if (obj->posWS.y < floorHeight)
			{
				return JFALSE;
			}
		}
		return JTRUE;
	}

	// Kill the actor, this clears the current logic so the rest of the task function proceeds correctly.
	void actor_kill()
	{
		actorLogicCleanupFunc(s_actorState.curLogic);
		s_actorState.curLogic = nullptr;
	}

	// Removes logics other than the current Logic.
	void actor_removeLogics(SecObject* obj)
	{
		Logic** logicList = (Logic**)allocator_getHead((Allocator*)obj->logic);
		while (logicList)
		{
			Logic* logic = *logicList;
			if (logic != s_actorState.curLogic)
			{
				if (logic->cleanupFunc) { logic->cleanupFunc(logic); }
			}
			logicList = (Logic**)allocator_getNext((Allocator*)obj->logic);
		};
	}

	void actor_addVelocity(fixed16_16 pushX, fixed16_16 pushY, fixed16_16 pushZ)
	{
		ActorLogic* actorLogic = (ActorLogic*)s_actorState.curLogic;
		actorLogic->vel.x += pushX;
		actorLogic->vel.y += pushY;
		actorLogic->vel.z += pushZ;
	}
	
	void actor_hitEffectMsgFunc(MessageType msg, void* logic)
	{
		ActorLogic* actorLogic = (ActorLogic*)logic;
		s_actorState.curLogic = (Logic*)logic;
		SecObject* obj = s_actorState.curLogic->obj;
		for (s32 i = 0; i < ACTOR_MAX_AI; i++)
		{
			AiActor* aiActor = actorLogic->aiActors[ACTOR_MAX_AI - 1 - i];
			if (aiActor)
			{
				ActorHeader* header = &aiActor->enemy.header;
				if (header->msgFunc)
				{
					Tick nextTick = header->msgFunc(msg, aiActor, actorLogic->actor);
					if (nextTick != 0xffffffff)
					{
						header->nextTick = nextTick;
					}
				}
			}
		}

		if (msg == MSG_WAKEUP)
		{
			if (actorLogic->flags & 1)
			{
				// imuse_triggerFightTrack();
			}

			if ((actorLogic->flags & 4) && (actorLogic->flags & 1))
			{
				if (s_actorState.nextAlertTick < s_curTick)
				{
					if (actorLogic->flags & 16)  // Officer alert list.
					{
						actorLogic->alertSndID = playSound3D_oneshot(s_officerAlertSndSrc[s_actorState.officerAlertIndex], obj->posWS);
						s_actorState.officerAlertIndex++;
						if (s_actorState.officerAlertIndex >= 4)
						{
							s_actorState.officerAlertIndex = 0;
						}
					}
					else if (actorLogic->flags & 32)  // Storm trooper alert list
					{
						actorLogic->alertSndID = playSound3D_oneshot(s_stormAlertSndSrc[s_actorState.stormtrooperAlertIndex], obj->posWS);
						s_actorState.stormtrooperAlertIndex++;
						if (s_actorState.stormtrooperAlertIndex >= 8)
						{
							s_actorState.stormtrooperAlertIndex = 0;
						}
					}
					else // Single alert.
					{
						actorLogic->alertSndID = playSound3D_oneshot(actorLogic->alertSndSrc, obj->posWS);
					}
					s_actorState.nextAlertTick = s_curTick + 291;	// ~2 seconds between alerts
				}
				actorLogic->flags &= 0xfffffffe;
			}
		}
		else if (msg == MSG_DAMAGE || msg == MSG_EXPLOSION)
		{
			if (actorLogic->flags & 1)
			{
				// imuse_triggerFightTrack();
			}
			actorLogic->flags &= ~1;
			s_actorState.curAnimation = nullptr;
		}
	}

	void actor_sendTerminalVelMsg(SecObject* obj)
	{
		message_sendToObj(obj, MSG_TERMINAL_VEL, actor_hitEffectMsgFunc);
	}

	void actor_handlePhysics(Actor* phyObj, vec3_fixed* vel)
	{
		SecObject* obj = phyObj->physics.obj;
		assert(obj && obj->sector);

		phyObj->delta = { 0, 0, 0 };
		if (vel->x) { phyObj->delta.x = mul16(vel->x, s_deltaTime); }
		if (vel->y) { phyObj->delta.y = mul16(vel->y, s_deltaTime); }
		if (vel->z) { phyObj->delta.z = mul16(vel->z, s_deltaTime); }

		fixed16_16 friction = ONE_16 - s_deltaTime*2;
		if (phyObj->collisionFlags & ACTORCOL_GRAVITY)
		{
			fixed16_16 ceilHeight, floorHeight;
			sector_getObjFloorAndCeilHeight(obj->sector, obj->posWS.y, &floorHeight, &ceilHeight);

			// Add gravity to our velocity if the object is not on the floor.
			if (obj->posWS.y < floorHeight)
			{
				vel->y += mul16(s_gravityAccel, s_deltaTime);
			}

			JBool vertCollision = JFALSE;
			JBool hitCeiling = JFALSE;
			if (vel->y < 0)
			{
				// Did the object hit the ceiling?
				if (obj->posWS.y <= ceilHeight + phyObj->physics.height)
				{
					vertCollision = JTRUE;
					hitCeiling = JTRUE;
				}
			}
			else if (obj->posWS.y >= floorHeight)
			{
				// Did it hit the floor?
				vertCollision = JTRUE;
				hitCeiling = JFALSE;
			}
			
			if (vertCollision)
			{
				// Collision with floor or ceiling.
				if (vel->y > FIXED(160))
				{
					actor_sendTerminalVelMsg(obj);
				}

				// Clear out the yVel if the object hits the floor or ceiling.
				if ((hitCeiling && vel->y < 0) || (!hitCeiling && vel->y >= 0))
				{
					vel->y = 0;
				}
			}
		}
		else
		{
			vel->y = mul16(vel->y, friction);
		}
		vel->x = mul16(vel->x, friction);
		vel->z = mul16(vel->z, friction);

		if (TFE_Jedi::abs(vel->x) < ACTOR_MIN_VELOCITY) { vel->x = 0; }
		if (TFE_Jedi::abs(vel->y) < ACTOR_MIN_VELOCITY) { vel->y = 0; }
		if (TFE_Jedi::abs(vel->z) < ACTOR_MIN_VELOCITY) { vel->z = 0; }
	}

	void actorLogicMsgFunc(MessageType msg)
	{
		if (msg == MSG_FREE)
		{
			actorLogicCleanupFunc((Logic*)s_msgTarget);
		}
		else
		{
			actor_hitEffectMsgFunc(msg, s_msgTarget);
		}
	}

	void actorLogicTaskFunc(MessageType msg)
	{
		task_begin;
		while (msg != MSG_FREE_TASK)
		{
			if (s_istate.objCollisionEnabled)
			{
				task_yield(TASK_NO_DELAY);
			}
			if (!s_istate.objCollisionEnabled)
			{
				task_yield(0x123);
			}

			if (msg == MSG_RUN_TASK)
			{
				ActorLogic* actorLogic = (ActorLogic*)allocator_getHead(s_istate.actorLogics);
				while (actorLogic)
				{
					SecObject* obj = actorLogic->logic.obj;
					u32 flags = actorLogic->flags;
					if ((flags & 1) && (flags & 4))
					{
						if (actorLogic->nextTick < s_curTick)
						{
							actorLogic->nextTick = s_curTick + actorLogic->delay;
							if (actor_isObjectVisible(obj, s_playerObject, actorLogic->fov, actorLogic->nearDist))
							{
								message_sendToObj(obj, MSG_WAKEUP, actor_hitEffectMsgFunc);
								// imuse_triggerFightTrack();
								collision_effectObjectsInRangeXZ(obj->sector, FIXED(150), obj->posWS, hitEffectWakeupFunc, obj, ETFLAG_AI_ACTOR);
							}
						}
					}
					else
					{
						s_actorState.curLogic = (Logic*)actorLogic;
						s_actorState.curAnimation = nullptr;
						for (s32 i = 0; i < ACTOR_MAX_AI; i++)
						{
							AiActor* aiActor = actorLogic->aiActors[ACTOR_MAX_AI - 1 - i];
							if (aiActor)
							{
								ActorHeader* header = &aiActor->enemy.header;
								if (header->func && header->nextTick < s_curTick)
								{
									header->nextTick = header->func(aiActor, actorLogic->actor);
								}
							}
						}

						if (s_actorState.curLogic && !(actorLogic->flags & 1))
						{
							Actor* actor = actorLogic->actor;
							if (actor)
							{
								ActorFunc func = actor->header.func;
								if (func)
								{
									actor_handlePhysics(actor, &actorLogic->vel);
									func((AiActor*)actor, actor);
								}
							}

							if ((obj->type & OBJ_TYPE_SPRITE) && s_actorState.curAnimation)
							{
								obj->anim = s_actorState.curAnimation->animId;
								if (actor_advanceAnimation(s_actorState.curAnimation, obj))
								{
									// The animation has finished.
									s_actorState.curAnimation->flags |= AFLAG_READY;
								}
							}
						}
					}

					actorLogic = (ActorLogic*)allocator_getNext(s_istate.actorLogics);
				}
			}
		}
		task_end;
	}

	void actorPhysicsTaskFunc(MessageType msg)
	{
		task_begin;
		while (msg != MSG_FREE_TASK)
		{
			task_localBlockBegin;
			PhysicsActor** phyObjPtr = (PhysicsActor**)list_getHead(s_physicsActors);
			while (phyObjPtr)
			{
				PhysicsActor* phyObj = *phyObjPtr;
				phyObj->actor.physics.wall = nullptr;
				phyObj->actor.physics.u24 = 0;
				if (phyObj->actor.target.flags & 4)
				{
					actor_applyTransform(&phyObj->actor);
				}
				actor_handlePhysics(&phyObj->actor, &phyObj->vel);

				if ((phyObj->actor.target.flags & 1) || (phyObj->actor.target.flags & 2) || phyObj->vel.x || phyObj->vel.y || phyObj->vel.z)
				{
					actor_handleMovementAndCollision(&phyObj->actor);
					CollisionInfo* physics = &phyObj->actor.physics;
					if (physics->responseStep)
					{
						handleCollisionResponseSimple(physics->responseDir.x, physics->responseDir.z, &phyObj->vel.x, &phyObj->vel.z);
					}
				}

				SecObject* obj = phyObj->actor.header.obj;
				LogicAnimation* anim = &phyObj->anim;
				if (obj->type & OBJ_TYPE_SPRITE)
				{
					if (!(anim->flags & 2))
					{
						obj->anim = anim->animId;
						if (actor_advanceAnimation(anim, obj))
						{
							anim->flags |= AFLAG_READY;
						}
					}
				}

				phyObjPtr = (PhysicsActor**)list_getNext(s_physicsActors);
			}
			task_localBlockEnd;
			task_yield(TASK_NO_DELAY);
		}
		task_end;
	}
}  // namespace TFE_DarkForces