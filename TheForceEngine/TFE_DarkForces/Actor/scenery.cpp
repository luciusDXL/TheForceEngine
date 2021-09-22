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
		SecObject* obj = aiActor->actor.header.obj;

		if (!(aiActor->anim.flags & AFLAG_READY))
		{
			s_curAnimation = &aiActor->anim;
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
					actor_setupAnimation(4, &aiActor->anim);
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
				actor_setupAnimation(4, &aiActor->anim);
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

		AiActor* aiActor = actor_createAiActor((Logic*)logic);
		aiActor->actor.header.func = sceneryLogicFunc;
		aiActor->actor.header.msgFunc = sceneryMsgFunc;
		aiActor->anim.flags |= AFLAG_READY;
		aiActor->hp = FIXED(1);
		actor_addLogicGameObj(logic, aiActor);
	}
}  // namespace TFE_DarkForces