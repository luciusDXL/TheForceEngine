#include "enemies.h"
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
	static const s32 s_reeyeesAnimTable[] =
	{ 0, 1, 2, 3, 4, 5, -1, 7, 8, -1, -1, -1, 12, -1, -1, -1 };
	static const s32 s_bosskAnimTable[] =
	{ 0, 1, 2, 3, 4, 5, 6, -1, -1, -1, -1, -1, 12, -1, -1, -1 };
	static s32 s_gamorAnimTable[] =
	{ 0, 1, 2, 3, 4, 5, -1, -1, -1, -1, -1, -1, 12, -1, -1, -1 };

	static const s32 s_reeyeeMinDist = FIXED(30);
	
	Logic* reeyees_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);
		dispatch->alertSndSrc = s_alertSndSrc[ALERT_REEYEE];

		DamageModule* module = actor_createDamageModule(dispatch);
		module->hp = FIXED(36);
		module->itemDropId = ITEM_DETONATORS;
		module->hurtSndSrc = s_agentSndSrc[AGENTSND_REEYEE_2];
		module->dieSndSrc = s_agentSndSrc[AGENTSND_REEYEE_3];
		actor_addModule(dispatch, (ActorModule*)module);

		AttackModule* attackMod = actor_createAttackModule(dispatch);
		attackMod->projType = PROJ_THERMAL_DET;
		attackMod->attackPrimSndSrc = 0;
		attackMod->attackSecSndSrc = 0;
		attackMod->timing.state4Delay = 436;
		attackMod->maxDist = FIXED(100);
		attackMod->meleeRange = FIXED(10);
		attackMod->meleeDmg = FIXED(15);
		attackMod->attackFlags = (attackMod->attackFlags | 3) & 0xfffffff7;
		attackMod->minDist = s_reeyeeMinDist;
		s_actorState.attackMod = attackMod;
		actor_addModule(dispatch, (ActorModule*)attackMod);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)dispatch);
		actorSimple->target.speedRotation = 0x7fff;
		actorSimple->target.speed = FIXED(13);
		actorSimple->anim.flags &= 0xfffffffe;
		actorSimple->startDelay = TICKS(2);
		actor_addModule(dispatch, (ActorModule*)actorSimple);

		Actor* actor = actor_create((Logic*)dispatch);
		dispatch->mover = (ActorModule*)actor;
		dispatch->animTable = s_reeyeesAnimTable;

		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)dispatch;
	}

	Logic* reeyees2_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);
		dispatch->alertSndSrc = s_alertSndSrc[ALERT_REEYEE];

		DamageModule* module = actor_createDamageModule(dispatch);
		module->hp = FIXED(36);
		module->hurtSndSrc = s_agentSndSrc[AGENTSND_REEYEE_2];
		module->dieSndSrc = s_agentSndSrc[AGENTSND_REEYEE_3];
		actor_addModule(dispatch, (ActorModule*)module);

		AttackModule* attackMod = actor_createAttackModule(dispatch);
		attackMod->attackSecSndSrc = 0;
		attackMod->maxDist = FIXED(100);
		attackMod->attackFlags = (attackMod->attackFlags | 1) & 0xfffffff5;
		attackMod->meleeDmg = FIXED(15);
		attackMod->meleeRange = FIXED(10);
		s_actorState.attackMod = attackMod;
		actor_addModule(dispatch, (ActorModule*)attackMod);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)dispatch);
		actorSimple->target.speedRotation = 0x7fff;
		actorSimple->target.speed = FIXED(13);
		actorSimple->anim.flags &= 0xfffffffe;
		actorSimple->startDelay = TICKS(2);
		actor_addModule(dispatch, (ActorModule*)actorSimple);

		Actor* actor = actor_create((Logic*)dispatch);
		dispatch->mover = (ActorModule*)actor;
		dispatch->animTable = s_reeyeesAnimTable;

		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)dispatch;
	}

	Logic* gamor_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);
		dispatch->alertSndSrc = s_alertSndSrc[ALERT_GAMOR];

		DamageModule* module = actor_createDamageModule(dispatch);
		module->hp = FIXED(90);
		module->stopOnHit = JFALSE;
		module->dieSndSrc = s_agentSndSrc[AGENTSND_GAMOR_1];
		module->hurtSndSrc = s_agentSndSrc[AGENTSND_GAMOR_2];
		actor_addModule(dispatch, (ActorModule*)module);

		AttackModule* attackMod = actor_createAttackModule(dispatch);
		s_actorState.attackMod = attackMod;
		attackMod->timing.state2Delay = 57;
		attackMod->meleeRange = FIXED(13);
		attackMod->meleeDmg = FIXED(40);
		attackMod->ua4 = FIXED(360);
		attackMod->attackSecSndSrc = s_agentSndSrc[AGENTSND_AXE_1];
		attackMod->attackFlags = (attackMod->attackFlags | 1) & 0xfffffffd;
		actor_addModule(dispatch, (ActorModule*)attackMod);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)dispatch);
		actorSimple->approachVariation = 2730;
		actorSimple->u3c = 0;
		actorSimple->target.speedRotation = 0x3fff;
		actorSimple->target.speed = FIXED(12);
		actorSimple->anim.flags &= 0xfffffffe;
		actorSimple->startDelay = TICKS(2);
		actor_addModule(dispatch, (ActorModule*)actorSimple);

		Actor* actor = actor_create((Logic*)dispatch);
		dispatch->mover = (ActorModule*)actor;
		dispatch->animTable = s_gamorAnimTable;

		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)dispatch;
	}

	Logic* bossk_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* dispatch = actor_createDispatch(obj, setupFunc);
		dispatch->alertSndSrc = s_alertSndSrc[ALERT_BOSSK];

		DamageModule* module = actor_createDamageModule(dispatch);
		module->hp = FIXED(45);
		module->dieSndSrc = s_agentSndSrc[AGENTSND_BOSSK_DIE];
		module->hurtSndSrc = s_agentSndSrc[AGENTSND_BOSSK_3];
		module->itemDropId = ITEM_CONCUSSION;
		actor_addModule(dispatch, (ActorModule*)module);

		AttackModule* attackMod = actor_createAttackModule(dispatch);
		s_actorState.attackMod = attackMod;
		attackMod->projType = PROJ_CONCUSSION;
		attackMod->maxDist = FIXED(100);
		attackMod->minDist = FIXED(10);
		attackMod->attackPrimSndSrc = s_concussion5SndSrc;
		attackMod->fireSpread = 0;
		attackMod->attackFlags = (attackMod->attackFlags & 0xfffffffc) | 2;
		actor_addModule(dispatch, (ActorModule*)attackMod);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)dispatch);
		actorSimple->target.speedRotation = 0x7fff;
		actorSimple->target.speed = FIXED(9);
		actorSimple->u3c = 0;
		actorSimple->anim.flags &= 0xfffffffe;
		actorSimple->startDelay = TICKS(2);
		actor_addModule(dispatch, (ActorModule*)actorSimple);

		Actor* actor = actor_create((Logic*)dispatch);
		dispatch->mover = (ActorModule*)actor;
		dispatch->animTable = s_bosskAnimTable;

		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)dispatch;
	}
}  // namespace TFE_DarkForces