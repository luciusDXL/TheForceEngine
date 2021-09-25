#include "actor.h"
#include "../logic.h"
#include "aiActor.h"
#include <TFE_Game/igame.h>
#include <TFE_DarkForces/random.h>
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
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
	// Constants
	///////////////////////////////////////////
	enum ActorAlert
	{
		ALERT_GAMOR = 0,
		ALERT_REEYEE,	
		ALERT_BOSSK,	
		ALERT_CREATURE,	
		ALERT_PROBE,	
		ALERT_INTDROID,	
		ALERT_COUNT
	};
	enum AgentActionSounds
	{
		AGENTSND_REMOTE_2 = 0,
		AGENTSND_AXE_1,			
		AGENTSND_INTSTUN,	
		AGENTSND_PROBFIRE_11,
		AGENTSND_PROBFIRE_12,
		AGENTSND_CREATURE2,	
		AGENTSND_PROBFIRE_13,
		AGENTSND_STORM_HURT,
		AGENTSND_GAMOR_2,	
		AGENTSND_REEYEE_2,	
		AGENTSND_BOSSK_3,	
		AGENTSND_CREATURE_HURT,
		AGENTSND_STORM_DIE,	
		AGENTSND_REEYEE_3,	
		AGENTSND_BOSSK_DIE,	
		AGENTSND_GAMOR_1,	
		AGENTSND_CREATURE_DIE,
		AGENTSND_EEEK_3,	
		AGENTSND_SMALL_EXPLOSION,
		AGENTSND_PROBE_ALM,
		AGENTSND_TINY_EXPLOSION,
		AGENTSND_COUNT
	};

	enum
	{
		OFFICER_ALERT_COUNT = 4,
		STORM_ALERT_COUNT = 8,
		ACTOR_MIN_VELOCITY = 0x1999,	// < 0.1
	};

	///////////////////////////////////////////
	// Internal State
	///////////////////////////////////////////
	static SoundSourceID s_alertSndSrc[ALERT_COUNT];
	static SoundSourceID s_officerAlertSndSrc[OFFICER_ALERT_COUNT];
	static SoundSourceID s_stormAlertSndSrc[STORM_ALERT_COUNT];
	static SoundSourceID s_agentSndSrc[AGENTSND_COUNT];
	static List* s_physicsActors;
		
	static Allocator* s_actorLogics = nullptr;
	static Task* s_actorTask = nullptr;
	static Task* s_actorPhysicsTask = nullptr;

	static JBool s_objCollisionEnabled = JTRUE;
	
	LogicAnimation* s_curAnimation = nullptr;
	Logic* s_curLogic = nullptr;

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
		actor->ud0  = 0;
		actor->parent = actorPtr;
	}

	void actor_removePhysicsActorFromWorld(PhysicsActor* phyActor)
	{
		list_removeItem(s_physicsActors, phyActor->parent);
	}
		
	void actor_createTask()
	{
		s_actorLogics = allocator_create(sizeof(ActorLogic));
		s_actorTask = createSubTask("actor", actorLogicTaskFunc, actorLogicMsgFunc);
		s_actorPhysicsTask = createSubTask("physics", actorPhysicsTaskFunc);
	}

	ActorLogic* actor_setupActorLogic(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorLogic* logic = (ActorLogic*)allocator_newItem(s_actorLogics);
		memset(logic->aiActors, 0, sizeof(AiActor*) * 6);

		logic->actor = nullptr;
		logic->animTable = nullptr;
		logic->delay = 72;			// Delay in ticks
		logic->alertSndSrc = 0;
		logic->u44 = 0;
		logic->u48 = 0x2555;		// 0.1458?
		logic->u4c = FIXED(20);		// possibly movement speed?
		logic->vel = { 0, 0, 0 };
		logic->freeTask = nullptr;
		logic->flags = 4;

		obj_addLogic(obj, (Logic*)logic, s_actorTask, actorLogicCleanupFunc);
		if (setupFunc)
		{
			*setupFunc = actorLogicSetupFunc;
		}

		s_curLogic = (Logic*)logic;
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

	void gameObj_Init(ActorHeader* gameObj, Logic* logic)
	{
		gameObj->func = nullptr;
		gameObj->msgFunc = nullptr;
		gameObj->u08 = 0;
		gameObj->freeFunc = nullptr;
		gameObj->nextTick = 0;
		gameObj->obj = logic->obj;
	}

	fixed16_16 gameObj_InitEnemy(GameObject2* gameObj, Logic* logic)
	{
		gameObj_Init((ActorHeader*)gameObj, logic);

		gameObj->width = 0;
		gameObj->flags = 0;
		gameObj->u30 = 0;
		gameObj->u3c = 218;
		gameObj->u40 = 218;
		gameObj->u44 = 0;
		gameObj->height = 291;
		gameObj->u4c = 43695;	// 0.6667
		gameObj->u54 = 5;
		gameObj->u58 = ONE_16;
		gameObj->u5c = 0;
		gameObj->u70 = 0;
		gameObj->u74 = FIXED(30);
		gameObj->u78 = 0;
		gameObj->u7c = 0;

		// u38 and u68 are not set anywhere, so these should probably be equal.
		gameObj->u38 &= 0xf0;
		gameObj->u68 |= 3;
		gameObj->u84 = 0;
		gameObj->nextTick = s_curTick + 0x4446;	// ~120 seconds

		SecObject* obj = gameObj->header.obj;
		// world width and height was set from the sprite data.
		gameObj->centerOffset = -((TFE_Jedi::abs(obj->worldHeight) >> 1) + ONE_16);

		gameObj->u88 = 2;
		gameObj->u8c = 0;
		gameObj->u90 = 0;
		gameObj->u94 = 0;
		gameObj->u98 = 0;
		gameObj->u9c = FIXED(160);
		gameObj->ua0 = 0;
		gameObj->ua4 = FIXED(230);
		gameObj->ua8 = 10;

		return gameObj->centerOffset;
	}

	JBool defaultAiFunc(AiActor* aiActor, Actor* actor)
	{
		return JFALSE;
	}

	JBool defaultMsgFunc(s32 msg, AiActor* aiActor, Actor* actor)
	{
		return JFALSE;
	}
	
	AiActor* actor_createAiActor(Logic* logic)
	{
		AiActor* enemy = (AiActor*)level_alloc(sizeof(AiActor));
		gameObj_InitEnemy((GameObject2*)enemy, logic);
		enemy->actor.header.func  = defaultAiFunc;
		enemy->actor.header.msgFunc = defaultMsgFunc;
		enemy->actor.header.nextTick = 0xffffffff;

		enemy->hp = FIXED(4);	// default to 4 HP.
		enemy->itemDropId = -1;
		enemy->hurtSndSrc = 0;
		enemy->dieSndSrc = 0;
		enemy->ubc = 0;
		enemy->uc0 = -1;
		enemy->uc4 = -1;

		return enemy;
	}

	void actor_addLogicGameObj(ActorLogic* logic, AiActor* aiActor)
	{
		if (!aiActor) { return; }
		for (s32 i = 0; i < ACTOR_MAX_AI; i++)
		{
			if (!logic->aiActors[i])
			{
				logic->aiActors[i] = aiActor;
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
		if (!(actor->updateFlags & 8))
		{
			if (actor->updateFlags & 1)
			{
				desiredMove.x = actor->nextPos.x - obj->posWS.x;
				desiredMove.z = actor->nextPos.z - obj->posWS.z;
			}
			if (!(actor->collisionFlags & 1))
			{
				if (actor->updateFlags & 2)
				{
					desiredMove.y = actor->nextPos.y - obj->posWS.y;
				}
			}
			move.z = move.y = move.x = 0;
			if (desiredMove.x | desiredMove.z)
			{
				fixed16_16 dirZ, dirX;
				fixed16_16 frameMove = mul16(actor->speed, s_deltaTime);
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
				fixed16_16 deltaY = mul16(actor->speedVert, s_deltaTime);
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
		if (actor->updateFlags & 8)
		{
			return;
		}

		const angle14_32 speedRotation = actor->speedRotation & 0xffff;
		if (speedRotation == 0)
		{
			obj->pitch = actor->pitch;
			obj->yaw   = actor->yaw;
			obj->roll  = actor->roll;
			if (obj->type & OBJ_TYPE_3D)
			{
				obj3d_computeTransform(obj);
			}
		}
		else
		{
			const angle14_32 pitchDiff = getAngleDifference(obj->pitch, actor->pitch);
			const angle14_32 yawDiff   = getAngleDifference(obj->yaw,   actor->yaw);
			const angle14_32 rollDiff  = getAngleDifference(obj->roll,  actor->roll);
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
		Actor* actor = &aiActor->actor;
		actor->physics.wall = nullptr;
		actor->physics.u24 = 0;

		if (actor->updateFlags & 4)
		{
			actor_applyTransform(actor);
		}

		if ((actor->updateFlags & 1) || (actor->updateFlags & 2) || actor->delta.x || actor->delta.y || actor->delta.z)
		{
			actor_handleMovementAndCollision(actor);
		}
		actor->updateFlags &= ~(1 | 2 | 4);
		return JFALSE;
	}

	JBool defaultActorMsgFunc(AiActor* aiActor, Actor* baseActor)
	{
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
		actor->speed = ONE_16;
		actor->speedVert = ONE_16;
		actor->speedRotation = FIXED(45);
		actor->updateFlags &= 0xf0;
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
		gameObj_Init((ActorHeader*)actor, logic);
		actor_setupSmartObj(actor);

		actor->header.func = defaultActorFunc;
		actor->header.freeFunc = nullptr;
		actor->func3 = defaultActorMsgFunc;
		// Overwrites height even though it was set in actor_setupSmartObj()
		actor->physics.height = 0x18000;	// 1.5 units
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
				ActorHeader* header = &aiActor->actor.header;
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
		allocator_deleteItem(s_actorLogics, actorLogic);
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
		ActorLogic* logic = (ActorLogic*)s_curLogic;
		SecObject* obj = logic->logic.obj;
		if ((obj->type & OBJ_TYPE_SPRITE) && logic->animTable)
		{
			return logic->animTable[action];
		}
		return -1;
	}

	void actor_setupAnimation(s32 animIdx, LogicAnimation* aiAnim)
	{
		ActorLogic* actorLogic = (ActorLogic*)s_curLogic;
		const s32* animTable = actorLogic->animTable;
		SecObject* obj = actorLogic->logic.obj;

		aiAnim->flags |= AFLAG_READY;
		if (obj->type & OBJ_TYPE_SPRITE)
		{
			aiAnim->prevTick = 0;
			aiAnim->animId = animTable[animIdx];
			aiAnim->startFrame = 0;
			aiAnim->flags |= AFLAG_PLAYED;
			if (aiAnim->animId != -1)
			{
				WaxAnim* anim = WAX_AnimPtr(obj->wax, aiAnim->animId);

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

	void actor_setCurAnimation(LogicAnimation* aiAnim)
	{
		if (aiAnim->animId != -1)
		{
			s_curAnimation = aiAnim;
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
		actorLogicCleanupFunc(s_curLogic);
		s_curLogic = nullptr;
	}

	// Removes logics other than the current Logic.
	void actor_removeLogics(SecObject* obj)
	{
		Logic** logicList = (Logic**)allocator_getHead((Allocator*)obj->logic);
		while (logicList)
		{
			Logic* logic = *logicList;
			if (logic != s_curLogic)
			{
				if (logic->cleanupFunc) { logic->cleanupFunc(logic); }
			}
			logicList = (Logic**)allocator_getNext((Allocator*)obj->logic);
		};
	}

	void actor_addVelocity(fixed16_16 pushX, fixed16_16 pushY, fixed16_16 pushZ)
	{
		ActorLogic* actorLogic = (ActorLogic*)s_curLogic;
		actorLogic->vel.x += pushX;
		actorLogic->vel.y += pushY;
		actorLogic->vel.z += pushZ;
	}

	// Actor function for exploders (i.e. landmines and exploding barrels).
	JBool exploderFunc(AiActor* aiActor, Actor* actor)
	{
		if (!(aiActor->anim.flags & AFLAG_READY))
		{
			s_curAnimation = &aiActor->anim;
			return JFALSE;
		}
		else if ((aiActor->anim.flags & AFLAG_PLAYED) && aiActor->hp <= 0)
		{
			actor_kill();
			return JFALSE;
		}
		return JTRUE;
	}

	// Actor message function for exploders, this is responsible for processing messages such as 
	// projectile damage and explosions. For other AI message functions, it would also process
	// "wake up" messages, but those messages are ignored for exploders.
	JBool exploderMsgFunc(s32 msg, AiActor* aiActor, Actor* actor)
	{
		JBool retValue = JFALSE;
		SecObject* obj = aiActor->actor.header.obj;

		if (msg == MSG_DAMAGE)
		{
			if (aiActor->hp > 0)
			{
				ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
				aiActor->hp -= proj->dmg;
				JBool retValue;
				if (aiActor->hp > 0)
				{
					retValue = JTRUE;
				}
				else
				{
					ProjectileLogic* proj = (ProjectileLogic*)createProjectile(PROJ_EXP_BARREL, obj->sector, obj->posWS.x, obj->posWS.y, obj->posWS.z, obj);
					proj->vel = { 0, 0, 0 };
					obj->flags |= OBJ_FLAG_FULLBRIGHT;

					// I have to remove the logics here in order to get this to work, but this doesn't actually happen here in the original code.
					// TODO: Move to the correct location.
					actor_removeLogics(obj);

					actor_setupAnimation(2/*animIndex*/, &aiActor->anim);
					//actor_setCurAnimation(&aiActor->anim);
					//aiActor->actor.func3(actor, aiActor->actor.func3);

					retValue = JFALSE;
				}
			}
		}
		else if (msg == MSG_EXPLOSION)
		{
			if (aiActor->hp <= 0)
			{
				return JTRUE;
			}

			fixed16_16 dmg = s_msgArg1;
			fixed16_16 force = s_msgArg2;
			aiActor->hp -= dmg;

			vec3_fixed pushDir;
			vec3_fixed pos = { obj->posWS.x, obj->posWS.y - obj->worldHeight, obj->posWS.z };
			computeExplosionPushDir(&pos, &pushDir);

			fixed16_16 pushX = mul16(force, pushDir.x);
			fixed16_16 pushZ = mul16(force, pushDir.z);
			if (aiActor->hp > 0)
			{
				actor_addVelocity(pushX >> 1, 0, pushZ >> 1);
				return JTRUE;
			}

			obj->flags |= OBJ_FLAG_FULLBRIGHT;
			actor_addVelocity(pushX, 0, pushZ);

			ProjectileLogic* proj = (ProjectileLogic*)createProjectile(PROJ_EXP_BARREL, obj->sector, obj->posWS.x, obj->posWS.y, obj->posWS.z, obj);
			proj->vel = { 0, 0, 0 };

			ProjectileLogic* proj2 = (ProjectileLogic*)createProjectile(PROJ_EXP_BARREL, obj->sector, obj->posWS.x, obj->posWS.y, obj->posWS.z, obj);
			proj2->vel = { 0, 0, 0 };
			proj2->hitEffectId = HEFFECT_EXP_INVIS;
			proj2->duration = s_curTick + 36;

			ActorLogic* actorLogic = (ActorLogic*)s_curLogic;
			CollisionInfo colInfo = { 0 };
			colInfo.obj = proj2->logic.obj;
			colInfo.offsetY = 0;
			colInfo.offsetX = mul16(actorLogic->vel.x, 0x4000);
			colInfo.offsetZ = mul16(actorLogic->vel.z, 0x4000);
			colInfo.botOffset = ONE_16;
			colInfo.yPos = FIXED(9999);
			colInfo.height = ONE_16;
			colInfo.u1c = 0;
			colInfo.width = colInfo.obj->worldWidth;
			handleCollision(&colInfo);

			// I have to remove the logics here in order to get this to work, but this doesn't actually happen here in the original code.
			// TODO: Move to the correct location.
			actor_removeLogics(obj);
			actor_setupAnimation(2/*animIndex*/, &aiActor->anim);
			retValue = JFALSE;
		}
		return retValue;
	}

	void actor_hitEffectMsgFunc(s32 msg, void* logic)
	{
		ActorLogic* actorLogic = (ActorLogic*)logic;
		s_curLogic = (Logic*)logic;
		SecObject* obj = s_curLogic->obj;
		for (s32 i = 0; i < ACTOR_MAX_AI; i++)
		{
			AiActor* aiActor = actorLogic->aiActors[ACTOR_MAX_AI - 1 - i];
			if (aiActor)
			{
				ActorHeader* header = &aiActor->actor.header;
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
			if (floorHeight > obj->posWS.y)
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
			if (s_objCollisionEnabled)
			{
				task_yield(TASK_NO_DELAY);
			}
			if (!s_objCollisionEnabled)
			{
				task_yield(0x123);
			}

			if (msg == MSG_RUN_TASK)
			{
				ActorLogic* actorLogic = (ActorLogic*)allocator_getHead(s_actorLogics);
				while (actorLogic)
				{
					SecObject* obj = actorLogic->logic.obj;
					u32 flags = actorLogic->flags;
					if ((flags & 1) && (flags & 0xf0))
					{
						if (actorLogic->nextTick < s_curTick)
						{
							// TODO	
						}
					}
					else
					{
						s_curLogic = (Logic*)actorLogic;
						s_curAnimation = nullptr;
						for (s32 i = 0; i < ACTOR_MAX_AI; i++)
						{
							AiActor* aiActor = actorLogic->aiActors[ACTOR_MAX_AI - 1 - i];
							if (aiActor)
							{
								ActorHeader* header = &aiActor->actor.header;
								if (header->func && header->nextTick < s_curTick)
								{
									header->nextTick = header->func(aiActor, actorLogic->actor);
								}
							}
						}

						if (s_curLogic && !(actorLogic->flags & 1))
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

							if ((obj->type & OBJ_TYPE_SPRITE) && s_curAnimation)
							{
								obj->anim = s_curAnimation->animId;
								if (actor_advanceAnimation(s_curAnimation, obj))
								{
									// The animation has finished.
									s_curAnimation->flags |= AFLAG_READY;
								}
							}
						}
					}

					actorLogic = (ActorLogic*)allocator_getNext(s_actorLogics);
				}
			}

			task_yield(TASK_NO_DELAY);
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
				if (phyObj->actor.updateFlags & 4)
				{
					actor_applyTransform(&phyObj->actor);
				}
				actor_handlePhysics(&phyObj->actor, &phyObj->vel);

				if ((phyObj->actor.updateFlags & 1) || (phyObj->actor.updateFlags & 2) || phyObj->vel.x || phyObj->vel.y || phyObj->vel.z)
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
							anim->flags |= 2;
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