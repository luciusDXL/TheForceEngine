#include "animLogic.h"
#include "time.h"
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/InfSystem/message.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	Allocator* s_spriteAnimList = nullptr;
	Task* s_spriteAnimTask = nullptr;

	void setSpriteAnimation(Task* spriteAnimTask, Allocator* spriteAnimAlloc)
	{
		s_spriteAnimTask = spriteAnimTask;
		s_spriteAnimList = spriteAnimAlloc;
	}
	
	void spriteAnimLogicCleanupFunc(Logic* logic)
	{
		SpriteAnimLogic* animLogic = (SpriteAnimLogic*)logic;
		Task* completeTask = animLogic->completeTask;
		// The original code would loop and yield while not in the main task.
		// For TFE, that should be unnecessary.
		deleteLogicAndObject(logic);
		allocator_deleteItem(s_spriteAnimList, logic);
		if (completeTask)
		{
			task_runAndReturn(completeTask, MSG_SEQ_COMPLETE);
		}
	}

	// Logic update function, handles the update of all sprite animations.
	void spriteAnimLogicFunc(MessageType msg)
	{
		struct LocalContext
		{
			SpriteAnimLogic* anim;
			SecObject* obj;
			s32 count;
		};
		task_begin_ctx;
		while (msg != MSG_FREE_TASK)
		{
			if (msg == MSG_FREE)
			{
				deleteLogicAndObject((Logic*)s_msgTarget);
				allocator_deleteItem(s_spriteAnimList, s_msgTarget);
			}
			else           // otherwise update all of the animations in the list.
			{
				taskCtx->anim = (SpriteAnimLogic*)allocator_getHead(s_spriteAnimList);
				taskCtx->count = 0;
				while (taskCtx->anim)
				{
					taskCtx->count++;
					if (taskCtx->anim->nextTick < s_curTick)
					{
						taskCtx->obj = taskCtx->anim->logic.obj;
						taskCtx->obj->frame++;
						taskCtx->anim->nextTick += taskCtx->anim->delay;

						if (taskCtx->obj->frame > taskCtx->anim->lastFrame)
						{
							taskCtx->anim->loopCount--;
							if (taskCtx->anim->loopCount == 0)
							{
								// Only delete from the main task.
								task_waitWhileIdNotZero(TASK_NO_DELAY);

								// Finally delete the logic and call the complete function.
								Task* completeTask = taskCtx->anim->completeTask;

								deleteLogicAndObject((Logic*)taskCtx->anim);	// This just deletes the Logic from the list
								if (completeTask)
								{
									task_runAndReturn(completeTask, MSG_SEQ_COMPLETE);
								}
								allocator_deleteItem(s_spriteAnimList, taskCtx->anim); // This deletes the logic itself.
							}
							else
							{
								taskCtx->obj->frame = taskCtx->anim->firstFrame;
							}
						}
					}
					taskCtx->anim = (SpriteAnimLogic*)allocator_getNext(s_spriteAnimList);
				}
			}
			// There must be a yield in an endless while() loop.
			// If there are no more animated logics, then make the task go to sleep.
			task_yield(taskCtx->count ? TASK_NO_DELAY : TASK_SLEEP);
		}
		// Once we break out of the while loop, the task gets removed.
		task_end;
	}

	Logic* obj_setSpriteAnim(SecObject* obj)
	{
		if (!(obj->type & OBJ_TYPE_SPRITE))
		{
			return nullptr;
		}

		// Create an allocator if one is not already setup.
		if (!s_spriteAnimTask)
		{
			s_spriteAnimTask = createTask("sprite animation", spriteAnimLogicFunc);
		}
		if (!s_spriteAnimList)
		{
			s_spriteAnimList = allocator_create(sizeof(SpriteAnimLogic));
		}

		SpriteAnimLogic* anim = (SpriteAnimLogic*)allocator_newItem(s_spriteAnimList);
		const WaxAnim* waxAnim = WAX_AnimPtr(obj->wax, 0);

		obj_addLogic(obj, (Logic*)anim, s_spriteAnimTask, spriteAnimLogicCleanupFunc);

		anim->firstFrame = 0;
		anim->lastFrame = waxAnim->frameCount - 1;
		anim->loopCount = 0;
		anim->completeTask = nullptr;
		anim->delay = time_frameRateToDelay(waxAnim->frameRate);
		anim->nextTick = s_curTick + anim->delay;

		obj->frame = 0;
		obj->anim = 0;
		task_makeActive(s_spriteAnimTask);

		return (Logic*)anim;
	}

	void setAnimCompleteTask(SpriteAnimLogic* logic, Task* completeTask)
	{
		logic->completeTask = completeTask;
	}

	void setupAnimationFromLogic(SpriteAnimLogic* animLogic, s32 animIndex, u32 firstFrame, u32 lastFrame, u32 loopCount)
	{
		SecObject* obj = animLogic->logic.obj;
		obj->anim  = animIndex;
		obj->frame = firstFrame;
	
		const WaxAnim* anim = WAX_AnimPtr(obj->wax, animIndex);
		const s32 animLastFrame = anim->frameCount - 1;
		if (lastFrame >= (u32)animLastFrame)
		{
			lastFrame = animLastFrame;
		}

		animLogic->firstFrame = firstFrame;
		animLogic->lastFrame  = lastFrame;
		animLogic->delay      = time_frameRateToDelay(anim->frameRate);
		animLogic->loopCount  = loopCount;
	}

}  // TFE_DarkForces