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
		aiActor->actor.header.func  = exploderFunc;
		aiActor->actor.header.msgFunc = exploderMsgFunc;
		aiActor->hp = FIXED(20);
		aiActor->anim.flags |= 2;
		actor_addLogicGameObj(actorLogic, aiActor);

		Actor* actor = actor_create((Logic*)actorLogic);
		actorLogic->actor = actor;
		actor->collisionFlags |= 1;
		// This was cleared to 0 in createProjectile()
		actor->physics.width = obj->worldWidth;

		aiActor->actor.physics.u1c = (aiActor->actor.physics.u1c | 8) & 0xfffffff8;
		aiActor->actor.physics.botOffset = 0;
		aiActor->actor.physics.height = 0;

		actor->updateFlags = 8;
		actor->speed = 0;
		actor->speedRotation = 0;
		
		actorLogic->flags &= ~1;
		actorLogic->animTable = s_landmineAnimTable;
	}
}  // namespace TFE_DarkForces