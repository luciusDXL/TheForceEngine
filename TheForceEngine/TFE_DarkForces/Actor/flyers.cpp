#include "flyers.h"
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
	static const s32 s_intDroidAnimTable[] =
	{ 0, 1, 2, 3, 4, 5, -1, 7, -1, -1, -1, -1, 12, -1, -1, -1 };
		
	u32 flyingActorFunc(AiActor* aiActor, Actor* actor)
	{
		ActorFlyer* flyingActor = (ActorFlyer*)aiActor;
		SecObject* obj = flyingActor->header.obj;
		if (flyingActor->state == 0)
		{
			flyingActor->state = 2;
			return s_curTick + random(flyingActor->delay);
		}
		else if (flyingActor->state == 1)
		{
			if (actor_handleSteps(actor, &flyingActor->target))
			{
				flyingActor->state = 0;
			}
			// 1d0f60:
			&flyingActor->target;	// eax
			SecObject* actorObj = actor->header.obj;		// edx
			if (actor_arrivedAtTarget(&flyingActor->target, actorObj))
			{
				flyingActor->state = 0;
			}
		}
		else if (flyingActor->state == 2)
		{
			RSector* sector = obj->sector;
			if (sector == s_playerObject->sector)
			{
				// Target a random height above or below the player y position: playerPosY - [-1.5, 6.5]
				s32 heightChange = random(FIXED(5)) - 0x18000;	// rand(5) - 1.5
				flyingActor->target.pos.y = s_eyePos.y - heightChange;
				flyingActor->target.flags |= 2;
				flyingActor->state = 1;
			}
			else
			{
				flyingActor->state = 0;
			}
		}

		actor->updateTargetFunc(actor, &flyingActor->target);
		return 0;
	}

	ActorFlyer* actor_createFlying(Logic* logic)
	{
		ActorFlyer* actor = (ActorFlyer*)level_alloc(sizeof(ActorFlyer));

		actor->target.speedRotation = 0;
		actor->target.speed = FIXED(4);
		actor->target.speedVert = FIXED(10);
		actor->delay = 72;
		actor->u68 = 0;
		actor->u6c = -1;
		actor->state = 2;
		actor->u40 = 728;
		actor->width = 5;
		actor->u4c = ONE_16;
		actor->nextTick = 0;
		actor->u70 = 0;
		actor->target.flags &= 0xfffffff0;
		actor->u5c &= 0xfffffffe;
		actor_initHeader(&actor->header, logic);
		actor->header.func = flyingActorFunc;
		actor->delay = 145;

		return actor;
	}

	Logic* intDroid_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		obj->flags |= OBJ_FLAG_HAS_COLLISION;

		ActorLogic* logic = actor_setupActorLogic(obj, setupFunc);
		logic->fov = 0x4000;	// 360 degrees
		logic->alertSndSrc = s_alertSndSrc[ALERT_INTDROID];

		AiActor* aiActor = actor_createAiActor((Logic*)logic);
		aiActor->hp = FIXED(45);
		aiActor->itemDropId = ITEM_POWER;
		aiActor->stopOnHit = JFALSE;
		aiActor->dieSndSrc = s_agentSndSrc[AGENTSND_SMALL_EXPLOSION];
		actorLogic_addActor(logic, (AiActor*)aiActor);

		// ActorEnemy is really a type of Actor.
		ActorEnemy* enemyActor = actor_createEnemyActor((Logic*)logic);
		enemyActor->fireOffset.y = 9830;
		enemyActor->projType = PROJ_PROBE_PROJ;
		enemyActor->attackSecSndSrc = s_agentSndSrc[AGENTSND_INTSTUN];
		enemyActor->attackPrimSndSrc = s_agentSndSrc[AGENTSND_PROBFIRE_11];
		enemyActor->meleeRange = FIXED(30);
		enemyActor->meleeDmg = FIXED(5);
		enemyActor->attackFlags |= 3;
		s_curEnemyActor = enemyActor;
		actorLogic_addActor(logic, (AiActor*)enemyActor);

		// ActorSimple is really a type of Actor.
		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)logic);
		actorSimple->target.speedRotation = 0;
		actorSimple->target.speed = FIXED(6);
		actorSimple->u3c = 116;
		actorSimple->anim.flags &= ~FLAG_BIT(0);
		actorSimple->startDelay = TICKS(2);	// (145.5)*2
		actorLogic_addActor(logic, (AiActor*)actorSimple);

		ActorFlyer* flyingActor = actor_createFlying((Logic*)logic);	// eax
		flyingActor->target.speedRotation = 0;
		flyingActor->target.speed = FIXED(4);
		flyingActor->target.speedVert = FIXED(10);
		flyingActor->target.speedRotation = 0x7fff;
		flyingActor->target.speed = FIXED(13);
		flyingActor->target.speedVert = FIXED(10);
		flyingActor->delay = 436;	// just shy of 3 seconds.
		actorLogic_addActor(logic, (AiActor*)flyingActor);	// eax, edx

		Actor* actor = actor_create((Logic*)logic);
		logic->actor = actor;
		actor->collisionFlags = (actor->collisionFlags & 0xfffffff8) | 4;
		actor->physics.yPos = FIXED(200);
		actor->physics.width = obj->worldWidth;

		// Setup the animation.
		logic->animTable = s_intDroidAnimTable;
		actor_setupInitAnimation();

		return (Logic*)logic;
	}
}  // namespace TFE_DarkForces