#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/filestream.h>
#include <vector>
#include <string>

namespace LevelEditor
{
	enum ContextMenu
	{
		CONTEXTMENU_NONE = 0,
		CONTEXTMENU_VIEWPORT
	};
	extern ContextMenu s_contextMenu;
}
