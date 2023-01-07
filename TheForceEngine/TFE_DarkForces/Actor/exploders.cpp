#include "exploders.h"
#include "../logic.h"
#include "actorModule.h"
#include "animTables.h"
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/sound.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	// Actor function for exploders (i.e. landmines and exploding barrels).
	JBool exploderFunc(ActorModule* module, MovementModule* moveMod)
	{
		DamageModule* damageMod = (DamageModule*)module;
		LogicAnimation* anim = &damageMod->attackMod.anim;
		if (!(anim->flags & AFLAG_READY))
		{
			s_actorState.curAnimation = anim;
			return JFALSE;
		}
		else if ((anim->flags & AFLAG_PLAYED) && damageMod->hp <= 0)
		{
			actor_kill();
			return JFALSE;
		}
		return JTRUE;
	}

	// Actor message function for exploders, this is responsible for processing messages such as 
	// projectile damage and explosions. For other AI message functions, it would also process
	// "wake up" messages, but those messages are ignored for exploders.
	JBool exploderMsgFunc(s32 msg, ActorModule* module, MovementModule* moveMod)
	{
		DamageModule* damageMod = (DamageModule*)module;
		JBool retValue = JFALSE;
		SecObject* obj = damageMod->attackMod.header.obj;
		LogicAnimation* anim = &damageMod->attackMod.anim;

		if (msg == MSG_DAMAGE)
		{
			if (damageMod->hp > 0)
			{
				ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
				damageMod->hp -= proj->dmg;
				JBool retValue;
				if (damageMod->hp > 0)
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

					actor_setupAnimation(2/*animIndex*/, anim);
					moveMod->updateTargetFunc(moveMod, &damageMod->attackMod.target);
					retValue = JFALSE;
				}
			}
		}
		else if (msg == MSG_EXPLOSION)
		{
			if (damageMod->hp <= 0)
			{
				return JTRUE;
			}

			fixed16_16 dmg = s_msgArg1;
			fixed16_16 force = s_msgArg2;
			damageMod->hp -= dmg;

			vec3_fixed pushDir;
			vec3_fixed pos = { obj->posWS.x, obj->posWS.y - obj->worldHeight, obj->posWS.z };
			computeExplosionPushDir(&pos, &pushDir);

			fixed16_16 pushX = mul16(force, pushDir.x);
			fixed16_16 pushZ = mul16(force, pushDir.z);
			if (damageMod->hp > 0)
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

			ActorDispatch* actorLogic = (ActorDispatch*)s_actorState.curLogic;
			CollisionInfo colInfo = { 0 };
			colInfo.obj = proj2->logic.obj;
			colInfo.offsetY = 0;
			colInfo.offsetX = mul16(actorLogic->vel.x, 0x4000);
			colInfo.offsetZ = mul16(actorLogic->vel.z, 0x4000);
			colInfo.botOffset = ONE_16;
			colInfo.yPos = COL_INFINITY;
			colInfo.height = ONE_16;
			colInfo.unused = 0;
			colInfo.width = colInfo.obj->worldWidth;
			handleCollision(&colInfo);

			// I have to remove the logics here in order to get this to work, but this doesn't actually happen here in the original code.
			// TODO: Move to the correct location.
			actor_removeLogics(obj);
			actor_setupAnimation(2/*animIndex*/, anim);
			retValue = JFALSE;
		}
		return retValue;
	}

	Logic* barrel_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);

		dispatch->flags &= 0xfffffffb;
		dispatch->flags &= 0xfffffffe;
		dispatch->animTable = s_mineBarrelAnimTable;

		DamageModule* module = actor_createDamageModule(dispatch);
		module->attackMod.header.func = exploderFunc;
		module->attackMod.header.msgFunc = exploderMsgFunc;
		module->hp = FIXED(11);
		module->attackMod.anim.flags |= AFLAG_READY;
		actor_addModule(dispatch, (ActorModule*)module);

		MovementModule* moveMod = actor_createMovementModule(dispatch);
		dispatch->moveMod = moveMod;
		moveMod->collisionFlags |= 1;
		moveMod->physics.width = obj->worldWidth;
		moveMod->target.flags = (moveMod->target.flags | TARGET_FREEZE) & (~TARGET_ALL_MOVE);
		moveMod->target.speed = 0;
		moveMod->target.speedRotation = 0;

		ActorTarget* target = &module->attackMod.target;
		target->flags = (target->flags | 8) & 0xfffffff8;
		target->speed = 0;
		target->speedRotation = 0;

		return (Logic*)dispatch;
	}

	Logic* landmine_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);
		dispatch->flags &= ~4;
		dispatch->flags &= ~1;
		dispatch->animTable = s_mineBarrelAnimTable;

		DamageModule* module = actor_createDamageModule(dispatch);
		module->attackMod.header.func = exploderFunc;
		module->attackMod.header.msgFunc = exploderMsgFunc;
		module->attackMod.anim.flags |= AFLAG_READY;
		module->hp = FIXED(20);
		actor_addModule(dispatch, (ActorModule*)module);

		MovementModule* moveMod = actor_createMovementModule(dispatch);
		dispatch->moveMod = moveMod;
		moveMod->collisionFlags |= 1;
		// This was cleared to 0 in createProjectile()
		moveMod->physics.width = obj->worldWidth;

		ActorTarget* target = &module->attackMod.target;
		target->flags = (target->flags | 8) & 0xfffffff8;
		target->speed = 0;
		target->speedRotation = 0;

		moveMod->target.flags = TARGET_FREEZE;
		moveMod->target.speed = 0;
		moveMod->target.speedRotation = 0;

		dispatch->flags &= ~1;
		dispatch->animTable = s_mineBarrelAnimTable;

		return (Logic*)dispatch;
	}
}  // namespace TFE_DarkForces