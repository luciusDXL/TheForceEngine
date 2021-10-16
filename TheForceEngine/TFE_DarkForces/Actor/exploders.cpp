#include "exploders.h"
#include "../logic.h"
#include "aiActor.h"
#include <TFE_DarkForces/player.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	static s32 s_mineBarrelAnimTable[] =
	{ 0, -1, 1, 1, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

	// Actor function for exploders (i.e. landmines and exploding barrels).
	JBool exploderFunc(AiActor* aiActor, Actor* actor)
	{
		LogicAnimation* anim = &aiActor->enemy.anim;
		if (!(anim->flags & AFLAG_READY))
		{
			s_actorState.curAnimation = anim;
			return JFALSE;
		}
		else if ((anim->flags & AFLAG_PLAYED) && aiActor->hp <= 0)
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
		SecObject* obj = aiActor->enemy.header.obj;
		LogicAnimation* anim = &aiActor->enemy.anim;

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

					actor_setupAnimation(2/*animIndex*/, anim);
					actor->updateTargetFunc(actor, &aiActor->enemy.target);
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

			ActorLogic* actorLogic = (ActorLogic*)s_actorState.curLogic;
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
			actor_setupAnimation(2/*animIndex*/, anim);
			retValue = JFALSE;
		}
		return retValue;
	}

	void barrel_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorLogic* logic = actor_setupActorLogic(obj, setupFunc);
		obj->flags |= OBJ_FLAG_HAS_COLLISION;

		logic->flags &= 0xfffffffb;
		logic->flags &= 0xfffffffe;
		logic->animTable = s_mineBarrelAnimTable;

		AiActor* aiActor = actor_createAiActor((Logic*)logic);
		aiActor->enemy.header.func = exploderFunc;
		aiActor->enemy.header.msgFunc = exploderMsgFunc;
		aiActor->hp = FIXED(11);
		aiActor->enemy.anim.flags |= AFLAG_READY;
		actorLogic_addActor(logic, aiActor);

		Actor* actor = actor_create((Logic*)logic);
		logic->actor = actor;
		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor->target.flags = (actor->target.flags | 8) & 0xfffffff8;
		actor->target.speed = 0;
		actor->target.speedRotation = 0;

		ActorTarget* target = &aiActor->enemy.target;
		target->flags = (target->flags | 8) & 0xfffffff8;
		target->speed = 0;
		target->speedRotation = 0;
	}

	void landmine_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorLogic* actorLogic = actor_setupActorLogic(obj, setupFunc);
		actorLogic->flags &= ~4;
		actorLogic->flags &= ~1;
		actorLogic->animTable = s_mineBarrelAnimTable;

		AiActor* aiActor = actor_createAiActor((Logic*)actorLogic);
		aiActor->enemy.header.func = exploderFunc;
		aiActor->enemy.header.msgFunc = exploderMsgFunc;
		aiActor->enemy.anim.flags |= AFLAG_READY;
		aiActor->hp = FIXED(20);
		actorLogic_addActor(actorLogic, aiActor);

		Actor* actor = actor_create((Logic*)actorLogic);
		actorLogic->actor = actor;
		actor->collisionFlags |= 1;
		// This was cleared to 0 in createProjectile()
		actor->physics.width = obj->worldWidth;

		ActorTarget* target = &aiActor->enemy.target;
		target->flags = (target->flags | 8) & 0xfffffff8;
		target->speed = 0;
		target->speedRotation = 0;

		actor->target.flags = 8;
		actor->target.speed = 0;
		actor->target.speedRotation = 0;

		actorLogic->flags &= ~1;
		actorLogic->animTable = s_mineBarrelAnimTable;
	}
}  // namespace TFE_DarkForces