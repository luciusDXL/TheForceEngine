#include "hotkeys.h"
#include "contextMenu.h"
#include "levelEditor.h"
#include "editGeometry.h"
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
#include <TFE_Input/input.h>
#include <TFE_System/system.h>
#include <map>

namespace LevelEditor
{
	static f64 s_lastClickTime = 0.0;
	static f64 s_lastRightClick = 0.0;

	const f64 c_doubleClickThreshold = 0.25f;
	const f64 c_rightClickThreshold = 0.5;
	const s32 c_rightClickMoveThreshold = 1;
	
	// General
	bool s_singleClick = false;
	bool s_doubleClick = false;
	bool s_rightClick = false;
	bool s_leftMouseDown = false;

	// Right click.
	bool s_rightPressed = false;
	Vec2i s_rightMousePos = { 0 };
	u32 s_drawActions = DRAW_ACTION_NONE;

	void handleMouseClick()
	{
		s_doubleClick = false;
		s_singleClick = false;
		s_rightClick = false;
		s_leftMouseDown = false;
		const f64 curTime = TFE_System::getTime();

		// Single/double click (left).
		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			if (curTime - s_lastClickTime < c_doubleClickThreshold)
			{
				s_doubleClick = true;
			}
			else
			{
				s_lastClickTime = curTime;
				s_singleClick = true;
			}
		}

		if (TFE_Input::mouseDown(MBUTTON_LEFT))
		{
			s_leftMouseDown = true;
		}

		// Right-click.
		if (s_contextMenu != CONTEXTMENU_NONE)
		{
			s_rightPressed = false;
			s_rightClick = false;
			s_lastRightClick = 0.0;
			return;
		}

		const bool rightPressed = TFE_Input::mousePressed(MBUTTON_RIGHT);
		const bool rightDown = TFE_Input::mouseDown(MBUTTON_RIGHT);

