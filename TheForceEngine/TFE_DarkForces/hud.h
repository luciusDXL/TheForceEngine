#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces HUD - 
// This handles the heads up display and on-screen messages during
// gameplay.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/core_math.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	void hud_sendTextMessage(s32 msgId);

	extern fixed16_16 s_flashEffect;
}  // namespace TFE_DarkForces