#include "barrel.h"
#include "../logic.h"
#include "aiActor.h"
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	static s32 s_barrelAnimTable[] =
	{ 0, -1, 1, 1, -1, 0 };

	void barrel_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		ActorLogic* logic = actor_setupActorLogic(obj, setupFunc);
		obj->flags |= OBJ_FLAG_HAS_COLLISION;

		logic->flags &= 0xfffffffb;
		logic->flags &= 0xfffffffe;
		logic->animTable = s_barrelAnimTable;

		AiActor* aiActor = actor_createAiActor((Logic*)logic);
		aiActor->actor.header.func = exploderFunc;
		aiActor->actor.header.msgFunc = exploderMsgFunc;
		aiActor->hp = FIXED(11);
		aiActor->anim.flags |= 2;
		actor_addLogicGameObj(logic, aiActor);

		Actor* actor = actor_create((Logic*)logic);
		logic->actor = actor;
		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor->updateFlags = (actor->updateFlags | 8) & 0xfffffff8;
		actor->speed = 0;
		actor->speedRotation = 0;

		aiActor->actor.physics.u1c = (aiActor->actor.physics.u1c | 8) & 0xfffffff8;
		aiActor->actor.physics.botOffset = 0;
		aiActor->actor.physics.height = 0;
	}
}  // namespace TFE_DarkForces