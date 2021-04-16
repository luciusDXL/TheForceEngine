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
		anim->u2c = 0;
		anim->delay = Tick(SECONDS_TO_TICKS_ROUNDED / f32(waxAnim->frameRate));
		anim->nextTick = s_curTick + anim->delay;

		obj->frame = 0;
		obj->anim = 0;

		return (Logic*)anim;
	}

}  // TFE_DarkForces