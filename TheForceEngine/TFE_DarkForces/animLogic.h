#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Animation Logic-
// Handles cyclic "fire and forget" sprite animations.
// This includes animated sprite objects, explosions, and hit effects.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include "logic.h"
#include "time.h"

namespace TFE_DarkForces
{
	// This is a type of "Logic" and can be safety cast to Logic {}.
	struct SpriteAnimLogic
	{
		Logic logic;           // Logic header.
		s32 firstFrame;        // First frame (inclusive).
		s32 lastFrame;         // Last frame (inclusive).
		s32 loopCount;         // Number of loops before the animation ends. Set to <= 0 to loop forever.
		Tick delay;            // Delay between frames, computed from the frame rate.
		Tick nextTick;         // Next game tick to advance the animation to the next frame.
		Task* completeTask;    // Function to call once the animation has completed (can be null).
	};

	// Given an object, create and setup the animation logic. Note - this will return null if the object is not a sprite.
	Logic* obj_setSpriteAnim(SecObject* obj);
	// Set the completeFunc for a instance of the logic. The complete function is called when the animation completes.
	void setAnimCompleteTask(SpriteAnimLogic* logic, Task* completeTask);
	// Setup the animation logic instance given the WAX animation index, first and last frames (inclusive) and loop count.
	// Note that loopCount = 0 means that the animation loops forever.
	void setupAnimationFromLogic(SpriteAnimLogic* logic, s32 animIndex, u32 firstFrame, u32 lastFrame, u32 loopCount);

	void setSpriteAnimation(Task* spriteAnimTask, Allocator* spriteAnimAlloc);

	// Serialization
	void animLogic_serialize(Logic*& logic, SecObject* obj, Stream* stream);
}  // namespace TFE_DarkForces