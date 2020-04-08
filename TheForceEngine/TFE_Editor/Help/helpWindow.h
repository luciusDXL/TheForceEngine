#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Renderer/renderer.h>

enum Help
{
	HELP_ABOUT = 0,
	HELP_MANUAL,
	HELP_COUNT,
	HELP_NONE = HELP_COUNT,
};

namespace HelpWindow
{
	void init();
	bool show(Help help, u32 width, u32 height);
	bool isMinimized();
	bool isOpen();
}
