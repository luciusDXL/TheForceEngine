#include "scenery.h"
#include "../logic.h"
#include "actorModule.h"
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/sound.h>
#include <TFE_Jedi/InfSystem/message.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	static s32 s_sceneryAnimTable[] =
	{ 0, -1, -1, -1, 1, 0, -1, -1, -1, -1, -1, -1, 0, -1, -1, -1 };

	// Actor function for exploders (i.e. landmines and exploding barrels).
	JBool sceneryLogicFunc(ActorModule* module, Actor* actor)
	{
		DamageModule* damageMod = (DamageModule*)module;
		LogicAnimation* anim = &damageMod->attackMod.anim;
		SecObject* obj = damageMod->attackMod.header.obj;

		if (!(anim->flags & AFLAG_READY))
		{
			s_actorState.curAnimation = anim;
			return JFALSE;
		}
		else if (damageMod->hp <= 0 && actor_getAnimationIndex(4) != -1)
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
	JBool sceneryMsgFunc(s32 msg, ActorModule* module, Actor* actor)
	{
		JBool retValue = JFALSE;
		DamageModule* damageMod = (DamageModule*)module;
		LogicAnimation* anim = &damageMod->attackMod.anim;
		SecObject* obj = damageMod->attackMod.header.obj;

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
					actor_setupAnimation(4, anim);
					actor_removeLogics(obj);
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
			damageMod->hp -= dmg;
			if (damageMod->hp > 0)
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

	Logic* scenery_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);
		dispatch->flags &= ~(FLAG_BIT(0) | FLAG_BIT(2));
		dispatch->animTable = s_sceneryAnimTable;

		DamageModule* module = actor_createDamageModule(dispatch);
		module->attackMod.header.func = sceneryLogicFunc;
		module->attackMod.header.msgFunc = sceneryMsgFunc;
		module->attackMod.anim.flags |= AFLAG_READY;
		module->hp = FIXED(1);
		actor_addModule(dispatch, (ActorModule*)module);

		return (Logic*)dispatch;
	}
}  // namespace TFE_DarkForces