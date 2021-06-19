#include "animLogic.h"
#include "time.h"
#include <TFE_Memory/allocator.h>
#include <TFE_InfSystem/message.h>

using namespace TFE_Memory;
using namespace TFE_Message;

namespace TFE_DarkForces
{
	Allocator* s_spriteAnimList = nullptr;
	
	void spriteAnimLogicCleanupFunc(Logic* logic)
	{
		SpriteAnimLogic* animLogic = (SpriteAnimLogic*)logic;
		// The original code would loop and yield while not in the main task.
		// For TFE, that should be unnecessary.
		deleteLogicAndObject(logic);
		if (animLogic->completeFunc)
		{
			animLogic->completeFunc();
		}
		allocator_deleteItem(s_spriteAnimList, logic);
	}

	// Logic update function, handles the update of all sprite animations.
	void spriteAnimLogicFunc(s32 id)
	{
		SpriteAnimLogic* anim = (SpriteAnimLogic*)allocator_getHead(s_spriteAnimList);
		while (anim)
		{
			if (anim->nextTick < s_curTick)
			{
				SecObject* obj = anim->logic.obj;
				obj->frame++;
				anim->nextTick += anim->delay;

				if (obj->frame > anim->lastFrame)
				{
					anim->loopCount--;
					if (anim->loopCount == 0)
					{
						deleteLogicAndObject((Logic*)anim);
						if (anim->completeFunc)
						{
							anim->completeFunc();
						}
						allocator_deleteItem(s_spriteAnimList, obj);
					}
					else
					{
						obj->frame = anim->firstFrame;
					}
				}
			}
			anim = (SpriteAnimLogic*)allocator_getNext(s_spriteAnimList);
		}
	}

	// Split out "task function" to delete an animation.
	// Was called with taskID = 1
	void spriteAnimDelete()
	{
		deleteLogicAndObject((Logic*)s_msgTarget);
		allocator_deleteItem(s_spriteAnimList, s_msgTarget);
	}

	Logic* obj_setSpriteAnim(SecObject* obj)
	{
		if (!(obj->type & OBJ_TYPE_SPRITE))
		{
			return nullptr;
		}

		// Create an allocator if one is not already setup.
		if (!s_spriteAnimList)
		{
			s_spriteAnimList = allocator_create(sizeof(SpriteAnimLogic));
		}

		SpriteAnimLogic* anim = (SpriteAnimLogic*)allocator_newItem(s_spriteAnimList);
		const WaxAnim* waxAnim = WAX_AnimPtr(obj->wax, 0);

		obj_addLogic(obj, (Logic*)anim, spriteAnimLogicFunc, spriteAnimLogicCleanupFunc);

		anim->firstFrame = 0;
		anim->lastFrame = waxAnim->frameCount - 1;
		anim->loopCount = 0;
		anim->completeFunc = nullptr;
		anim->delay = time_frameRateToDelay(waxAnim->frameRate);
		anim->nextTick = s_curTick + anim->delay;

		obj->frame = 0;
		obj->anim = 0;

		return (Logic*)anim;
	}

	void setAnimCompleteFunc(SpriteAnimLogic* logic, SpriteCompleteFunc func)
	{
		logic->completeFunc = func;
	}

	void setupAnimationFromLogic(SpriteAnimLogic* animLogic, s32 animIndex, u32 firstFrame, u32 lastFrame, u32 loopCount)
	{
		SecObject* obj = animLogic->logic.obj;
		obj->anim  = animIndex;
		obj->frame = firstFrame;
	
		const WaxAnim* anim = WAX_AnimPtr(obj->wax, animIndex);
		const s32 animLastFrame = anim->frameCount - 1;
		if (lastFrame >= animLastFrame)
		{
			lastFrame = animLastFrame;
		}

		animLogic->firstFrame = firstFrame;
		animLogic->lastFrame  = lastFrame;
		animLogic->delay      = time_frameRateToDelay(anim->frameRate);
		animLogic->loopCount  = loopCount;
	}

}  // TFE_DarkForces