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
		ACTION_NONE = 0ull,
		ACTION_PLACE = FLAG_BIT64(0),
		ACTION_MOVE_X = FLAG_BIT64(1),
		ACTION_MOVE_Y = FLAG_BIT64(2),
		ACTION_MOVE_Z = FLAG_BIT64(3),
		ACTION_DELETE = FLAG_BIT64(4),
		ACTION_COPY   = FLAG_BIT64(5),
		ACTION_PASTE  = FLAG_BIT64(6),
		ACTION_ROTATE = FLAG_BIT64(7),

		ACTION_UNDO = FLAG_BIT64(8),
		ACTION_REDO = FLAG_BIT64(9),
		ACTION_SHOW_ALL_LABELS = FLAG_BIT64(10),

		ACTION_UNDEFINED = ~0x0ull
	};

	// Draw
	enum DrawActionFlag
	{
		DRAW_ACTION_NONE = 0,
		DRAW_ACTION_CURVE = FLAG_BIT(0),
		DRAW_ACTION_CANCEL = FLAG_BIT(1),
		DRAW_ACTION_FINISH = FLAG_BIT(2),
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
	extern u64 s_editActions;
	extern u32 s_drawActions;
	extern s32 s_rotationDelta;

	void handleHotkeys();
	bool getEditAction(u64 action);
}
