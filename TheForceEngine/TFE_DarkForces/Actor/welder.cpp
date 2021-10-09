#include "welder.h"
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
	struct Welder
	{
		Logic logic;
		PhysicsActor actor;

		s32 u104;
		s32 u108;
		angle14_16 yaw;
		angle14_16 pitch;
	};

	static Wax* s_welderSpark = nullptr;
	static SoundSourceID s_weld2SoundSrcId = NULL_SOUND;
	static SoundSourceID s_weld1SoundSrcId = NULL_SOUND;
	static SoundSourceID s_weldShotSoundSrcId = NULL_SOUND;
	static SoundSourceID s_weldHurtSoundSrcId = NULL_SOUND;
	static SoundSourceID s_weldDieSoundSrcId  = NULL_SOUND;

	static Welder* s_curWelder = nullptr;
	static s32 s_welderNum = 0;

	void welderTaskFunc(MessageType msg)
	{
		struct LocalContext
		{
			Welder* welder;
			PhysicsActor* physicsActor;
		};
		task_begin_ctx;

		local(welder) = (Welder*)task_getUserData();
		local(physicsActor) = &local(welder)->actor;
		while (local(physicsActor)->alive)
		{
		}  // while (alive)

		// Dead:
		while (msg != MSG_RUN_TASK)
		{
			task_yield(TASK_NO_DELAY);
		}
		actor_removePhysicsActorFromWorld(local(physicsActor));
		deleteLogicAndObject((Logic*)local(welder));
		level_free(local(welder));

		task_end;
	}

	void welderCleanupFunc(Logic* logic)
	{
		Welder* welder = (Welder*)logic;

		actor_removePhysicsActorFromWorld(&welder->actor);
		deleteLogicAndObject(logic);
		level_free(welder);
		task_free(welder->actor.actorTask);
	}

	Logic* welder_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		if (!s_welderSpark)
		{
			s_welderSpark = TFE_Sprite_Jedi::getWax("spark.wax");
		}
		if (!s_weld2SoundSrcId)
		{
			s_weld2SoundSrcId = sound_Load("weld-2.voc");
		}
		if (!s_weld1SoundSrcId)
		{
			s_weld1SoundSrcId = sound_Load("weld-1.voc");
		}
		if (!s_weldShotSoundSrcId)
		{
			s_weldShotSoundSrcId = sound_Load("weldsht1.voc");
		}
		if (!s_weldHurtSoundSrcId)
		{
			s_weldHurtSoundSrcId = sound_Load("weldhrt.voc");
		}
		if (!s_weldDieSoundSrcId)
		{
			s_weldDieSoundSrcId = sound_Load("weld-die.voc");
		}

		Welder* welder = (Welder*)level_alloc(sizeof(Welder));
		memset(welder, 0, sizeof(Welder));

		// Give the name of the task a number so I can tell them apart when debugging.
		char name[32];
		sprintf(name, "Welder%d", s_welderNum);
		s_welderNum++;
		Task* task = createSubTask(name, welderTaskFunc);
		task_setUserData(task, welder);

		obj->entityFlags = ETFLAG_AI_ACTOR;
		PhysicsActor* physicsActor = &welder->actor;
		physicsActor->alive = JTRUE;
		physicsActor->hp = FIXED(130);
		physicsActor->state = 0;
		physicsActor->actorTask = task;
		welder->logic.obj = obj;
		welder->yaw = obj->yaw;
		welder->pitch = obj->pitch;
		welder->u104 = 0;
		welder->u108 = 0;
		actor_addPhysicsActorToWorld(physicsActor);
		CollisionInfo* physics = &physicsActor->actor.physics;

		ActorTarget* target = &physicsActor->actor.target;
		obj->worldWidth  = FIXED(10);
		obj->worldHeight = FIXED(4);
		physicsActor->actor.header.obj = obj;
		physics->obj = obj;
		physics->width = obj->worldWidth;
		physics->wall = nullptr;
		physics->u24 = 0;
		physics->botOffset = 0;
		physics->yPos = 0;
		physics->height = obj->worldHeight + HALF_16;

		physicsActor->actor.delta = { 0, 0, 0 };
		physicsActor->actor.collisionFlags &= 0xfffffff8;

		target->speed = 0;
		target->speedRotation = 4551;	// ~100 degrees/second.
		target->flags &= 0xfffffff0;
		target->pitch = obj->pitch;
		target->yaw   = obj->yaw;
		target->roll  = obj->roll;
		obj_addLogic(obj, &welder->logic, task, welderCleanupFunc);

		if (setupFunc)
		{
			*setupFunc = nullptr;
		}
		return (Logic*)welder;
	}
}  // namespace TFE_DarkForces