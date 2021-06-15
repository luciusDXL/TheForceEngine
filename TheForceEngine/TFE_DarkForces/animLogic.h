#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Pickup - 
// Items that can be picked up such as ammo, keys, extra lives,
// and weapons.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Level/rsector.h>
#include <TFE_Level/robject.h>
#include "logic.h"
#include "time.h"

typedef void(*SpriteCompleteFunc)();

namespace TFE_DarkForces
{
	struct SpriteAnimLogic
	{
		Logic logic;

		s32 firstFrame;
		s32 lastFrame;
		s32 loopCount;
		Tick delay;
		Tick nextTick;
		SpriteCompleteFunc completeFunc;
	};

	Logic* obj_setSpriteAnim(SecObject* obj);
	void setAnimCompleteFunc(SpriteAnimLogic* logic, SpriteCompleteFunc func);
	void setupAnimationFromLogic(SpriteAnimLogic* logic, s32 animIndex, u32 firstFrame, u32 lastFrame, u32 loopCount);

	// Logic update function, handles the update of all sprite animations.
	void spriteAnimLogicFunc();
	// Split out "task function" to delete an animation.
	// Was called with taskID = 1
	void spriteAnimDelete();
}  // namespace TFE_DarkForces