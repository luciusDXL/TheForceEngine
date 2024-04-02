#include "hotkeys.h"
#include "contextMenu.h"
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
}
