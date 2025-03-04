#include "hotkeys.h"
#include "contextMenu.h"
#include "levelEditor.h"
#include "editGeometry.h"
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
#include <TFE_Input/input.h>
#include <TFE_System/system.h>

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

	// Entity
	u64 s_editActions = ACTION_NONE;
	u32 s_drawActions = DRAW_ACTION_NONE;
	s32 s_rotationDelta = 0;

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

	void updateGeneralHotkeys()
	{
		if (TFE_Input::keyPressed(KEY_Z) && TFE_Input::keyModDown(KEYMOD_CTRL))
		{
			s_editActions = ACTION_UNDO;
		}
		else if (TFE_Input::keyPressed(KEY_Y) && TFE_Input::keyModDown(KEYMOD_CTRL))
		{
			s_editActions = ACTION_REDO;
		}
		if (TFE_Input::keyPressed(KEY_TAB))
		{
			s_editActions = ACTION_SHOW_ALL_LABELS;
		}
	}

	void updateEntityEditHotkeys()
	{
		if (TFE_Input::keyPressed(KEY_INSERT)) { s_editActions = ACTION_PLACE; }
		if (TFE_Input::keyPressed(KEY_DELETE)) { s_editActions = ACTION_DELETE; }

		if (TFE_Input::keyDown(KEY_X)) { s_editActions |= ACTION_MOVE_X; }
		if (TFE_Input::keyDown(KEY_Y) && s_view != EDIT_VIEW_2D) { s_editActions |= ACTION_MOVE_Y; }
		if (TFE_Input::keyDown(KEY_Z)) { s_editActions |= ACTION_MOVE_Z; }
		if (TFE_Input::keyModDown(KEYMOD_CTRL)) { s_editActions |= ACTION_ROTATE; }

		if (s_editMode == LEDIT_ENTITY)
		{
			s32 dummy;
			TFE_Input::getMouseWheel(&dummy, &s_rotationDelta);
		}
	}

	void updateGuidelineHotkeys()
	{
		if (TFE_Input::keyPressed(KEY_INSERT)) { s_editActions = ACTION_PLACE; }
		if (TFE_Input::keyPressed(KEY_DELETE)) { s_editActions = ACTION_DELETE; }
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
			if (TFE_Input::keyPressedWithRepeat(KEY_LEFTBRACKET))
			{
				setCurveSegDelta(delta - 1);
			}
			else if (TFE_Input::keyPressedWithRepeat(KEY_RIGHTBRACKET))
			{
				setCurveSegDelta(delta + 1);
			}
			else if (TFE_Input::keyPressed(KEY_BACKSLASH))
			{
				setCurveSegDelta();
			}
		}
	}

	bool getEditAction(u64 action)
	{
		return (s_editActions & action) != 0u;
	}

	void handleHotkeys()
	{
		s_editActions = ACTION_NONE;
		s_drawActions = DRAW_ACTION_NONE;
		handleMouseClick();
		if (isUiModal()) { return; }

		updateGeneralHotkeys();
		if (s_editMode == LEDIT_ENTITY || s_editMode == LEDIT_NOTES)
		{
			updateEntityEditHotkeys();
		}
		else if (s_editMode == LEDIT_DRAW || s_editMode == LEDIT_GUIDELINES)
		{
			updateDrawModeHotkeys();
			// Guideline-only hotkeys.
			if (s_editMode == LEDIT_GUIDELINES)
			{
				updateGuidelineHotkeys();
			}
		}

		// Copy and Paste.
		if (s_editMode == LEDIT_ENTITY || s_editMode == LEDIT_WALL || s_editMode == LEDIT_SECTOR)
		{
			if (TFE_Input::keyPressed(KEY_C) && TFE_Input::keyModDown(KEYMOD_CTRL))
			{
				s_editActions = ACTION_COPY;
			}
			else if (TFE_Input::keyPressed(KEY_V) && TFE_Input::keyModDown(KEYMOD_CTRL))
			{
				s_editActions = ACTION_PASTE;
			}
		}
	}
}
