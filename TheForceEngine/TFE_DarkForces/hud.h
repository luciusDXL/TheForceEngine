#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces HUD - 
// This handles the heads up display and on-screen messages during
// gameplay.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/core_math.h>

using namespace TFE_Jedi;

struct OffScreenBuffer;
struct ScreenRect;

namespace TFE_DarkForces
{
	void hud_sendTextMessage(s32 msgId);
	void hud_clearMessage();

	void hud_loadGameMessages();
	void hud_loadGraphics();

	void hud_startup();
	void hud_initAnimation();
	void hud_setupToggleAnim1(JBool enable);
	void hud_toggleDataDisplay();

	void hud_drawMessage(u8* framebuffer);
	void hud_drawAndUpdate(u8* framebuffer);
	void hud_drawElementToScreen(OffScreenBuffer* elem, ScreenRect* rect, s32 x0, s32 y0, u8* framebuffer);

	extern fixed16_16 s_flashEffect;
	extern fixed16_16 s_healthDamageFx;
	extern fixed16_16 s_shieldDamageFx;
	extern s32 s_secretsFound;
	extern s32 s_secretsPercent;
	extern JBool s_showData;
}  // namespace TFE_DarkForces