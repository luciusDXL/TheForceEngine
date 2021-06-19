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
	// This is a type of "Logic" and can be safety cast to Logic {}.
	struct SpriteAnimLogic
	{
		Logic logic;                        // Logic header.
		s32 firstFrame;                     // First frame (inclusive).
		s32 lastFrame;                      // Last frame (inclusive).
		s32 loopCount;                      // Number of loops before the animation ends. Set to <= 0 to loop forever.
		Tick delay;                         // Delay between frames, computed from the frame rate.
		Tick nextTick;                      // Next game tick to advance the animation to the next frame.
		SpriteCompleteFunc completeFunc;    // Function to call once the animation has completed (can be null).
	};

	// Given an object, create and setup the animation logic. Note - this will return null if the object is not a sprite.
	Logic* obj_setSpriteAnim(SecObject* obj);
	// Set the completeFunc for a instance of the logic. The complete function is called when the animation completes.
	void setAnimCompleteFunc(SpriteAnimLogic* logic, SpriteCompleteFunc func);
	// Setup the animation logic instance given the WAX animation index, first and last frames (inclusive) and loop count.
	// Note that loopCount = 0 means that the animation loops forever.
	void setupAnimationFromLogic(SpriteAnimLogic* logic, s32 animIndex, u32 firstFrame, u32 lastFrame, u32 loopCount);

	// Logic update function, handles the update of all sprite animations.
	void spriteAnimLogicFunc(s32 id);
	// Split out "task function" to delete an animation.
	// Was called with taskID = 1
	void spriteAnimDelete();
}  // namespace TFE_DarkForces