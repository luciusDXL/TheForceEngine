#include "officer.h"
#include "aiActor.h"
#include "../logic.h"
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/item.h>
#include <TFE_DarkForces/random.h>
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
	static s32 s_officerAnimTable[] =
	{ 0, 6, 2, 3, 4, 5 };

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
		ActorLogic* logic = actor_setupActorLogic(obj, setupFunc);
		logic->flags |= FLAG_BIT(4);
		logic->alertSndSrc = 0;

		AiActor* aiActor = actor_createAiActor((Logic*)logic);
		aiActor->hp = FIXED(9);
		aiActor->hurtSndSrc = s_agentSndSrc[AGENTSND_STORM_HURT];
		aiActor->dieSndSrc  = s_agentSndSrc[AGENTSND_STORM_DIE];
		aiActor->itemDropId = officer_getItemDropId(logicId);
		actorLogic_addActor(logic, aiActor);

		ActorEnemy* enemyActor = actor_createEnemyActor((Logic*)logic);
		enemyActor->attackFlags = (enemyActor->attackFlags & 0xfffffffc) | 2;
		enemyActor->u88 = 1;
		enemyActor->attackPrimSndSrc = s_pistolSndSrc;
		s_curEnemyActor = enemyActor;
		actorLogic_addActor(logic, (AiActor*)enemyActor);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)logic);
		actorSimple->target.speedRotation = HALF_16 - 1;
		actorSimple->target.speed = FIXED(7);
		actorSimple->timing.delay = 0;
		actorSimple->anim.prevTick &= 0xfffffffe;
		actorSimple->timing.state2Delay = TICKS(2);
		actorLogic_addActor(logic, (AiActor*)actorSimple);

		Actor* actor = actor_create((Logic*)logic);
		logic->actor = actor;
		logic->animTable = s_officerAnimTable;
		s_curLogic = (Logic*)logic;

		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)logic;
	}
}  // namespace TFE_DarkForces