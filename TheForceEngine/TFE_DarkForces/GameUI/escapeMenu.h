#pragma once
//////////////////////////////////////////////////////////////////////
// Note this is still the original TFE code, reverse-engineered game
// UI code will not be available until future releases.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

enum EscapeMenuAction
{
	ESC_CONTINUE = 0,	// the menu is still open.
	ESC_RETURN,			// return to the game.
	ESC_ABORT_OR_NEXT,	// abort the current mission or move on to the next.
	ESC_QUIT,			// quit the game.
	ESC_CONFIG,			// configuration menu.
	ESC_COUNT
};

namespace TFE_DarkForces
{
	// Opens the escape menu, which sets up the background.
	void escapeMenu_open(u8* framebuffer, u8* palette);

	// Returns JTRUE if the escape menu is open.
	JBool escapeMenu_isOpen();

	// Returns ESC_CONTINUE if the menu is still running,
	// otherwise the menu has been closed.
	EscapeMenuAction escapeMenu_update();

	// Reset Persistent state
	void escapeMenu_resetState();
}
