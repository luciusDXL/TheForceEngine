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
#include <TFE_Input/input.h>
#include <vector>
#include <string>

namespace LevelEditor
{
	// Draw
	enum DrawActionFlag
	{
		DRAW_ACTION_NONE = 0,
		DRAW_ACTION_CURVE = FLAG_BIT(0),
		DRAW_ACTION_CANCEL = FLAG_BIT(1),
		DRAW_ACTION_FINISH = FLAG_BIT(2),
	};

	enum ShortcutId
	{
		SHORTCUT_PLACE = 0,
		SHORTCUT_MOVE_X,
		SHORTCUT_MOVE_Y,
		SHORTCUT_MOVE_Z,
		SHORTCUT_MOVE_NORMAL,
		SHORTCUT_COPY_TEXTURE,
		SHORTCUT_SET_TEXTURE,
		SHORTCUT_SET_SIGN,
		SHORTCUT_AUTO_ALIGN,
		SHORTCUT_MOVE_TO_FLOOR,
		SHORTCUT_MOVE_TO_CEIL,
		SHORTCUT_DELETE,
		SHORTCUT_COPY,
		SHORTCUT_PASTE,
		SHORTCUT_ROTATE,
		SHORTCUT_REDUCE_CURVE_SEGS,
		SHORTCUT_INCREASE_CURVE_SEGS,
		SHORTCUT_RESET_CURVE_SEGS,
		SHORTCUT_SHOW_ALL_LABELS,
		SHORTCUT_UNDO,
		SHORTCUT_REDO,

		SHORTCUT_COUNT,
		SHORTCUT_NONE = SHORTCUT_COUNT,
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
	extern u32 s_drawActions;

	void handleHotkeys();

	void clearKeyboardShortcuts();
	void setDefaultKeyboardShortcuts();
	void addKeyboardShortcut(ShortcutId id, KeyboardCode code = KEY_UNKNOWN, KeyModifier mod = KEYMOD_NONE);
	void removeKeyboardShortcut(ShortcutId id);
	const char* getKeyboardShortcutDesc(ShortcutId id);
	KeyboardCode getShortcutKeyboardCode(ShortcutId id);
	KeyModifier getShortcutKeyMod(ShortcutId id);
	bool isShortcutPressed(ShortcutId shortcutId, u32 allowedKeyMods = (1 << KEYMOD_SHIFT) | (1 << KEYMOD_ALT));
	bool isShortcutRepeat(ShortcutId shortcutId, u32 allowedKeyMods = (1 << KEYMOD_SHIFT) | (1 << KEYMOD_ALT));
	bool isShortcutHeld(ShortcutId shortcutId, u32 allowedKeyMods = (1 << KEYMOD_SHIFT) | (1 << KEYMOD_ALT));
}
