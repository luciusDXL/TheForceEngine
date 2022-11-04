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
		
	u32 flyingModuleFunc(ActorModule* module, MovementModule* moveMod)
	{
		ThinkerModule* flyingMod = (ThinkerModule*)module;
		SecObject* obj = flyingMod->header.obj;
		if (flyingMod->anim.state == 0)
		{
			flyingMod->anim.state = 2;
			return s_curTick + random(flyingMod->delay);
		}
		else if (flyingMod->anim.state == 1)
		{
			if (actor_handleSteps(moveMod, &flyingMod->target))
			{
				flyingMod->anim.state = 0;
			}
			SecObject* actorObj = moveMod->header.obj;
			if (actor_arrivedAtTarget(&flyingMod->target, actorObj))
			{
				flyingMod->anim.state = 0;
			}
		}
		else if (flyingMod->anim.state == 2)
		{
			RSector* sector = obj->sector;
			if (sector == s_playerObject->sector)
			{
				// Target a random height above or below the player y position: playerPosY - [-1.5, 6.5]
				fixed16_16 heightChange = random(FIXED(5)) - 0x18000;	// rand(5) - 1.5
				flyingMod->target.pos.y = s_eyePos.y - heightChange;
				flyingMod->target.flags |= 2;
				flyingMod->anim.state = 1;
			}
			else
			{
				flyingMod->anim.state = 0;
			}
		}

		moveMod->updateTargetFunc(moveMod, &flyingMod->target);
		return 0;
	}

	u32 flyingModuleFunc_Remote(ActorModule* module, MovementModule* moveMod)
	{
		ThinkerModule* flyingMod = (ThinkerModule*)module;
		ActorTarget* target = &flyingMod->target;
		SecObject* obj = flyingMod->header.obj;

		if (flyingMod->anim.state == 0)
		{
			flyingMod->anim.state = 2;
			return s_curTick + random(flyingMod->delay);
		}
		else if (flyingMod->anim.state == 1)
		{
			if (actor_arrivedAtTarget(target, obj))
			{
				flyingMod->anim.state = 0;
			}
		}
		else if (flyingMod->anim.state == 2)
		{
			target->yaw   = random_next() & ANGLE_MASK;
			target->pitch = obj->pitch;
			target->roll  = obj->roll;
			target->flags |= 4;
			flyingMod->anim.state = 1;
		}

		moveMod->updateTargetFunc(moveMod, target);
		return 0;
	}

	ThinkerModule* actor_createFlyingModule(Logic* logic)
	{
		ThinkerModule* flyingMod = (ThinkerModule*)level_alloc(sizeof(ThinkerModule));
		actor_thinkerModuleInit(flyingMod);
		actor_initModule((ActorModule*)flyingMod, logic);

		flyingMod->header.func = flyingModuleFunc;
		flyingMod->delay = 145;

		return flyingMod;
	}

	ThinkerModule* actor_createFlyingModule_Remote(Logic* logic)
	{
		ThinkerModule* flyingMod = (ThinkerModule*)level_alloc(sizeof(ThinkerModule));
		actor_thinkerModuleInit(flyingMod);
		actor_initModule((ActorModule*)flyingMod, logic);

		flyingMod->header.func = flyingModuleFunc_Remote;
		flyingMod->delay = 145;	// 1 second.
		return flyingMod;
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

		ThinkerModule* thinkerMod = actor_createThinkerModule(dispatch);
		thinkerMod->target.speedRotation = 0;
		thinkerMod->target.speed = FIXED(6);
		thinkerMod->delay = 116;
		thinkerMod->anim.flags &= ~FLAG_BIT(0);
		thinkerMod->startDelay = TICKS(2);	// (145.5)*2
		actor_addModule(dispatch, (ActorModule*)thinkerMod);

		ThinkerModule* flyingMod = actor_createFlyingModule((Logic*)dispatch);
		flyingMod->target.speedRotation = 0x7fff;
		flyingMod->target.speed = FIXED(13);
		flyingMod->target.speedVert = FIXED(10);
		flyingMod->delay = 436;	// just shy of 3 seconds.
		actor_addModule(dispatch, (ActorModule*)flyingMod);
				
		MovementModule* moveMod = actor_createMovementModule(dispatch);
		dispatch->moveMod = moveMod;
		moveMod->collisionFlags = (moveMod->collisionFlags & 0xfffffff8) | 4;
		moveMod->physics.yPos = FIXED(200);
		moveMod->physics.width = obj->worldWidth;

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

		ThinkerModule* thinkerMod = actor_createThinkerModule(dispatch);
		thinkerMod->target.speedRotation = 0;
		thinkerMod->target.speed = FIXED(6);
		thinkerMod->delay = 116;
		thinkerMod->anim.flags &= ~FLAG_BIT(0);
		thinkerMod->startDelay = TICKS(2);	// (145.5)*2
		actor_addModule(dispatch, (ActorModule*)thinkerMod);

		ThinkerModule* flyingMod = actor_createFlyingModule((Logic*)dispatch);
		flyingMod->target.speedRotation = 0x7fff;
		flyingMod->target.speed = FIXED(4);
		flyingMod->target.speedVert = FIXED(2);
		flyingMod->delay = 436;	// just shy of 3 seconds.
		actor_addModule(dispatch, (ActorModule*)flyingMod);

		MovementModule* moveMod = actor_createMovementModule(dispatch);
		dispatch->moveMod = moveMod;
		moveMod->collisionFlags = (moveMod->collisionFlags & 0xfffffff8) | 4;
		moveMod->physics.yPos = FIXED(200);
		moveMod->physics.width = obj->worldWidth;

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

		ThinkerModule* thinkerMod = actor_createThinkerModule(dispatch);
		thinkerMod->target.speedRotation = 0x7fff;
		thinkerMod->target.speed = FIXED(15);
		thinkerMod->target.speedVert = FIXED(10);
		thinkerMod->delay = 436;
		thinkerMod->startDelay = 109;
		thinkerMod->targetOffset = FIXED(9);
		actor_addModule(dispatch, (ActorModule*)thinkerMod);

		ThinkerModule* flyingMod = actor_createFlyingModule_Remote((Logic*)dispatch);
		flyingMod->target.speedRotation = 0x7fff;
		flyingMod->delay = 291;
		actor_addModule(dispatch, (ActorModule*)flyingMod);

		flyingMod = actor_createFlyingModule((Logic*)dispatch);
		flyingMod->target.speedRotation = 0x7fff;
		flyingMod->target.speed = FIXED(13);
		flyingMod->target.speedVert = FIXED(10);
		flyingMod->delay = 291;
		actor_addModule(dispatch, (ActorModule*)flyingMod);

		MovementModule* moveMod = actor_createMovementModule(dispatch);
		dispatch->moveMod = moveMod;
		moveMod->collisionFlags &= 0xfffffff8;
		moveMod->collisionFlags |= 4;
		moveMod->physics.yPos = FIXED(200);

		// should be: 0xa7ec
		moveMod->physics.width = obj->worldWidth >> 1;
		obj->entityFlags &= 0xfffff7ff;
		dispatch->animTable = s_remoteAnimTable;
		actor_setupInitAnimation();

		return (Logic*)dispatch;
	}
}  // namespace TFE_DarkForces