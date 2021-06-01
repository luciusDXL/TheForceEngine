#include "animLogic.h"
#include "time.h"
#include <TFE_Memory/allocator.h>
using namespace TFE_Memory;

namespace TFE_DarkForces
{
	Allocator* s_spriteAnimList = nullptr;
	Task* s_spriteAnimTask = nullptr;
	
	void spriteAnimLogicFunc()
	{
		// TODO
	}

	Logic* obj_setSpriteAnim(SecObject* obj)
	{
		if (!(obj->type & OBJ_TYPE_SPRITE))
		{
			return nullptr;
		}

		if (!s_spriteAnimList)
		{
			s_spriteAnimList = allocator_create(sizeof(SpriteAnimLogic));
		}

		SpriteAnimLogic* anim = (SpriteAnimLogic*)allocator_newItem(s_spriteAnimList);
		const WaxAnim* waxAnim = WAX_AnimPtr(obj->wax, 0);

		obj_addLogic(obj, (Logic*)anim, s_spriteAnimTask, spriteAnimLogicFunc);

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

	void setupAnimationFromLogic(SpriteAnimLogic* logic, s32 animIndex, u32 firstFrame, u32 lastFrame, u32 loopCount)
	{
		SecObject* obj = logic->logic.obj;
		obj->anim = animIndex;
		Wax* wax = obj->wax;
		obj->frame = firstFrame;
		logic->firstFrame = firstFrame;
		WaxAnim* anim = WAX_AnimPtr(wax, animIndex);
		s32 animLastFrame = anim->frameCount - 1;
		if (lastFrame >= lastFrame)
		{
			lastFrame = animLastFrame;
		}
		logic->lastFrame = lastFrame;
		logic->delay = u32(145.5f / float(anim->frameRate));
		logic->loopCount = loopCount;
	}

}  // TFE_DarkForces