#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/levelAsset.h>

namespace TFE_GameConstants
{
	static f32 c_step = 1.0f / 60.0f;
	//static f32 c_step = 1.0f / 35.0f;
	static f32 c_minInfDelay = 0.0f;

	static const f32 c_walkSpeed = 32.0f;
	static const f32 c_runSpeed = 64.0f;
	static const f32 c_crouchWalk = 17.0f;
	static const f32 c_crouchRun = 34.0f;

	static const f32 c_walkSpeedJump = 22.50f;
	static const f32 c_runSpeedJump = 45.00f;
	static const f32 c_crouchWalkJump = 11.95f;
	static const f32 c_crouchRunJump = 23.91f;

	static const f32 c_jumpImpulse = -36.828f;
	static const f32 c_gravityAccel = 111.6f;
	static const f32 c_gravityAccelStep = c_gravityAccel * c_step;	// gravity acceleration per step.

	static const f32 c_standingEyeHeight  = -380108.0f / 65536.0f;  // Kyle's standing eye height is ~5.8 (-380108/65536 = -5.7999878)
	static const f32 c_crouchingEyeHeight = -16.0f * 0.125f;		// Kyle's crouching eye height is 16 texels.
	static const f32 c_standingHeight = 6.8f;
	static const f32 c_crouchingHeight = 3.0f;
	static const f32 c_crouchOnSpeed = 12.0f;	// DFU's per second.
	static const f32 c_crouchOffSpeed = 12.0f;

	static const f32 c_yellThreshold = 40.0f;

	static const s32 c_levelLoadInputDelay = 15;
}
