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
		aiActor->enemy.header.func = exploderFunc;
		aiActor->enemy.header.msgFunc = exploderMsgFunc;
		aiActor->hp = FIXED(11);
		aiActor->enemy.anim.flags |= AFLAG_READY;
		actorLogic_addActor(logic, aiActor);

		Actor* actor = actor_create((Logic*)logic);
		logic->actor = actor;
		actor->collisionFlags |= 1;
		actor->physics.width = obj->worldWidth;
		actor->target.flags = (actor->target.flags | 8) & 0xfffffff8;
		actor->target.speed = 0;
		actor->target.speedRotation = 0;

		ActorTarget* target = &aiActor->enemy.target;
		target->flags = (target->flags | 8) & 0xfffffff8;
		target->speed = 0;
		target->speedRotation = 0;
	}
}  // namespace TFE_DarkForces