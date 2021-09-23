#include "mousebot.h"
#include "aiActor.h"
#include "../logic.h"
#include <TFE_Game/igame.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	struct MouseBotResources
	{
		WaxFrame* deadFrame;
		SoundSourceID sound0;
		SoundSourceID sound1;
		SoundSourceID sound2;
	};
	static MouseBotResources s_mouseBotRes = { 0 };

	struct MouseBot
	{
		Logic logic;
		PhysicsActor actor;
	};

	void mouseBotTaskFunc(s32 id)
	{
		task_begin;
		while (id != -1)
		{
			task_yield(TASK_NO_DELAY);
		}
		task_end;
	}

	void mouseBotLogicCleanupFunc(Logic* logic)
	{
	}

	Logic* mousebot_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		if (!s_mouseBotRes.deadFrame)
		{
			s_mouseBotRes.deadFrame = TFE_Sprite_Jedi::getFrame("dedmouse.fme");
		}
		if (!s_mouseBotRes.sound0)
		{
			s_mouseBotRes.sound0 = sound_Load("eeek-1.voc");
		}
		if (!s_mouseBotRes.sound1)
		{
			s_mouseBotRes.sound1 = sound_Load("eeek-2.voc");
		}
		if (!s_mouseBotRes.sound2)
		{
			s_mouseBotRes.sound2 = sound_Load("eeek-3.voc");
		}

		MouseBot* mouseBot = (MouseBot*)level_alloc(sizeof(MouseBot));
		memset(mouseBot, 0, sizeof(MouseBot));

		Task* mouseBotTask = createTask("mouseBot", mouseBotTaskFunc);
		task_setUserData(mouseBotTask, mouseBot);

		obj->flags &= ~OBJ_FLAG_ENEMY;
		obj->entityFlags = ETFLAG_AI_ACTOR;
		mouseBot->logic.obj = obj;

		PhysicsActor* physActor = &mouseBot->actor;
		physActor->ue4 = -1;
		physActor->hp = FIXED(10);
		physActor->task2 = mouseBotTask;
		physActor->ue8 = 0;
		physActor->actor.header.obj = obj;
		physActor->actor.physics.obj = obj;
		actor_addPhysicsActorToWorld(physActor);
		actor_setupSmartObj(&physActor->actor);

		JediModel* model = obj->model;
		fixed16_16 width = model->radius + ONE_16;
		obj->worldWidth = width;
		obj->worldHeight = width >> 1;

		physActor->actor.physics.botOffset = 0;
		physActor->actor.physics.yPos = 0;
		physActor->actor.collisionFlags = (physActor->actor.collisionFlags | 3) & 0xfffffffb;
		physActor->actor.physics.height = obj->worldHeight + HALF_16;
		physActor->actor.speed = FIXED(22);
		physActor->actor.speedRotation = FIXED(3185);
		physActor->actor.updateFlags &= 0xfffffff0;
		physActor->actor.pitch = obj->pitch;
		physActor->actor.yaw = obj->yaw;
		physActor->actor.roll = obj->roll;

		obj_addLogic(obj, (Logic*)mouseBot, mouseBotTask, mouseBotLogicCleanupFunc);
		if (setupFunc)
		{
			setupFunc = nullptr;
		}
		return (Logic*)mouseBot;
	}
}  // namespace TFE_DarkForces