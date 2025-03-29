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
		// Insert/Delete/Copy/Paste
		SHORTCUT_PLACE = 0,
		SHORTCUT_DELETE,
		SHORTCUT_COPY,
		SHORTCUT_PASTE,
		// Translation & Rotation
		SHORTCUT_MOVE_X,
		SHORTCUT_MOVE_Y,
		SHORTCUT_MOVE_Z,
		SHORTCUT_MOVE_NORMAL,
		SHORTCUT_MOVE_TO_FLOOR,
		SHORTCUT_MOVE_TO_CEIL,
		SHORTCUT_SELECT_BACKFACES,
		// Textures.
		SHORTCUT_COPY_TEXTURE,
		SHORTCUT_SET_TEXTURE,
		SHORTCUT_SET_SIGN,
		SHORTCUT_AUTO_ALIGN,
		SHORTCUT_TEXOFFSET_UP,
		SHORTCUT_TEXOFFSET_DOWN,
		SHORTCUT_TEXOFFSET_LEFT,
		SHORTCUT_TEXOFFSET_RIGHT,
		SHORTCUT_TEXOFFSET_PAN,
		SHORTCUT_TEXOFFSET_RESET,
		// Lighting
		SHORTCUT_ADJUST_LIGHTING,
		// Drawing
		SHORTCUT_REDUCE_CURVE_SEGS,
		SHORTCUT_INCREASE_CURVE_SEGS,
		SHORTCUT_RESET_CURVE_SEGS,
		SHORTCUT_SET_GRID_HEIGHT,
		SHORTCUT_MOVE_GRID,
		SHORTCUT_RESET_GRID,
		SHORTCUT_ALIGN_GRID,
		// Camera
		SHORTCUT_TOGGLE_GRAVITY,
		SHORTCUT_CAMERA_FWD,
		SHORTCUT_CAMERA_BACK,
		SHORTCUT_CAMERA_LEFT,
		SHORTCUT_CAMERA_RIGHT,
		SHORTCUT_CAMERA_UP,
		SHORTCUT_CAMERA_DOWN,
		SHORTCUT_CAMERA_PAN2D,
		SHORTCUT_CAMERA_ROTATE3D,
		SHORTCUT_SET_TOP_DEPTH,
		SHORTCUT_SET_BOT_DEPTH,
		SHORTCUT_RESET_DEPTH,
		// Other
		SHORTCUT_SHOW_ALL_LABELS,
		SHORTCUT_UNDO,
		SHORTCUT_REDO,
		// Menus
		SHORTCUT_SAVE,
		SHORTCUT_RELOAD,
		SHORTCUT_FIND_SECTOR,
		SHORTCUT_JOIN_SECTORS,
		SHORTCUT_VIEW_2D,
		SHORTCUT_VIEW_3D,
		SHORTCUT_VIEW_WIREFRAME,
		SHORTCUT_VIEW_LIGHTING,
		SHORTCUT_VIEW_GROUP_COLOR,
		SHORTCUT_VIEW_TEXTURED_FLOOR,
		SHORTCUT_VIEW_TEXTURED_CEIL,
		SHORTCUT_VIEW_FULLBRIGHT,
		// Edit Modes
		SHORTCUT_MODE_DRAW,
		SHORTCUT_MODE_VERTEX,
		SHORTCUT_MODE_WALL,
		SHORTCUT_MODE_SECTOR,
		SHORTCUT_MODE_ENTITY,

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

	void parseLevelEditorShortcut(const char* key, const std::string* values, s32 valueCount);
	void writeLevelEditorShortcuts(FileStream& configFile);

	void clearKeyboardShortcuts();
	void setDefaultKeyboardShortcuts();
	void addKeyboardShortcut(ShortcutId id, KeyboardCode code = KEY_UNKNOWN, KeyModifier mod = KEYMOD_NONE, MouseButton mbtn = MBUTTON_UNKNOWN);
	void removeKeyboardShortcut(ShortcutId id);
	const char* getKeyboardShortcutDesc(ShortcutId id);
	KeyboardCode getShortcutKeyboardCode(ShortcutId id);
	MouseButton getShortcutMouseButton(ShortcutId id);
	KeyModifier getShortcutKeyMod(ShortcutId id);
	const char* getShortcutKeyComboText(ShortcutId id);
	bool isMouseInvertEnabled();
	bool isShortcutPressed(ShortcutId shortcutId, u32 allowedKeyMods = (1 << KEYMOD_SHIFT) | (1 << KEYMOD_ALT));
	bool isShortcutRepeat(ShortcutId shortcutId, u32 allowedKeyMods = (1 << KEYMOD_SHIFT) | (1 << KEYMOD_ALT));
	bool isShortcutHeld(ShortcutId shortcutId, u32 allowedKeyMods = (1 << KEYMOD_SHIFT) | (1 << KEYMOD_ALT));

	const u8* hotkeys_checkForKeyRepeats();
}
