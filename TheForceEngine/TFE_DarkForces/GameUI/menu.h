#pragma once
//////////////////////////////////////////////////////////////////////
// General Dark Forces shared menu code.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

namespace TFE_DarkForces
{
	void menu_init();
	void menu_destroy();
	void menu_resetState();

	void menu_handleMousePosition();
	void menu_blitCursor(s32 x, s32 y, u8* framebuffer);
	void menu_resetCursor();
	u8*  menu_startupDisplay();

	// Variables.
	extern Vec2i s_cursorPos;
	extern s32 s_buttonPressed;
	extern JBool s_buttonHover;
}
