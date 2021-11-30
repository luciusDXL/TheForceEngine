#pragma once
//////////////////////////////////////////////////////////////////////
// Note this is still the original TFE code, reverse-engineered game
// UI code will not be available until future releases.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

namespace TFE_DarkForces
{
	// Return True if the menu is still running.
	// levelIndex will hold the selected level index (1 - 14).
	JBool agentMenu_update(s32* levelIndex);

	// Reset Presistent State.
	void agentMenu_resetState();

	// TODO: Factor out into its own file.
	void menu_blitCursor(s32 x, s32 y, u8* framebuffer);
}
