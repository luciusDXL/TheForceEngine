#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

namespace LevelEditor
{
	enum BrowseMode
	{
		BROWSE_TEXTURE,
		BROWSE_ENTITY,
	};

	void browserBegin(s32 offset);
	bool drawBrowser(BrowseMode mode = BROWSE_TEXTURE);
	void browserEnd();
	void browserScrollToSelection();

	void browserLoadIcons();
	void browserFreeIcons();

	bool textureSourcesUI();
}
