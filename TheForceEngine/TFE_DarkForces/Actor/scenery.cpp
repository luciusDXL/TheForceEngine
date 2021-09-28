#include "scenery.h"
#include "../logic.h"
#include "aiActor.h"
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/player.h>
#include <TFE_Jedi/InfSystem/message.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	static s32 s_sceneryAnimTable[] =
	{ 0, -1, -1, -1, 1, 0 };

	// Actor function for exploders (i.e. landmines and exploding barrels).
	JBool sceneryLogicFunc(AiActor* aiActor, Actor* actor)
	{
		LogicAnimation* anim = &aiActor->enemy.anim;
		SecObject* obj = aiActor->enemy.header.obj;

		if (!(anim->flags & AFLAG_READY))
		{
			s_curAnimation = anim;
			return JFALSE;
		}
		else if (aiActor->hp <= 0 && actor_getAnimationIndex(4) != -1)
		{
			SecObject* newObj = allocateObject();
			sprite_setData(newObj, obj->wax);
			newObj->frame = 0;
			newObj->anim = actor_getAnimationIndex(4);
			newObj->posWS = obj->posWS;
			newObj->worldWidth = obj->worldWidth;
			if (newObj->worldWidth)
			{
				newObj->flags |= OBJ_FLAG_HAS_COLLISION;
			}

			sector_addObject(obj->sector, newObj);
			actor_kill();
			return JFALSE;
		}
		return JTRUE;
	}

	// Actor message function for exploders, this is responsible for processing messages such as 
	// projectile damage and explosions. For other AI message functions, it would also process
	// "wake up" messages, but those messages are ignored for exploders.
	JBool sceneryMsgFunc(s32 msg, AiActor* aiActor, Actor* actor)
	{
		JBool retValue = JFALSE;
		LogicAnimation* anim = &aiActor->enemy.anim;
		SecObject* obj = aiActor->enemy.header.obj;

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
					actor_setupAnimation(4, anim);
					actor_removeLogics(obj);
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
			aiActor->hp -= dmg;
			if (aiActor->hp > 0)
			{
				retValue = JTRUE;
			}
			else
			{
				actor_setupAnimation(4, anim);
				actor_removeLogics(obj);
				retValue = JFALSE;
			}
		}
		return retValue;
	}

	void scenery_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorLogic* logic = actor_setupActorLogic(obj, setupFunc);
		logic->flags &= ~(FLAG_BIT(0) | FLAG_BIT(2));
		logic->animTable = s_sceneryAnimTable;

		if (obj->worldWidth)
		{
			obj->flags |= OBJ_FLAG_HAS_COLLISION;
		}

		AiActor* aiActor = actor_createAiActor((Logic*)logic);
		aiActor->enemy.header.func = sceneryLogicFunc;
		aiActor->enemy.header.msgFunc = sceneryMsgFunc;
		aiActor->enemy.anim.flags |= AFLAG_READY;
		aiActor->hp = FIXED(1);
		actorLogic_addActor(logic, aiActor);
	}
}  // namespace TFE_DarkForces