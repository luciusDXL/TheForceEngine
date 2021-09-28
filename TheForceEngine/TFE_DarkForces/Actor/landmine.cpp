#include "landmine.h"
#include "../logic.h"
#include "aiActor.h"
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	// Animation Table
	static const s32 s_landmineAnimTable[] =
	{ 0, -1, 1, 1, -1, 0 };
		
	void landmine_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorLogic* actorLogic = actor_setupActorLogic(obj, setupFunc);
		actorLogic->flags &= ~4;
		actorLogic->flags &= ~1;
		actorLogic->animTable = s_landmineAnimTable;
		
		AiActor* aiActor = actor_createAiActor((Logic*)actorLogic);
		aiActor->enemy.header.func  = exploderFunc;
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
		actorLogic->animTable = s_landmineAnimTable;
	}
}  // namespace TFE_DarkForces