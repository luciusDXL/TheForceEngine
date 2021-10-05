#include "sewer.h"
#include "aiActor.h"
#include "../logic.h"
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/item.h>
#include <TFE_DarkForces/random.h>
#include <TFE_DarkForces/pickup.h>
#include <TFE_DarkForces/weapon.h>
#include <TFE_Game/igame.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	static const s32 s_sewerCreatureAnimTable[] =
	{ -1, 1, 2, 3, 4, 5, 6, -1, -1, -1, -1, -1, -1, -1, 12, 13 };

	JBool sewerAiFunc(AiActor* aiActor, Actor* actor)
	{
		return JFALSE;
	}

	JBool sewerAiMsgFunc(s32 msg, AiActor* aiActor, Actor* actor)
	{
		return JFALSE;
	}

	JBool sewerEnemyFunc(AiActor* aiActor, Actor* actor)
	{
		return JFALSE;
	}

	Logic* sewerCreature_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		obj->flags &= ~OBJ_FLAG_NEEDS_TRANSFORM;
		obj->entityFlags = ETFLAG_AI_ACTOR;

		ActorLogic* logic = actor_setupActorLogic(obj, setupFunc);
		logic->alertSndSrc = s_alertSndSrc[ALERT_CREATURE];
		logic->fov = ANGLE_MAX;

		AiActor* aiActor = actor_createAiActor((Logic*)logic);
		aiActor->enemy.header.func = sewerAiFunc;
		aiActor->enemy.header.msgFunc = sewerAiMsgFunc;
		aiActor->hp = FIXED(36);
		aiActor->hurtSndSrc = s_agentSndSrc[AGENTSND_CREATURE_HURT];
		aiActor->dieSndSrc = s_agentSndSrc[AGENTSND_CREATURE_DIE];
		actorLogic_addActor(logic, (AiActor*)aiActor);

		ActorEnemy* enemyActor = actor_createEnemyActor((Logic*)logic);
		s_curEnemyActor = enemyActor;
		enemyActor->header.func = sewerEnemyFunc;
		enemyActor->timing.state0Delay = 1240;
		enemyActor->timing.state2Delay = 1240;
		enemyActor->meleeRange = FIXED(13);
		enemyActor->meleeDmg = FIXED(20);
		enemyActor->ua4 = FIXED(360);
		enemyActor->attackSecSndSrc = s_agentSndSrc[AGENTSND_CREATURE2];
		enemyActor->attackFlags = (enemyActor->attackFlags | 1) & 0xfffffffd;
		actorLogic_addActor(logic, (AiActor*)enemyActor);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)logic);
		actorSimple->target.speedRotation = 0x7fff;
		actorSimple->target.speed = FIXED(18);
		actorSimple->u3c = 58;
		actorSimple->startDelay = 72;
		actorSimple->anim.flags &= 0xfffffffe;
		actorLogic_addActor(logic, (AiActor*)actorSimple);

		Actor* actor = actor_create((Logic*)logic);	// eax
		logic->actor = actor;
		logic->animTable = s_sewerCreatureAnimTable;
		obj->entityFlags &= ~ETFLAG_SMART_OBJ;

		actor->collisionFlags = (actor->collisionFlags | 1) & 0xfffffffd;
		actor->physics.yPos = 0;
		actor->physics.botOffset = 0;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)logic;
	}
}  // namespace TFE_DarkForces