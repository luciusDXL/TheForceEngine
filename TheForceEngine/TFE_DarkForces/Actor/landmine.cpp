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

	JBool landAiMineFunc(Actor* actor0, Actor* actor1)
	{
		return JFALSE;
	}

	void landmine_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorLogic* actorLogic = actor_setupActorLogic(obj, setupFunc);
		actorLogic->flags &= ~4;
		actorLogic->flags &= ~1;
		actorLogic->animTable = s_landmineAnimTable;
		
		AiActor* aiActor = actor_createAiActor((Logic*)actorLogic);
		aiActor->baseObj.u68 |= 2;
		aiActor->baseObj.header.func  = landAiMineFunc;
		aiActor->baseObj.header.func2 = landAiMineFunc;
		aiActor->hp = FIXED(20);
		actor_addLogicGameObj(actorLogic, (ActorHeader*)aiActor);

		Actor* actor = actor_create((Logic*)actorLogic);
		actorLogic->actor = actor;
		actor->collisionFlags |= 1;
		// This was cleared to 0 in createProjectile()
		actor->physics.width = obj->worldWidth;

		aiActor->baseObj.u38 |= 8;
		aiActor->baseObj.u38 &= ~(1 | 2 | 4);
		aiActor->baseObj.flags = 0;
		aiActor->baseObj.width = 0;

		actor->updateFlags = 8;
		actor->speed = 0;
		actor->speedRotation = 0;
		
		actorLogic->flags &= ~1;
		actorLogic->animTable = s_landmineAnimTable;
	}
}  // namespace TFE_DarkForces