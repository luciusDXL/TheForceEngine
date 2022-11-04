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
		ActorDispatch* logic = actor_setupActorLogic(obj, setupFunc);
		logic->alertSndSrc = s_alertSndSrc[ALERT_REEYEE];

		AiActor* module = actor_createAiActor((Logic*)logic);
		module->hp = FIXED(36);
		module->itemDropId = ITEM_DETONATORS;
		module->hurtSndSrc = s_agentSndSrc[AGENTSND_REEYEE_2];
		module->dieSndSrc = s_agentSndSrc[AGENTSND_REEYEE_3];
		actor_addModule(logic, (ActorModule*)module);

		ActorEnemy* enemyActor = actor_createEnemyActor((Logic*)logic);
		enemyActor->projType = PROJ_THERMAL_DET;
		enemyActor->attackPrimSndSrc = 0;
		enemyActor->attackSecSndSrc = 0;
		enemyActor->timing.state4Delay = 436;
		enemyActor->maxDist = FIXED(100);
		enemyActor->meleeRange = FIXED(10);
		enemyActor->meleeDmg = FIXED(15);
		enemyActor->attackFlags = (enemyActor->attackFlags | 3) & 0xfffffff7;
		enemyActor->minDist = s_reeyeeMinDist;
		s_actorState.curEnemyActor = enemyActor;
		actor_addModule(logic, (ActorModule*)enemyActor);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)logic);
		actorSimple->target.speedRotation = 0x7fff;
		actorSimple->target.speed = FIXED(13);
		actorSimple->anim.flags &= 0xfffffffe;
		actorSimple->startDelay = TICKS(2);
		actor_addModule(logic, (ActorModule*)actorSimple);

		Actor* actor = actor_create((Logic*)logic);
		logic->mover = (ActorModule*)actor;
		logic->animTable = s_reeyeesAnimTable;

		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)logic;
	}

	Logic* reeyees2_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* logic = actor_setupActorLogic(obj, setupFunc);
		logic->alertSndSrc = s_alertSndSrc[ALERT_REEYEE];

		AiActor* module = actor_createAiActor((Logic*)logic);
		module->hp = FIXED(36);
		module->hurtSndSrc = s_agentSndSrc[AGENTSND_REEYEE_2];
		module->dieSndSrc = s_agentSndSrc[AGENTSND_REEYEE_3];
		actor_addModule(logic, (ActorModule*)module);

		ActorEnemy* enemyActor = actor_createEnemyActor((Logic*)logic);
		enemyActor->attackSecSndSrc = 0;
		enemyActor->maxDist = FIXED(100);
		enemyActor->attackFlags = (enemyActor->attackFlags | 1) & 0xfffffff5;
		enemyActor->meleeDmg = FIXED(15);
		enemyActor->meleeRange = FIXED(10);
		s_actorState.curEnemyActor = enemyActor;
		actor_addModule(logic, (ActorModule*)enemyActor);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)logic);
		actorSimple->target.speedRotation = 0x7fff;
		actorSimple->target.speed = FIXED(13);
		actorSimple->anim.flags &= 0xfffffffe;
		actorSimple->startDelay = TICKS(2);
		actor_addModule(logic, (ActorModule*)actorSimple);

		Actor* actor = actor_create((Logic*)logic);
		logic->mover = (ActorModule*)actor;
		logic->animTable = s_reeyeesAnimTable;

		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)logic;
	}

	Logic* gamor_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* logic = actor_setupActorLogic(obj, setupFunc);
		logic->alertSndSrc = s_alertSndSrc[ALERT_GAMOR];

		AiActor* module = actor_createAiActor((Logic*)logic);
		module->hp = FIXED(90);
		module->stopOnHit = JFALSE;
		module->dieSndSrc = s_agentSndSrc[AGENTSND_GAMOR_1];
		module->hurtSndSrc = s_agentSndSrc[AGENTSND_GAMOR_2];
		actor_addModule(logic, (ActorModule*)module);

		ActorEnemy* enemyActor = actor_createEnemyActor((Logic*)logic);
		s_actorState.curEnemyActor = enemyActor;
		enemyActor->timing.state2Delay = 57;
		enemyActor->meleeRange = FIXED(13);
		enemyActor->meleeDmg = FIXED(40);
		enemyActor->ua4 = FIXED(360);
		enemyActor->attackSecSndSrc = s_agentSndSrc[AGENTSND_AXE_1];
		enemyActor->attackFlags = (enemyActor->attackFlags | 1) & 0xfffffffd;
		actor_addModule(logic, (ActorModule*)enemyActor);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)logic);
		actorSimple->approachVariation = 2730;
		actorSimple->u3c = 0;
		actorSimple->target.speedRotation = 0x3fff;
		actorSimple->target.speed = FIXED(12);
		actorSimple->anim.flags &= 0xfffffffe;
		actorSimple->startDelay = TICKS(2);
		actor_addModule(logic, (ActorModule*)actorSimple);

		Actor* actor = actor_create((Logic*)logic);
		logic->mover = (ActorModule*)actor;
		logic->animTable = s_gamorAnimTable;

		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)logic;
	}

	Logic* bossk_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorDispatch* logic = actor_setupActorLogic(obj, setupFunc);
		logic->alertSndSrc = s_alertSndSrc[ALERT_BOSSK];

		AiActor* aiActor = actor_createAiActor((Logic*)logic);
		aiActor->hp = FIXED(45);
		aiActor->dieSndSrc = s_agentSndSrc[AGENTSND_BOSSK_DIE];
		aiActor->hurtSndSrc = s_agentSndSrc[AGENTSND_BOSSK_3];
		aiActor->itemDropId = ITEM_CONCUSSION;
		actor_addModule(logic, (ActorModule*)aiActor);

		ActorEnemy* enemyActor = actor_createEnemyActor((Logic*)logic);
		s_actorState.curEnemyActor = enemyActor;
		enemyActor->projType = PROJ_CONCUSSION;
		enemyActor->maxDist = FIXED(100);
		enemyActor->minDist = FIXED(10);
		enemyActor->attackPrimSndSrc = s_concussion5SndSrc;
		enemyActor->fireSpread = 0;
		enemyActor->attackFlags = (enemyActor->attackFlags & 0xfffffffc) | 2;
		actor_addModule(logic, (ActorModule*)enemyActor);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)logic);
		actorSimple->target.speedRotation = 0x7fff;
		actorSimple->target.speed = FIXED(9);
		actorSimple->u3c = 0;
		actorSimple->anim.flags &= 0xfffffffe;
		actorSimple->startDelay = TICKS(2);
		actor_addModule(logic, (ActorModule*)actorSimple);

		Actor* actor = actor_create((Logic*)logic);
		logic->mover = (ActorModule*)actor;
		logic->animTable = s_bosskAnimTable;

		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)logic;
	}
}  // namespace TFE_DarkForces