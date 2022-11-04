#include "troopers.h"
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
	static const s32 s_officerAnimTable[] =
	{ 0, 6, 2, 3, 4, 5,  1, -1, -1, -1, -1, -1, 12, -1, -1, -1 };
	static const s32 s_troopAnimTable[] =
	{ 0, 1, 3, 2, 4, 5, -1, -1, -1, -1, -1, -1, 12, -1, -1, -1 };
	static const s32 s_commandoAnimTable[] =
	{ 0, 1, 2, 3, 4, 5,  6, -1, -1, -1, -1, -1, 12, -1, -1, -1 };

	ItemId officer_getItemDropId(KEYWORD logicId)
	{
		switch (logicId)
		{
			case KW_I_OFFICER:  { return ITEM_ENERGY; } break;
			case KW_I_OFFICER1: { return ITEM_CODE1; } break;
			case KW_I_OFFICER2: { return ITEM_CODE2; } break;
			case KW_I_OFFICER3:	{ return ITEM_CODE3; } break;
			case KW_I_OFFICER4:	{ return ITEM_CODE4; } break;
			case KW_I_OFFICER5: { return ITEM_CODE5; } break;
			case KW_I_OFFICER6:	{ return ITEM_CODE6; } break;
			case KW_I_OFFICER7: { return ITEM_CODE7; } break;
			case KW_I_OFFICER8: { return ITEM_CODE8; } break;
			case KW_I_OFFICER9: { return ITEM_CODE9; } break;
			case KW_I_OFFICERR:	{ return ITEM_RED_KEY;    } break;
			case KW_I_OFFICERY: { return ITEM_YELLOW_KEY; } break;
			case KW_I_OFFICERB: { return ITEM_BLUE_KEY;   } break;
		}
		return ITEM_ENERGY;
	}

	Logic* officer_setup(SecObject* obj, LogicSetupFunc* setupFunc, KEYWORD logicId)
	{
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);
		dispatch->flags |= FLAG_BIT(4);	// Use Officer alert table.
		dispatch->alertSndSrc = 0;

		DamageModule* module = actor_createDamageModule(dispatch);
		module->hp = FIXED(9);
		module->hurtSndSrc = s_agentSndSrc[AGENTSND_STORM_HURT];
		module->dieSndSrc  = s_agentSndSrc[AGENTSND_STORM_DIE];
		module->itemDropId = officer_getItemDropId(logicId);
		actor_addModule(dispatch, (ActorModule*)module);

		AttackModule* attackMod = actor_createAttackModule(dispatch);
		attackMod->attackFlags = (attackMod->attackFlags & 0xfffffffc) | 2;
		attackMod->projType = PROJ_PISTOL_BOLT;
		attackMod->attackPrimSndSrc = s_pistolSndSrc;

		s_actorState.attackMod = attackMod;
		actor_addModule(dispatch, (ActorModule*)attackMod);

		ThinkerModule* thinkerMod = actor_createThinkerModule(dispatch);
		thinkerMod->target.speedRotation = HALF_16 - 1;
		thinkerMod->target.speed = FIXED(7);
		thinkerMod->u3c = 0;
		thinkerMod->anim.flags &= 0xfffffffe;
		thinkerMod->startDelay = TICKS(2);
		actor_addModule(dispatch, (ActorModule*)thinkerMod);

		Actor* actor = actor_create((Logic*)dispatch);
		dispatch->mover = (ActorModule*)actor;
		dispatch->animTable = s_officerAnimTable;
		s_actorState.curLogic = (Logic*)dispatch;

		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)dispatch;
	}

	Logic* trooper_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);
		dispatch->flags |= FLAG_BIT(5);	// Use Stormtrooper alert table.
		dispatch->alertSndSrc = 0;

		DamageModule* module = actor_createDamageModule(dispatch);
		module->hp = FIXED(18);
		module->hurtSndSrc = s_agentSndSrc[AGENTSND_STORM_HURT];
		module->dieSndSrc = s_agentSndSrc[AGENTSND_STORM_DIE];
		module->itemDropId = ITEM_RIFLE;
		actor_addModule(dispatch, (ActorModule*)module);

		AttackModule* attackMod = actor_createAttackModule(dispatch);
		s_actorState.attackMod = attackMod;
		attackMod->attackFlags = (attackMod->attackFlags & 0xfffffffc) | 2;
		attackMod->projType = PROJ_RIFLE_BOLT;
		attackMod->attackPrimSndSrc = s_rifleSndSrc;
		actor_addModule(dispatch, (ActorModule*)attackMod);

		ThinkerModule* thinkerMod = actor_createThinkerModule(dispatch);
		thinkerMod->target.speedRotation = HALF_16 - 1;
		thinkerMod->target.speed = FIXED(8);
		thinkerMod->u3c = 116;
		thinkerMod->anim.flags &= 0xfffffffe;
		thinkerMod->startDelay = TICKS(2);
		actor_addModule(dispatch, (ActorModule*)thinkerMod);

		Actor* actor = actor_create((Logic*)dispatch);
		dispatch->mover = (ActorModule*)actor;
		dispatch->animTable = s_troopAnimTable;
		s_actorState.curLogic = (Logic*)dispatch;

		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)dispatch;
	}

	Logic* commando_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);
		dispatch->flags |= FLAG_BIT(5);	// Use Stormtrooper alert table.
		dispatch->alertSndSrc = 0;

		DamageModule* module = actor_createDamageModule(dispatch);
		module->hp = FIXED(27);
		module->hurtSndSrc = s_agentSndSrc[AGENTSND_STORM_HURT];
		module->dieSndSrc = s_agentSndSrc[AGENTSND_STORM_DIE];
		module->itemDropId = ITEM_RIFLE;
		actor_addModule(dispatch, (ActorModule*)module);

		AttackModule* attackMod = actor_createAttackModule(dispatch);
		s_actorState.attackMod = attackMod;
		attackMod->attackFlags = (attackMod->attackFlags & 0xfffffffc) | 2;
		attackMod->projType = PROJ_RIFLE_BOLT;
		attackMod->attackPrimSndSrc = s_rifleSndSrc;
		actor_addModule(dispatch, (ActorModule*)attackMod);

		ThinkerModule* thinkerMod = actor_createThinkerModule(dispatch);
		thinkerMod->target.speedRotation = HALF_16 - 1;
		thinkerMod->target.speed = FIXED(9);
		thinkerMod->u3c = 116;
		thinkerMod->anim.flags &= 0xfffffffe;
		thinkerMod->startDelay = TICKS(1);
		actor_addModule(dispatch, (ActorModule*)thinkerMod);

		Actor* actor = actor_create((Logic*)dispatch);
		dispatch->mover = (ActorModule*)actor;
		dispatch->animTable = s_commandoAnimTable;
		s_actorState.curLogic = (Logic*)dispatch;

		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)dispatch;
	}
}  // namespace TFE_DarkForces