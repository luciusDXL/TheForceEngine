#include "actor.h"
#include "../logic.h"
#include "aiActor.h"
#include <TFE_Game/igame.h>
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/hitEffect.h>
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

	Logic* s_curLogic = nullptr;

	///////////////////////////////////////////
	// Forward Declarations
	///////////////////////////////////////////
	void actorLogicTaskFunc(s32 id);
	void actorPhysicsTaskFunc(s32 id);
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
		s_physicsActors = list_allocate(sizeof(void*), 80);
	}
		
	void actor_createTask()
	{
		s_actorLogics = allocator_create(sizeof(ActorLogic));
		s_actorTask = createTask("actor", actorLogicTaskFunc);
		s_actorPhysicsTask = createTask("physics", actorPhysicsTaskFunc);
	}

	ActorLogic* actor_setupActorLogic(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorLogic* logic = (ActorLogic*)allocator_newItem(s_actorLogics);
		memset(logic->gameObj, 0, sizeof(ActorHeader*) * 6);

		logic->actor = nullptr;
		logic->animTable = nullptr;
		logic->delay = 72;			// Delay in ticks
		logic->alertSndSrc = 0;
		logic->u44 = 0;
		logic->u48 = 0x2555;		// 0.1458?
		logic->u4c = FIXED(20);		// possibly movement speed?
		logic->vel = { 0, 0, 0 };
		logic->u64 = 0;
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
				for (s32 i = 0; i < 6; i++)
				{
					if (!actorLogic->gameObj[i])
					{
						actorLogic->gameObj[i] = (ActorHeader*)res;
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
		gameObj->func2 = nullptr;
		gameObj->u08 = 0;
		gameObj->freeFunc = free;
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

	JBool defaultAiFunc(Actor* actor0, Actor* actor1)
	{
		return JFALSE;
	}
	
	AiActor* actor_createAiActor(Logic* logic)
	{
		AiActor* enemy = (AiActor*)level_alloc(sizeof(AiActor));
		gameObj_InitEnemy((GameObject2*)enemy, logic);
		enemy->baseObj.header.func = defaultAiFunc;
		enemy->baseObj.header.func2 = defaultAiFunc;
		enemy->baseObj.header.nextTick = 0xffffffff;

		enemy->hp = FIXED(4);	// default to 4 HP.
		enemy->itemDropId = -1;
		enemy->hurtSndSrc = 0;
		enemy->dieSndSrc = 0;
		enemy->ubc = 0;
		enemy->uc0 = -1;
		enemy->uc4 = -1;

		return enemy;
	}

	void actor_addLogicGameObj(ActorLogic* logic, ActorHeader* gameObj)
	{
		if (!gameObj) { return; }

		if (!logic->gameObj[0])
		{
			logic->gameObj[0] = gameObj;
			return;
		}

		for (s32 i = 1; i < ACTOR_MAX_GAME_OBJ; i++)
		{
			if (!logic->gameObj[i])
			{
				logic->gameObj[i] = gameObj;
				return;
			}
		}
	}

	JBool actor_handleMovementAndCollision(Actor* actor)
	{
		SecObject* obj = actor->header.obj;
		fixed16_16 dx = 0;
		fixed16_16 dz = 0;
		fixed16_16 dy = 0;
		fixed16_16 moveZ = 0;
		fixed16_16 moveY = 0;
		fixed16_16 moveX = 0;

		actor->collisionWall = nullptr;
		if (!(actor->updateFlags & 8))
		{
			if (actor->updateFlags & 1)
			{
				dx = actor->nextPos.x - obj->posWS.x;
				dz = actor->nextPos.z - obj->posWS.z;
			}
			if (!(actor->collisionFlags & 1))
			{
				if (actor->updateFlags & 2)
				{
					dy = actor->nextPos.y - obj->posWS.y;
				}
			}
			moveZ = moveY = moveX = 0;
			if (dx | dz)
			{
				fixed16_16 dirZ, dirX;
				fixed16_16 move = mul16(actor->speed, s_deltaTime);
				computeDirAndLength(dx, dz, &dirX, &dirZ);

				if (dx && dirX)
				{
					fixed16_16 deltaX = mul16(move, dirX);
					fixed16_16 absDx = (dx < 0) ? -dx : dx;
					moveX = clamp(deltaX, -absDx, absDx);
				}
				if (dz && dirZ)
				{
					fixed16_16 deltaZ = mul16(move, dirZ);
					fixed16_16 absDz = (dz < 0) ? -dz : dz;
					moveZ = clamp(deltaZ, -absDz, absDz);
				}
			}
			if (dy)
			{
				fixed16_16 deltaY = mul16(actor->speedVert, s_deltaTime);
				fixed16_16 absDy = (dy < 0) ? -dy : dy;
				moveY = clamp(deltaY, -absDy, absDy);
			}
		}

		if (actor->delta.y)
		{
			moveY += actor->delta.y;
		}
		CollisionInfo* physics = &actor->physics;
		fixed16_16 dirX;
		fixed16_16 dirZ;
		if (moveX | moveY | moveZ)
		{
			physics->offsetX = moveX;
			physics->offsetY = moveY;
			physics->offsetZ = moveZ;
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
				obj->yaw = (obj->yaw + computeAngleChange(yawDiff, angleChange)) & 0x3fff;

				if (obj->type & OBJ_TYPE_3D)
				{
					obj->pitch = (obj->pitch + computeAngleChange(pitchDiff, angleChange)) & 0x3fff;
					obj->roll  = (obj->roll  + computeAngleChange(rollDiff,  angleChange)) & 0x3fff;
					obj3d_computeTransform(obj);
				}
			}
		}
	}

	JBool defaultActorFunc(Actor* actor, Actor* actor1)
	{
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

	JBool defaultActorFunc2(Actor* actor0, Actor* actor1)
	{
		return JFALSE;
	}

	void obj_setupSmartObj(Actor* actor)
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

	Actor* actor_create(Logic* logic)
	{
		Actor* actor = (Actor*)level_alloc(sizeof(Actor));
		gameObj_Init((ActorHeader*)actor, logic);
		obj_setupSmartObj(actor);

		actor->header.func = defaultActorFunc;
		actor->func3 = defaultActorFunc2;
		// Overwrites height even though it was set in obj_setupSmartObj()
		actor->physics.height = 0x18000;	// 1.5 units
		actor->physics.wall = nullptr;
		actor->physics.u24 = 0;
		actor->physics.obj = actor->header.obj;

		return actor;
	}

	void actorLogicCleanupFunc(Logic* logic)
	{
		logic;
	}

	struct LogicAnimation
	{
		s32 u00;
		s32 dt;
		s32 u08;
		s32 u0c;
		fixed16_16 frame;
		u32 flags;
		s32 animId;
		s32 u1c;
	};

	static JBool s_objCollisionEnabled = JTRUE;
	static LogicAnimation* s_curAnimation = nullptr;
	static s32 s_2974f4[16] = { 0 };

	JBool actor_advanceAnimation(LogicAnimation* anim, SecObject* obj)
	{
		if (!anim->u08)
		{
			anim->u08 = s_2974f4[anim->u00];
			anim->flags &= ~2;

			obj->frame = floor16(anim->frame);
			obj->projectileLogic = (Logic*)&obj->posWS;
		}
		else
		{
			anim->u0c += s_2974f4[anim->u00] - anim->u08;
			s32 newFrame = anim->frame + anim->dt;
			anim->u08 = s_2974f4[anim->u00];

			if (newFrame <= anim->u0c)
			{
				if (anim->flags & 1)
				{
					newFrame -= ONE_16;
					anim->u0c = newFrame;
					obj->frame = floor16(newFrame);
					return JTRUE;
				}

				while (newFrame <= anim->u0c)
				{
					anim->u0c -= anim->dt;
				}
			}
			obj->frame = floor16(anim->u0c);
		}
		return JFALSE;
	}

	void actor_sendTerminalVelMsg(SecObject* obj)
	{
		message_sendToObj(obj, MSG_TERMINAL_VEL, hitEffectMsgFunc);
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

	void actorLogicTaskFunc(s32 id)
	{
		task_begin;
		while (id != -1)
		{
			if (s_objCollisionEnabled)
			{
				task_yield(TASK_NO_DELAY);
			}
			if (!s_objCollisionEnabled)
			{
				task_yield(0x123);
			}

			if (id == 0)
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
						for (s32 i = 0; i < ACTOR_MAX_GAME_OBJ; i++)
						{
							ActorHeader* header = actorLogic->gameObj[5 - i];
							if (header && header->func)
							{
								if (header->nextTick < s_curTick)
								{
									header->nextTick = header->func((Actor*)header, actorLogic->actor);
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
									func(actor, actor);
								}
							}

							if ((obj->type & OBJ_TYPE_SPRITE) && s_curAnimation)
							{
								obj->anim = s_curAnimation->animId;
								if (actor_advanceAnimation(s_curAnimation, obj))
								{
									s_curAnimation->flags |= 2;
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

	void actorPhysicsTaskFunc(s32 id)
	{
		task_begin;
		while (id != -1)
		{
			// TODO(Core Game Loop Release)
			task_yield(TASK_NO_DELAY);
		}
		task_end;
	}
}  // namespace TFE_DarkForces