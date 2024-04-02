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
	// Entity
	enum EditorActionFlag
	{
		ACTION_NONE = 0,
		ACTION_PLACE = FLAG_BIT(0),
		ACTION_MOVE_X = FLAG_BIT(1),
		ACTION_MOVE_Y = FLAG_BIT(2),
		ACTION_MOVE_Z = FLAG_BIT(3),
		ACTION_DELETE = FLAG_BIT(4),
		ACTION_ROTATE = FLAG_BIT(5),

		ACTION_UNDO = FLAG_BIT(6),
		ACTION_REDO = FLAG_BIT(7),
		ACTION_SHOW_ALL_LABELS = FLAG_BIT(8),
	};

	// General
	extern bool s_singleClick;
	extern bool s_doubleClick;
	extern bool s_rightClick;
	extern bool s_leftMouseDown;

	// Right click.
	extern bool s_rightPressed;
	extern Vec2i s_rightMousePos;

	// Editor Hotkey Actions.
	extern u32 s_editActions;
	extern s32 s_rotationDelta;

	void handleHotkeys();
	bool getEditAction(u32 action);
}