		if (rightPressed)
		{
			s_lastRightClick = curTime;
			s_rightPressed = true;
			TFE_Input::getMousePos(&s_rightMousePos.x, &s_rightMousePos.z);
		}
		else if (!rightDown && curTime - s_lastRightClick < c_rightClickThreshold && s_rightPressed)
		{
			Vec2i curMousePos;
			TFE_Input::getMousePos(&curMousePos.x, &curMousePos.z);

			const Vec2i delta = { curMousePos.x - s_rightMousePos.x, curMousePos.z - s_rightMousePos.z };
			if (::abs(delta.x) < c_rightClickMoveThreshold && ::abs(delta.z) < c_rightClickMoveThreshold)
			{
				s_rightClick = true;
			}
		}
		if (!rightDown)
		{
			s_rightPressed = false;
			s_lastRightClick = 0.0;
		}
	}

	void updateDrawModeHotkeys()
	{
		if (TFE_Input::keyModDown(KeyModifier::KEYMOD_SHIFT))
		{
			s_drawActions = DRAW_ACTION_CURVE;
		}
		if (TFE_Input::keyPressed(KEY_BACKSPACE))
		{
			s_drawActions = DRAW_ACTION_CANCEL;
		}
		else if (TFE_Input::keyPressed(KEY_RETURN))
		{
			s_drawActions = DRAW_ACTION_FINISH;
		}

		if (s_editMode != LEDIT_GUIDELINES && s_geoEdit.drawMode == DMODE_CURVE_CONTROL)
		{
			s32 delta = getCurveSegDelta();
			if (isShortcutRepeat(SHORTCUT_REDUCE_CURVE_SEGS))
			{
				setCurveSegDelta(delta - 1);
			}
			else if (isShortcutRepeat(SHORTCUT_INCREASE_CURVE_SEGS))
			{
				setCurveSegDelta(delta + 1);
			}
			else if (isShortcutPressed(SHORTCUT_RESET_CURVE_SEGS))
			{
				setCurveSegDelta();
			}
		}
	}

	void handleHotkeys()
	{
		s_drawActions = DRAW_ACTION_NONE;
		handleMouseClick();
		if (isUiModal()) { return; }

		if (s_editMode == LEDIT_DRAW || s_editMode == LEDIT_GUIDELINES)
		{
			updateDrawModeHotkeys();
		}
	}

	struct KeyboardShortcut
	{
		KeyboardCode code = KEY_UNKNOWN;
		KeyModifier mod = KEYMOD_NONE;
		ShortcutId shortcutId = SHORTCUT_NONE;
	};
	const char* c_shortcutDesc[] =
	{
		"Insert Vertex/Object",								// SHORTCUT_PLACE
		"Lock movement to X-Axis",							// SHORTCUT_MOVE_X
		"Lock movement to Y-Axis",							// SHORTCUT_MOVE_Y
		"Lock movement to Z-Axis",							// SHORTCUT_MOVE_Z
		"Lock movement to wall normal",						// SHORTCUT_MOVE_NORMAL
		"Copy texture from surface",						// SHORTCUT_COPY_TEXTURE
		"Set current texture on surface or selection",		// SHORTCUT_SET_TEXTURE
		"Apply the current texture as a sign at the current mouse position on the surface", // SHORTCUT_SET_SIGN
		"Auto-align texture offsets on matching adjacent surfaces", // SHORTCUT_AUTO_ALIGN
		"Move object to sector floor",						// SHORTCUT_MOVE_TO_FLOOR
		"Move object to sector ceiling",					// SHORTCUT_MOVE_TO_CEIL
		"Delete selection",									// SHORTCUT_DELETE
		"Copy selection",									// SHORTCUT_COPY
		"Paste from clipboard at cursor",					// SHORTCUT_PASTE
		"Rotate with mousewheel while held",				// SHORTCUT_ROTATE
		"Reduce curve segments",							// SHORTCUT_REDUCE_CURVE_SEGS
		"Increase curve segments",							// SHORTCUT_INCREASE_CURVE_SEGS
		"Reset curve segments to default",					// SHORTCUT_RESET_CURVE_SEGS
		"Toggle Show all labels in viewport",				// SHORTCUT_SHOW_ALL_LABELS
		"Undo",												// SHORTCUT_UNDO
		"Redo",												// SHORTCUT_REDO
	};

	static std::vector<KeyboardShortcut> s_keyboardShortcutList;
	static std::vector<u32> s_keyboardShortcutFreeList;
	static std::map<u32, u32> s_keyboardShortcutMap;
	
	KeyboardShortcut* getShortcutFromId(ShortcutId id);
	bool validateKeyMods(KeyboardShortcut* shortcut, u32 allowedKeyMods);

	void addKeyboardShortcut(ShortcutId id, KeyboardCode code, KeyModifier mod)
	{
		KeyboardShortcut* shortcut = getShortcutFromId(id);
		// Replace it.
		if (shortcut)
		{
			shortcut->code = code;
			shortcut->mod = mod;
		}
		// Add it.
		else if (!s_keyboardShortcutFreeList.empty())
		{
			const u32 index = (u32)s_keyboardShortcutFreeList.back();
			s_keyboardShortcutFreeList.pop_back();

			s_keyboardShortcutList[index].code = code;
			s_keyboardShortcutList[index].mod = mod;
			s_keyboardShortcutList[index].shortcutId = id;
			s_keyboardShortcutMap[id] = index;
		}
		else
		{
			const u32 index = (u32)s_keyboardShortcutList.size();
			s_keyboardShortcutList.push_back({ code, mod, id });
			s_keyboardShortcutMap[id] = index;
		}
	}

	void removeKeyboardShortcut(ShortcutId id)
	{
		std::map<u32, u32>::iterator iShortcut = s_keyboardShortcutMap.find(id);
		if (iShortcut != s_keyboardShortcutMap.end())
		{
			const u32 index = iShortcut->second;
			s_keyboardShortcutFreeList.push_back(index);
			s_keyboardShortcutList[index].shortcutId = SHORTCUT_NONE; // Invalid
			s_keyboardShortcutMap.erase(iShortcut);
		}
	}

	const char* getKeyboardShortcutDesc(ShortcutId id)
	{
		if (id < SHORTCUT_PLACE || id >= SHORTCUT_COUNT)
		{
			return nullptr;
		}
		return c_shortcutDesc[id];
	}

	void clearKeyboardShortcuts()
	{
		s_keyboardShortcutList.clear();
		s_keyboardShortcutMap.clear();
		s_keyboardShortcutFreeList.clear();
	}

	void setDefaultKeyboardShortcuts()
	{
		clearKeyboardShortcuts();
		addKeyboardShortcut(SHORTCUT_PLACE, KEY_INSERT);
		addKeyboardShortcut(SHORTCUT_MOVE_X, KEY_X);
		addKeyboardShortcut(SHORTCUT_MOVE_Y, KEY_Y);
		addKeyboardShortcut(SHORTCUT_MOVE_Z, KEY_Z);
		addKeyboardShortcut(SHORTCUT_MOVE_NORMAL, KEY_N);
		addKeyboardShortcut(SHORTCUT_COPY_TEXTURE, KEY_T, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_SET_TEXTURE, KEY_T);
		addKeyboardShortcut(SHORTCUT_SET_SIGN, KEY_T, KEYMOD_SHIFT);
		addKeyboardShortcut(SHORTCUT_AUTO_ALIGN, KEY_A, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_MOVE_TO_FLOOR, KEY_F);
		addKeyboardShortcut(SHORTCUT_MOVE_TO_CEIL, KEY_C);
		addKeyboardShortcut(SHORTCUT_DELETE, KEY_DELETE);
		addKeyboardShortcut(SHORTCUT_COPY, KEY_C, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_PASTE, KEY_V, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_ROTATE, KEY_UNKNOWN, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_REDUCE_CURVE_SEGS, KEY_LEFTBRACKET);
		addKeyboardShortcut(SHORTCUT_INCREASE_CURVE_SEGS, KEY_RIGHTBRACKET);
		addKeyboardShortcut(SHORTCUT_RESET_CURVE_SEGS, KEY_BACKSLASH);
		addKeyboardShortcut(SHORTCUT_SHOW_ALL_LABELS, KEY_TAB);
		addKeyboardShortcut(SHORTCUT_UNDO, KEY_Z, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_REDO, KEY_Y, KEYMOD_CTRL);
	}
				
	bool isShortcutPressed(ShortcutId shortcutId, u32 allowedKeyMods)
	{
		KeyboardShortcut* shortcut = getShortcutFromId(shortcutId);
		if (!shortcut || shortcut->code == KEY_UNKNOWN) { return false; }

		bool pressed = TFE_Input::keyPressed(shortcut->code);
		pressed &= validateKeyMods(shortcut, allowedKeyMods);
		return pressed;
	}

	bool isShortcutRepeat(ShortcutId shortcutId, u32 allowedKeyMods)
	{
		KeyboardShortcut* shortcut = getShortcutFromId(shortcutId);
		if (!shortcut || shortcut->code == KEY_UNKNOWN) { return false; }

		bool pressed = TFE_Input::keyPressedWithRepeat(shortcut->code);
		pressed &= validateKeyMods(shortcut, allowedKeyMods);
		return pressed;
	}

	bool isShortcutHeld(ShortcutId shortcutId, u32 allowedKeyMods)
	{
		KeyboardShortcut* shortcut = getShortcutFromId(shortcutId);
		if (!shortcut) { return false; }

		bool down = true;
		if (shortcut->code != KEY_UNKNOWN)
		{
			down &= TFE_Input::keyDown(shortcut->code);
		}
		down &= validateKeyMods(shortcut, allowedKeyMods);
		return down;
	}

	KeyboardShortcut* getShortcutFromId(ShortcutId id)
	{
		std::map<u32, u32>::iterator iShortcut = s_keyboardShortcutMap.find(id);
		if (iShortcut == s_keyboardShortcutMap.end())
		{
			return nullptr;
		}
		return &s_keyboardShortcutList[iShortcut->second];
	}

	bool validateKeyMods(KeyboardShortcut* shortcut, u32 allowedKeyMods)
	{
		bool requiredModsDown = true;
		u32 reqMods = 0;
		if (shortcut->mod != KEYMOD_NONE)
		{
			reqMods = 1 << shortcut->mod;
		}
		for (u32 m = KEYMOD_NONE + 1; m <= KEYMOD_SHIFT; m++)
		{
			bool modDown = TFE_Input::keyModDown(KeyModifier(m));
			if (reqMods & (1 << m))
			{
				requiredModsDown &= modDown;
			}
			else if (modDown && !(allowedKeyMods & (1 << m)))
			{
				requiredModsDown = false;
			}
		}
		return requiredModsDown;
	}
}
