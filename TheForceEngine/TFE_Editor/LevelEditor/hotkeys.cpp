#include "hotkeys.h"
#include "contextMenu.h"
#include "levelEditor.h"
#include "editGeometry.h"
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Input/input.h>
#include <TFE_System/system.h>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
	static f64 s_lastClickTime = 0.0;
	static f64 s_lastRightClick = 0.0;
	static char s_keyComboText[256];

	static std::map<std::string, ShortcutId> s_shortcutNameMap;
	static std::map<std::string, KeyboardCode> s_keyNameMap;
	static std::map<std::string, MouseButton> s_mouseButtonNameMap;
	static std::map<std::string, KeyModifier> s_keyModMap;
	static u8 s_keyRepeats[SHORTCUT_COUNT] = { 0 };

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
		MouseButton mbtn = MBUTTON_UNKNOWN;
		KeyModifier mod = KEYMOD_NONE;
		ShortcutId shortcutId = SHORTCUT_NONE;
		bool allowDuplicates = false;
	};
	const char* c_shortcutName[] =
	{
		// Insert/Delete/Copy/Paste
		"LEV_SHORTCUT_PLACE",
		"LEV_SHORTCUT_DELETE",
		"LEV_SHORTCUT_COPY",
		"LEV_SHORTCUT_PASTE",
		// Translation & Rotation
		"LEV_SHORTCUT_MOVE_X",
		"LEV_SHORTCUT_MOVE_Y",
		"LEV_SHORTCUT_MOVE_Z",
		"LEV_SHORTCUT_MOVE_NORMAL",
		"LEV_SHORTCUT_MOVE_TO_FLOOR",
		"LEV_SHORTCUT_MOVE_TO_CEIL",
		"LEV_SHORTCUT_SELECT_BACKFACES",
		// Textures.
		"LEV_SHORTCUT_COPY_TEXTURE",
		"LEV_SHORTCUT_SET_TEXTURE",
		"LEV_SHORTCUT_SET_SIGN",
		"LEV_SHORTCUT_AUTO_ALIGN",
		"LEV_SHORTCUT_TEXOFFSET_UP",
		"LEV_SHORTCUT_TEXOFFSET_DOWN",
		"LEV_SHORTCUT_TEXOFFSET_LEFT",
		"LEV_SHORTCUT_TEXOFFSET_RIGHT",
		"LEV_SHORTCUT_TEXOFFSET_PAN",
		"LEV_SHORTCUT_TEXOFFSET_RESET",
		// Lighting
		"LEV_SHORTCUT_ADJUST_LIGHTING",
		// Drawing
		"LEV_SHORTCUT_REDUCE_CURVE_SEGS",
		"LEV_SHORTCUT_INCREASE_CURVE_SEGS",
		"LEV_SHORTCUT_RESET_CURVE_SEGS",
		"LEV_SHORTCUT_SET_GRID_HEIGHT",
		"LEV_SHORTCUT_MOVE_GRID",
		"LEV_SHORTCUT_RESET_GRID",
		"LEV_SHORTCUT_ALIGN_GRID",
		// Camera
		"LEV_SHORTCUT_TOGGLE_GRAVITY",
		"LEV_SHORTCUT_CAMERA_FWD",
		"LEV_SHORTCUT_CAMERA_BACK",
		"LEV_SHORTCUT_CAMERA_LEFT",
		"LEV_SHORTCUT_CAMERA_RIGHT",
		"LEV_SHORTCUT_CAMERA_UP",
		"LEV_SHORTCUT_CAMERA_DOWN",
		"LEV_SHORTCUT_CAMERA_PAN2D",
		"LEV_SHORTCUT_CAMERA_ROTATE3D",
		"LEV_SHORTCUT_SET_TOP_DEPTH",
		"LEV_SHORTCUT_SET_BOT_DEPTH",
		"LEV_SHORTCUT_RESET_DEPTH",
		// Other
		"LEV_SHORTCUT_SHOW_ALL_LABELS",
		"LEV_SHORTCUT_UNDO",
		"LEV_SHORTCUT_REDO",
		// Menus
		"LEV_SHORTCUT_SAVE",
		"LEV_SHORTCUT_RELOAD",
		"LEV_SHORTCUT_FIND_SECTOR",
		"LEV_SHORTCUT_JOIN_SECTORS",
		"LEV_SHORTCUT_VIEW_2D",
		"LEV_SHORTCUT_VIEW_3D",
		"LEV_SHORTCUT_VIEW_WIREFRAME",
		"LEV_SHORTCUT_VIEW_LIGHTING",
		"LEV_SHORTCUT_VIEW_GROUP_COLOR",
		"LEV_SHORTCUT_VIEW_TEXTURED_FLOOR",
		"LEV_SHORTCUT_VIEW_TEXTURED_CEIL",
		"LEV_SHORTCUT_VIEW_FULLBRIGHT",
		// Edit Modes
		"LEV_SHORTCUT_MODE_DRAW",
		"LEV_SHORTCUT_MODE_VERTEX",
		"LEV_SHORTCUT_MODE_WALL",
		"LEV_SHORTCUT_MODE_SECTOR",
		"LEV_SHORTCUT_MODE_ENTITY",
	};
	const char* c_shortcutDesc[] =
	{
		"Insert Vertex/Object",								// SHORTCUT_PLACE
		"Delete selection",									// SHORTCUT_DELETE
		"Copy selection",									// SHORTCUT_COPY
		"Paste from clipboard at cursor",					// SHORTCUT_PASTE
		"Lock movement to X-Axis (sector, wall, vertex, object)",// SHORTCUT_MOVE_X
		"Lock movement to Y-Axis (sector, wall, vertex, object)",// SHORTCUT_MOVE_Y
		"Lock movement to Z-Axis (sector, wall, vertex, object)",// SHORTCUT_MOVE_Z
		"Lock movement to wall normal",						// SHORTCUT_MOVE_NORMAL
		"Move object to sector floor",						// SHORTCUT_MOVE_TO_FLOOR
		"Move object to sector ceiling",					// SHORTCUT_MOVE_TO_CEIL
		"Select backfacing surfaces",						// SHORTCUT_SELECT_BACKFACES
		"Copy texture from surface",						// SHORTCUT_COPY_TEXTURE
		"Set current texture on surface or selection",		// SHORTCUT_SET_TEXTURE
		"Apply the current texture as a sign at mouse position", // SHORTCUT_SET_SIGN
		"Auto-align texture offsets on matching adjacent surfaces", // SHORTCUT_AUTO_ALIGN
		"Move texture offset up",							// SHORTCUT_TEXOFFSET_UP
		"Move texture offset down",							// SHORTCUT_TEXOFFSET_DOWN
		"Move texture offset left",							// SHORTCUT_TEXOFFSET_LEFT
		"Move texture offset right",						// SHORTCUT_TEXOFFSET_RIGHT
		"Hold to pan the texture with the mouse",			// SHORTCUT_TEXOFFSET_PAN
		"Reset the texture offset",							// SHORTCUT_TEXOFFSET_RESET
		"Adjust lighting of surface or sector with mousewheel", // SHORTCUT_ADJUST_LIGHTING
		"Reduce curve segments",							// SHORTCUT_REDUCE_CURVE_SEGS
		"Increase curve segments",							// SHORTCUT_INCREASE_CURVE_SEGS
		"Reset curve segments to default",					// SHORTCUT_RESET_CURVE_SEGS
		"Set the grid to the selected floor/ceiling/sector height",// SHORTCUT_SET_GRID_HEIGHT
		"Hold to move the grid up and down (3d view/sector draw)", // SHORTCUT_MOVE_GRID
		"Reset Grid to default.",							// SHORTCUT_RESET_GRID,
		"Align Grid to Edge.",								// SHORTCUT_ALIGN_GRID,
		"Toggle gravity (3d view only)",					// SHORTCUT_TOGGLE_GRAVITY
		"Camera Forward",									// SHORTCUT_CAMERA_FWD
		"Camera Backward",									// SHORTCUT_CAMERA_BACK
		"Camera Strafe Left",								// SHORTCUT_CAMERA_LEFT
		"Camera Strafe Right",								// SHORTCUT_CAMERA_RIGHT
		"Camera Move Up",									// SHORTCUT_CAMERA_UP,
		"Camera Move Down",									// SHORTCUT_CAMERA_DOWN,
		"Camera Mouse Pan 2D",								// SHORTCUT_CAMERA_PAN2D,
		"Camera Mouse Rotate 3D",							// SHORTCUT_CAMERA_ROTATE3D,
		"View Depth - set top height (2d view only)",		// SHORTCUT_SET_TOP_DEPTH
		"View Depth - set bottom height (2d view only)",	// SHORTCUT_SET_BOT_DEPTH
		"View Depth - reset (2d view only)",				// SHORTCUT_RESET_DEPTH
		"Toggle Show all labels in viewport",				// SHORTCUT_SHOW_ALL_LABELS
		"Undo",												// SHORTCUT_UNDO
		"Redo",												// SHORTCUT_REDO
		"Save Level",										// SHORTCUT_SAVE
		"Reload Level",										// SHORTCUT_RELOAD
		"Find Sector",										// SHORTCUT_FIND_SECTOR
		"Join Sectors",										// SHORTCUT_JOIN_SECTORS
		"2D View",											// SHORTCUT_VIEW_2D
		"3D View",											// SHORTCUT_VIEW_3D
		"Display Wireframe",								// SHORTCUT_VIEW_WIREFRAME
		"Display Lighting",									// SHORTCUT_VIEW_LIGHTING
		"Display Group Color",								// SHORTCUT_VIEW_GROUP_COLOR
		"Display Textured Floors",							// SHORTCUT_VIEW_TEXTURED_FLOOR
		"Display Texture Ceilings",							// SHORTCUT_VIEW_TEXTURED_CEIL
		"Show Fullbright",									// SHORTCUT_VIEW_FULLBRIGHT
		"Edit Mode - Draw",									// SHORTCUT_MODE_DRAW
		"Edit Mode - Vertex",								// SHORTCUT_MODE_VERTEX
		"Edit Mode - Wall/Surface",							// SHORTCUT_MODE_WALL
		"Edit Mode - Sector",								// SHORTCUT_MODE_SECTOR
		"Edit Mode - Entity",								// SHORTCUT_MODE_ENTITY
	};

	static std::vector<KeyboardShortcut> s_keyboardShortcutList;
	static std::vector<u32> s_keyboardShortcutFreeList;
	static std::map<u32, u32> s_keyboardShortcutMap;
	
	KeyboardShortcut* getShortcutFromId(ShortcutId id);
	bool validateKeyMods(KeyboardShortcut* shortcut, u32 allowedKeyMods);

	ShortcutId stringToShortcutId(const std::string name);
	KeyboardCode stringToKeyCode(const std::string name);
	MouseButton stringToMouseButton(const std::string name);
	KeyModifier stringToKeyModifier(const std::string name);
	const char* shortcutIdToString(ShortcutId id);
	const char* mouseButtonToString(MouseButton id);
	const char* keycodeToString(KeyboardCode id);
	const char* keyModifierToString(KeyModifier id);
		
	void addKeyboardShortcut(ShortcutId id, KeyboardCode code, KeyModifier mod, MouseButton mbtn)
	{
		KeyboardShortcut* shortcut = getShortcutFromId(id);
		bool allowDup = (id == SHORTCUT_CAMERA_PAN2D || id == SHORTCUT_CAMERA_ROTATE3D);
		// Replace it.
		if (shortcut)
		{
			shortcut->code = code;
			shortcut->mbtn = mbtn;
			shortcut->mod = mod;
			shortcut->allowDuplicates = allowDup;
		}
		// Add it.
		else if (!s_keyboardShortcutFreeList.empty())
		{
			const u32 index = (u32)s_keyboardShortcutFreeList.back();
			s_keyboardShortcutFreeList.pop_back();

			s_keyboardShortcutList[index].code = code;
			s_keyboardShortcutList[index].mod = mod;
			s_keyboardShortcutList[index].mbtn = mbtn;
			s_keyboardShortcutList[index].allowDuplicates = allowDup;
			s_keyboardShortcutList[index].shortcutId = id;
			s_keyboardShortcutMap[id] = index;
		}
		else
		{
			const u32 index = (u32)s_keyboardShortcutList.size();
			s_keyboardShortcutList.push_back({ code, mbtn, mod, id, allowDup });
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

	KeyboardCode getShortcutKeyboardCode(ShortcutId id)
	{
		KeyboardShortcut* shortcut = getShortcutFromId(id);
		if (!shortcut)
		{
			return KEY_UNKNOWN;
		}
		return shortcut->code;
	}

	KeyModifier getShortcutKeyMod(ShortcutId id)
	{
		KeyboardShortcut* shortcut = getShortcutFromId(id);
		if (!shortcut)
		{
			return KEYMOD_NONE;
		}
		return shortcut->mod;
	}

	MouseButton getShortcutMouseButton(ShortcutId id)
	{
		KeyboardShortcut* shortcut = getShortcutFromId(id);
		if (!shortcut)
		{
			return MBUTTON_UNKNOWN;
		}
		return shortcut->mbtn;
	}
		
	void clearKeyboardShortcuts()
	{
		s_keyboardShortcutList.clear();
		s_keyboardShortcutMap.clear();
		s_keyboardShortcutFreeList.clear();
	}

	void parseLevelEditorShortcut(const char* idName, const std::string* values, s32 valueCount)
	{
		const ShortcutId id = stringToShortcutId(idName);
		if (id < 0 || id >= ShortcutId::SHORTCUT_COUNT) { return; }

		KeyboardCode keyCode = KEY_UNKNOWN;
		MouseButton mbtn = MBUTTON_UNKNOWN;
		KeyModifier keyMod = KEYMOD_NONE;
		if (valueCount >= 1)
		{
			keyCode = stringToKeyCode(values[0]);
		}
		if (valueCount >= 2)
		{
			keyMod = stringToKeyModifier(values[1]);
		}
		if (valueCount >= 3)
		{
			mbtn = stringToMouseButton(values[2]);
		}

		if (keyCode != KEY_UNKNOWN || mbtn != MBUTTON_UNKNOWN)
		{
			addKeyboardShortcut(id, keyCode, keyMod, mbtn);
		}
	}

	void writeLevelEditorShortcuts(FileStream& configFile)
	{
		char lineBuffer[TFE_MAX_PATH];
		for (s32 i = 0; i < SHORTCUT_COUNT; i++)
		{
			ShortcutId id = ShortcutId(i);
			KeyboardShortcut* shortcut = getShortcutFromId(id);
			const char* mbtnString = mouseButtonToString(shortcut->mbtn);
			if (mbtnString && mbtnString[0] != 0)
			{
				snprintf(lineBuffer, TFE_MAX_PATH, "%s=%s %s %s\r\n", shortcutIdToString(id), keycodeToString(shortcut->code), keyModifierToString(shortcut->mod), mbtnString);
			}
			else
			{
				snprintf(lineBuffer, TFE_MAX_PATH, "%s=%s %s\r\n", shortcutIdToString(id), keycodeToString(shortcut->code), keyModifierToString(shortcut->mod));
			}
			configFile.writeBuffer(lineBuffer, (u32)strlen(lineBuffer));
		}
	}

	void setDefaultKeyboardShortcuts()
	{
		clearKeyboardShortcuts();
		addKeyboardShortcut(SHORTCUT_PLACE, KEY_INSERT);
		addKeyboardShortcut(SHORTCUT_DELETE, KEY_DELETE);
		addKeyboardShortcut(SHORTCUT_COPY, KEY_C, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_PASTE, KEY_V, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_MOVE_X, KEY_X);
		addKeyboardShortcut(SHORTCUT_MOVE_Y, KEY_Y);
		addKeyboardShortcut(SHORTCUT_MOVE_Z, KEY_Z);
		addKeyboardShortcut(SHORTCUT_MOVE_NORMAL, KEY_N);
		addKeyboardShortcut(SHORTCUT_MOVE_TO_FLOOR, KEY_F);
		addKeyboardShortcut(SHORTCUT_MOVE_TO_CEIL, KEY_C);
		addKeyboardShortcut(SHORTCUT_SELECT_BACKFACES, KEY_B);
		addKeyboardShortcut(SHORTCUT_COPY_TEXTURE, KEY_T, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_SET_TEXTURE, KEY_T);
		addKeyboardShortcut(SHORTCUT_SET_SIGN, KEY_T, KEYMOD_SHIFT);
		addKeyboardShortcut(SHORTCUT_AUTO_ALIGN, KEY_A, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_TEXOFFSET_UP, KEY_UP);
		addKeyboardShortcut(SHORTCUT_TEXOFFSET_DOWN, KEY_DOWN);
		addKeyboardShortcut(SHORTCUT_TEXOFFSET_LEFT, KEY_LEFT);
		addKeyboardShortcut(SHORTCUT_TEXOFFSET_RIGHT, KEY_RIGHT);
		addKeyboardShortcut(SHORTCUT_TEXOFFSET_PAN, KEY_UNKNOWN, KEYMOD_NONE, MBUTTON_MIDDLE);
		addKeyboardShortcut(SHORTCUT_TEXOFFSET_RESET, KEY_T, KEYMOD_ALT);
		addKeyboardShortcut(SHORTCUT_ADJUST_LIGHTING, KEY_L);
		addKeyboardShortcut(SHORTCUT_REDUCE_CURVE_SEGS, KEY_LEFTBRACKET);
		addKeyboardShortcut(SHORTCUT_INCREASE_CURVE_SEGS, KEY_RIGHTBRACKET);
		addKeyboardShortcut(SHORTCUT_RESET_CURVE_SEGS, KEY_BACKSLASH);
		addKeyboardShortcut(SHORTCUT_SET_GRID_HEIGHT, KEY_G, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_MOVE_GRID, KEY_H);
		addKeyboardShortcut(SHORTCUT_RESET_GRID, KEY_R);
		addKeyboardShortcut(SHORTCUT_ALIGN_GRID, KEY_R, KEYMOD_ALT);
		addKeyboardShortcut(SHORTCUT_TOGGLE_GRAVITY, KEY_G);
		addKeyboardShortcut(SHORTCUT_CAMERA_FWD, KEY_W);
		addKeyboardShortcut(SHORTCUT_CAMERA_BACK, KEY_S);
		addKeyboardShortcut(SHORTCUT_CAMERA_LEFT, KEY_A);
		addKeyboardShortcut(SHORTCUT_CAMERA_RIGHT, KEY_D);
		addKeyboardShortcut(SHORTCUT_CAMERA_UP, KEY_SPACE);
		addKeyboardShortcut(SHORTCUT_CAMERA_DOWN, KEY_SPACE, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_CAMERA_PAN2D, KEY_UNKNOWN, KEYMOD_NONE, MBUTTON_RIGHT);
		addKeyboardShortcut(SHORTCUT_CAMERA_ROTATE3D, KEY_UNKNOWN, KEYMOD_NONE, MBUTTON_RIGHT);
		addKeyboardShortcut(SHORTCUT_SET_TOP_DEPTH, KEY_Q, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_SET_BOT_DEPTH, KEY_E, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_RESET_DEPTH, KEY_Q);
		addKeyboardShortcut(SHORTCUT_SHOW_ALL_LABELS, KEY_TAB);
		addKeyboardShortcut(SHORTCUT_UNDO, KEY_Z, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_REDO, KEY_Y, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_SAVE, KEY_S, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_RELOAD, KEY_R, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_FIND_SECTOR, KEY_F, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_JOIN_SECTORS, KEY_J, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_VIEW_2D, KEY_1, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_VIEW_3D, KEY_2, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_VIEW_WIREFRAME, KEY_4, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_VIEW_LIGHTING, KEY_5, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_VIEW_GROUP_COLOR, KEY_6, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_VIEW_TEXTURED_FLOOR, KEY_7, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_VIEW_TEXTURED_CEIL, KEY_8, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_VIEW_FULLBRIGHT, KEY_9, KEYMOD_CTRL);
		addKeyboardShortcut(SHORTCUT_MODE_DRAW, KEY_D, KEYMOD_ALT);
		addKeyboardShortcut(SHORTCUT_MODE_VERTEX, KEY_V, KEYMOD_ALT);
		addKeyboardShortcut(SHORTCUT_MODE_WALL, KEY_W, KEYMOD_ALT);
		addKeyboardShortcut(SHORTCUT_MODE_SECTOR, KEY_S, KEYMOD_ALT);
		addKeyboardShortcut(SHORTCUT_MODE_ENTITY, KEY_E, KEYMOD_ALT);
	}

	bool isMouseInvertEnabled()
	{
		return (s_editorConfig.levelEditorFlags&LEVEDITOR_FLAG_INVERT_Y) != 0;
	}
				
	bool isShortcutPressed(ShortcutId shortcutId, u32 allowedKeyMods)
	{
		KeyboardShortcut* shortcut = getShortcutFromId(shortcutId);
		if (!shortcut || (shortcut->code == KEY_UNKNOWN && shortcut->mbtn == MBUTTON_UNKNOWN)) { return false; }

		bool pressed = false;
		if (shortcut->code != KEY_UNKNOWN)
		{
			pressed = TFE_Input::keyPressed(shortcut->code);
		}
		else if (shortcut->mbtn != MBUTTON_UNKNOWN)
		{
			pressed = TFE_Input::mousePressed(shortcut->mbtn);
		}
		pressed &= validateKeyMods(shortcut, allowedKeyMods);
		return pressed;
	}

	bool isShortcutRepeat(ShortcutId shortcutId, u32 allowedKeyMods)
	{
		KeyboardShortcut* shortcut = getShortcutFromId(shortcutId);
		if (!shortcut || (shortcut->code == KEY_UNKNOWN && shortcut->mbtn == MBUTTON_UNKNOWN)) { return false; }

		bool pressed = false;
		if (shortcut->code != KEY_UNKNOWN)
		{
			pressed = TFE_Input::keyPressedWithRepeat(shortcut->code);
		}
		else if (shortcut->mbtn != MBUTTON_UNKNOWN)
		{
			pressed = TFE_Input::mousePressed(shortcut->mbtn);
		}
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
		else if (shortcut->mbtn != MBUTTON_UNKNOWN)
		{
			down &= TFE_Input::mouseDown(shortcut->mbtn);
		}
		down &= validateKeyMods(shortcut, allowedKeyMods);
		return down;
	}

	const char* getShortcutKeyComboText(ShortcutId id)
	{
		KeyboardCode code = getShortcutKeyboardCode(id);
		KeyModifier mod = getShortcutKeyMod(id);
		MouseButton mbtn = getShortcutMouseButton(id);

		s_keyComboText[0] = 0;
		if (code != KEY_UNKNOWN && mod != KEYMOD_NONE)
		{
			sprintf(s_keyComboText, "%s+%s", TFE_Input::getKeyboardModifierName(mod), TFE_Input::getKeyboardName(code));
		}
		else if (code != KEY_UNKNOWN)
		{
			sprintf(s_keyComboText, "%s", TFE_Input::getKeyboardName(code));
		}
		else if (mbtn != MBUTTON_UNKNOWN && mod != KEYMOD_NONE)
		{
			sprintf(s_keyComboText, "%s+%s", TFE_Input::getKeyboardModifierName(mod), TFE_Input::getMouseButtonName(mbtn));
		}
		else if (mbtn != MBUTTON_UNKNOWN)
		{
			sprintf(s_keyComboText, "%s", TFE_Input::getMouseButtonName(mbtn));
		}
		else if (mod != KEYMOD_NONE)
		{
			sprintf(s_keyComboText, "%s", TFE_Input::getKeyboardModifierName(mod));
		}
		return s_keyComboText;
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

	///////////////////////////////////////////////////////////////
	ShortcutId stringToShortcutId(const std::string name)
	{
		// Fill in the map to make lookups faster.
		if (s_shortcutNameMap.empty())
		{
			for (s32 i = 0; i < SHORTCUT_COUNT; i++)
			{
				ShortcutId id = ShortcutId(i);
				s_shortcutNameMap[c_shortcutName[i]] = id;
			}
		}
		// Find the id.
		std::map<std::string, ShortcutId>::iterator iShortcut = s_shortcutNameMap.find(name);
		if (iShortcut != s_shortcutNameMap.end())
		{
			return iShortcut->second;
		}
		return SHORTCUT_NONE;
	}

	MouseButton stringToMouseButton(const std::string name)
	{
		// Fill in the map to make lookups faster.
		if (s_mouseButtonNameMap.empty())
		{
			for (s32 i = 0; i < MBUTTON_COUNT; i++)
			{
				MouseButton mbtn = MouseButton(i);
				s_mouseButtonNameMap[TFE_Input::getMouseButtonName(mbtn)] = mbtn;
			}
		}
		// Find the id.
		std::map<std::string, MouseButton>::iterator iBtn = s_mouseButtonNameMap.find(name);
		if (iBtn != s_mouseButtonNameMap.end())
		{
			return iBtn->second;
		}
		return MBUTTON_UNKNOWN;
	}

	KeyboardCode stringToKeyCode(const std::string name)
	{
		// Fill in the map to make lookups faster.
		if (s_keyNameMap.empty())
		{
			for (s32 i = 0; i <= KEY_LAST; i++)
			{
				KeyboardCode key = KeyboardCode(i);
				s_keyNameMap[TFE_Input::getKeyboardName(key)] = key;
			}
		}
		// Find the id.
		std::map<std::string, KeyboardCode>::iterator iKeyCode = s_keyNameMap.find(name);
		if (iKeyCode != s_keyNameMap.end())
		{
			return iKeyCode->second;
		}
		return KEY_UNKNOWN;
	}

	KeyModifier stringToKeyModifier(const std::string name)
	{
		// Fill in the map to make lookups faster.
		if (s_keyModMap.empty())
		{
			for (s32 i = 0; i <= KEYMOD_SHIFT; i++)
			{
				KeyModifier mod = KeyModifier(i);
				s_keyModMap[TFE_Input::getKeyboardModifierName(mod)] = mod;
			}
		}
		// Find the id.
		std::map<std::string, KeyModifier>::iterator iKeyMod = s_keyModMap.find(name);
		if (iKeyMod != s_keyModMap.end())
		{
			return iKeyMod->second;
		}
		return KEYMOD_NONE;
	}

	const char* shortcutIdToString(ShortcutId id)
	{
		if (id < 0 || id >= SHORTCUT_NONE) { return nullptr; }
		return c_shortcutName[id];
	}

	const char* mouseButtonToString(MouseButton id)
	{
		if (id < 0 || id >= MBUTTON_COUNT) { return nullptr; }
		return TFE_Input::getMouseButtonName(id);
	}

	const char* keycodeToString(KeyboardCode id)
	{
		if (id < 0 || id > KEY_LAST) { return nullptr; }
		return TFE_Input::getKeyboardName(id);
	}

	const char* keyModifierToString(KeyModifier id)
	{
		if (id < 0 || id > KEYMOD_SHIFT) { return nullptr; }
		return TFE_Input::getKeyboardModifierName(id);
	}

	const u8* hotkeys_checkForKeyRepeats()
	{
		std::map<u32, u32> keyComboIndex;
		for (s32 i = 0; i < SHORTCUT_COUNT; i++)
		{
			KeyboardShortcut* shortcut = &s_keyboardShortcutList[i];

			const u32 key = u32(shortcut->code) | u32(shortcut->mbtn << 18u) | u32(shortcut->mod << 24u);
			s_keyRepeats[i] = 0;

			std::map<u32, u32>::iterator iShortcut = keyComboIndex.find(key);
			if (iShortcut != keyComboIndex.end())
			{
				const bool prevDup = s_keyboardShortcutList[iShortcut->second].allowDuplicates;

				if (!shortcut->allowDuplicates || !prevDup)
				{
					s_keyRepeats[i] = 1;
					s_keyRepeats[iShortcut->second] = 1;
				}
			}
			else
			{
				keyComboIndex[key] = i;
			}
		}
		return s_keyRepeats;
	}
}
