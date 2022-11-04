#include "flyers.h"
#include "actorModule.h"
#include "../logic.h"
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/item.h>
#include <TFE_DarkForces/random.h>
#include <TFE_DarkForces/weapon.h>
#include <TFE_DarkForces/sound.h>
#include <TFE_Game/igame.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	static const s32 s_intDroidAnimTable[] =
	{ 0, 1, 2, 3, 4, 5, -1, 7, -1, -1, -1, -1, 12, -1, -1, -1 };
	static const s32 s_probeDroidAnimTable[] = 
	{ 0, 1, 2, 3, 4, 5, -1, -1, -1, -1, -1, -1, 12, -1, -1, -1 };
	static const s32 s_remoteAnimTable[] =
	{ 0, 0, 2, 3, -1, 0, -1, -1, -1, -1, -1, -1, 0, -1, -1, -1 };
		
	u32 flyingActorFunc(ActorModule* module, Actor* actor)
	{
		ActorFlyer* flyingActor = (ActorFlyer*)module;
		SecObject* obj = flyingActor->header.obj;
		if (flyingActor->state == 0)
		{
			flyingActor->state = 2;
			return s_curTick + random(flyingActor->delay);
		}
		else if (flyingActor->state == 1)
		{
			if (actor_handleSteps(actor, &flyingActor->target))
			{
				flyingActor->state = 0;
			}
			SecObject* actorObj = actor->header.obj;
			if (actor_arrivedAtTarget(&flyingActor->target, actorObj))
			{
				flyingActor->state = 0;
			}
		}
		else if (flyingActor->state == 2)
		{
			RSector* sector = obj->sector;
			if (sector == s_playerObject->sector)
			{
				// Target a random height above or below the player y position: playerPosY - [-1.5, 6.5]
				fixed16_16 heightChange = random(FIXED(5)) - 0x18000;	// rand(5) - 1.5
				flyingActor->target.pos.y = s_eyePos.y - heightChange;
				flyingActor->target.flags |= 2;
				flyingActor->state = 1;
			}
			else
			{
				flyingActor->state = 0;
			}
		}

		actor->updateTargetFunc(actor, &flyingActor->target);
		return 0;
	}

	u32 flyingActorAiFunc2(ActorModule* module, Actor* actor)
	{
		ActorFlyer* flyingActor = (ActorFlyer*)module;
		ActorTarget* target = &flyingActor->target;
		SecObject* obj = flyingActor->header.obj;

		if (flyingActor->state == 0)
		{
			flyingActor->state = 2;
			return s_curTick + random(flyingActor->delay);
		}
		else if (flyingActor->state == 1)
		{
			if (actor_arrivedAtTarget(target, obj))
			{
				flyingActor->state = 0;
			}
		}
		else if (flyingActor->state == 2)
		{
			target->yaw   = random_next() & ANGLE_MASK;
			target->pitch = obj->pitch;
			target->roll  = obj->roll;
			target->flags |= 4;
			flyingActor->state = 1;
		}

		actor->updateTargetFunc(actor, target);
		return 0;
	}

	ActorFlyer* actor_createFlying(Logic* logic)
	{
		ActorFlyer* actor = (ActorFlyer*)level_alloc(sizeof(ActorFlyer));

		actor->target.speedRotation = 0;
		actor->target.speed = FIXED(4);
		actor->target.speedVert = FIXED(10);
		actor->delay = 72;
		actor->u68 = 0;
		actor->u6c = -1;
		actor->state = 2;
		actor->u40 = 728;
		actor->width = 5;
		actor->u4c = ONE_16;
		actor->nextTick = 0;
		actor->u70 = 0;
		actor->target.flags &= 0xfffffff0;
		actor->u5c &= 0xfffffffe;
		actor_initModule(&actor->header, logic);
		actor->header.func = flyingActorFunc;
		actor->delay = 145;

		return actor;
	}

	ActorFlyer* actor_createFlying2(Logic* logic)
	{
		ActorFlyer* actor = (ActorFlyer*)level_alloc(sizeof(ActorFlyer));
		actor->target.flags &= 0xfffffff0;
		actor->target.speedRotation = 0;
		actor->target.speed = FLAG_BIT(18);
		actor->target.speedVert = FIXED(10);
		actor->delay = 72;	// ~0.5 seconds
		actor->u68 = 0;
		actor->u6c = -1;
		actor->state = 2;
		actor->u40 = 728;
		actor->width = 5;
		actor->u4c = ONE_16;
		actor->u5c &= ~1;
		actor->nextTick = 0;
		actor->u70 = 0;
		actor_initModule(&actor->header, logic);

		actor->header.func = flyingActorAiFunc2;
		actor->delay = 145;	// 1 second.
		return actor;
	}

	Logic* intDroid_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);
		dispatch->fov = 0x4000;	// 360 degrees
		dispatch->alertSndSrc = s_alertSndSrc[ALERT_INTDROID];

		DamageModule* module = actor_createDamageModule(dispatch);
		module->hp = FIXED(45);
		module->itemDropId = ITEM_POWER;
		module->stopOnHit = JFALSE;
		module->dieSndSrc = s_agentSndSrc[AGENTSND_SMALL_EXPLOSION];
		actor_addModule(dispatch, (ActorModule*)module);

		AttackModule* attackMod = actor_createAttackModule(dispatch);
		attackMod->fireOffset.y = 9830;
		attackMod->projType = PROJ_PROBE_PROJ;
		attackMod->attackSecSndSrc = s_agentSndSrc[AGENTSND_INTSTUN];
		attackMod->attackPrimSndSrc = s_agentSndSrc[AGENTSND_PROBFIRE_11];
		attackMod->meleeRange = FIXED(30);
		attackMod->meleeDmg = FIXED(5);
		attackMod->attackFlags |= 3;
		s_actorState.attackMod = attackMod;
		actor_addModule(dispatch, (ActorModule*)attackMod);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)dispatch);
		actorSimple->target.speedRotation = 0;
		actorSimple->target.speed = FIXED(6);
		actorSimple->u3c = 116;
		actorSimple->anim.flags &= ~FLAG_BIT(0);
		actorSimple->startDelay = TICKS(2);	// (145.5)*2
		actor_addModule(dispatch, (ActorModule*)actorSimple);

		ActorFlyer* flyingActor = actor_createFlying((Logic*)dispatch);
		flyingActor->target.speedRotation = 0x7fff;
		flyingActor->target.speed = FIXED(13);
		flyingActor->target.speedVert = FIXED(10);
		flyingActor->delay = 436;	// just shy of 3 seconds.
		actor_addModule(dispatch, (ActorModule*)flyingActor);
				
		Actor* actor = actor_create((Logic*)dispatch);
		dispatch->mover = (ActorModule*)actor;
		actor->collisionFlags = (actor->collisionFlags & 0xfffffff8) | 4;
		actor->physics.yPos = FIXED(200);
		actor->physics.width = obj->worldWidth;

		// Setup the animation.
		dispatch->animTable = s_intDroidAnimTable;
		actor_setupInitAnimation();

		return (Logic*)dispatch;
	}

	Logic* probeDroid_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);
		dispatch->fov = 0x4000;	// 360 degrees
		dispatch->alertSndSrc = s_alertSndSrc[ALERT_PROBE];

		DamageModule* module = actor_createDamageModule(dispatch);
		module->hp = FIXED(45);
		module->itemDropId = ITEM_POWER;
		module->stopOnHit = JFALSE;
		module->dieEffect = HEFFECT_EXP_35;
		module->dieSndSrc = s_agentSndSrc[AGENTSND_PROBE_ALM];
		actor_addModule(dispatch, (ActorModule*)module);

		AttackModule* attackMod = actor_createAttackModule(dispatch);
		attackMod->fireOffset.y = -557056;
		attackMod->projType = PROJ_RIFLE_BOLT;
		attackMod->attackPrimSndSrc = s_agentSndSrc[AGENTSND_PROBFIRE_12];
		attackMod->attackFlags = 2;
		s_actorState.attackMod = attackMod;
		actor_addModule(dispatch, (ActorModule*)attackMod);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)dispatch);
		actorSimple->target.speedRotation = 0;
		actorSimple->target.speed = FIXED(6);
		actorSimple->u3c = 116;
		actorSimple->anim.flags &= ~FLAG_BIT(0);
		actorSimple->startDelay = TICKS(2);	// (145.5)*2
		actor_addModule(dispatch, (ActorModule*)actorSimple);

		ActorFlyer* flyingActor = actor_createFlying((Logic*)dispatch);
		flyingActor->target.speedRotation = 0x7fff;
		flyingActor->target.speed = FIXED(4);
		flyingActor->target.speedVert = FIXED(2);
		flyingActor->delay = 436;	// just shy of 3 seconds.
		actor_addModule(dispatch, (ActorModule*)flyingActor);

		Actor* actor = actor_create((Logic*)dispatch);
		dispatch->mover = (ActorModule*)actor;
		actor->collisionFlags = (actor->collisionFlags & 0xfffffff8) | 4;
		actor->physics.yPos = FIXED(200);
		actor->physics.width = obj->worldWidth;

		// Setup the animation.
		dispatch->animTable = s_probeDroidAnimTable;
		actor_setupInitAnimation();

		return (Logic*)dispatch;
	}

	Logic* remote_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		obj->entityFlags = ETFLAG_AI_ACTOR | ETFLAG_FLYING | ETFLAG_REMOTE;
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);
		dispatch->fov = 0x4000;

		DamageModule* module = actor_createDamageModule(dispatch);
		module->dieSndSrc = s_agentSndSrc[AGENTSND_TINY_EXPLOSION];
		module->hp = FIXED(9);
		actor_addModule(dispatch, (ActorModule*)module);

		AttackModule* attackMod = actor_createAttackModule(dispatch);
		attackMod->attackPrimSndSrc = s_agentSndSrc[AGENTSND_PROBFIRE_13];
		attackMod->projType = PROJ_REMOTE_BOLT;
		attackMod->attackFlags &= 0xfffffffc;
		attackMod->attackFlags |= 2;
		attackMod->fireOffset.y = 0x4000;	// 0.25 units.
		attackMod->maxDist = FIXED(50);
		s_actorState.attackMod = attackMod;
		actor_addModule(dispatch, (ActorModule*)attackMod);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)dispatch);
		actorSimple->target.speedRotation = 0x7fff;
		actorSimple->target.speed = FIXED(15);
		actorSimple->target.speedVert = FIXED(10);
		actorSimple->u3c = 436;
		actorSimple->startDelay = 109;
		actorSimple->targetOffset = FIXED(9);
		actor_addModule(dispatch, (ActorModule*)actorSimple);

		ActorFlyer* flyingActor = actor_createFlying2((Logic*)dispatch);
		flyingActor->target.speedRotation = 0x7fff;
		flyingActor->delay = 291;
		actor_addModule(dispatch, (ActorModule*)flyingActor);

		flyingActor = actor_createFlying((Logic*)dispatch);
		flyingActor->target.speedRotation = 0x7fff;
		flyingActor->target.speed = FIXED(13);
		flyingActor->target.speedVert = FIXED(10);
		flyingActor->delay = 291;
		actor_addModule(dispatch, (ActorModule*)flyingActor);

		Actor* actor = actor_create((Logic*)dispatch);
		dispatch->mover = (ActorModule*)actor;
		actor->collisionFlags &= 0xfffffff8;
		actor->collisionFlags |= 4;
		actor->physics.yPos = FIXED(200);

		// should be: 0xa7ec
		actor->physics.width = obj->worldWidth >> 1;
		obj->entityFlags &= 0xfffff7ff;
		dispatch->animTable = s_remoteAnimTable;
		actor_setupInitAnimation();

		return (Logic*)dispatch;
	}
}  // namespace TFE_DarkForces