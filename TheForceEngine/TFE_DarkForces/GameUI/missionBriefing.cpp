#include <cstring>

#include "agentMenu.h"
#include "editBox.h"
#include "uiDraw.h"
#include <TFE_DarkForces/Landru/lactorDelt.h>
#include <TFE_DarkForces/Landru/lactorAnim.h>
#include <TFE_DarkForces/Landru/lpalette.h>
#include <TFE_DarkForces/Landru/lcanvas.h>
#include <TFE_DarkForces/Landru/ldraw.h>
#include <TFE_Archive/lfdArchive.h>
#include <TFE_DarkForces/agent.h>
#include <TFE_DarkForces/util.h>
#include <TFE_Archive/archive.h>
#include <TFE_Settings/settings.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	///////////////////////////////////////////
	// Constants
	///////////////////////////////////////////
	enum MissionBriefingConstants
	{
		BRIEF_VERT_MARGIN = 0,
		BRIEF_LINE_SCROLL = 12,
		BRIEF_PAGE_SCROLL = BRIEF_LINE_SCROLL*10,
	};
	enum BriefingButton
	{
		BRIEF_BTN_OK = 0,
		BRIEF_BTN_UP,
		BRIEF_BTN_DOWN,
		BRIEF_BTN_CANCEL,
		BRIEF_BTN_EASY,
		BRIEF_BTN_MEDIUM,
		BRIEF_BTN_HARD,
		BRIEF_BTN_COUNT,
	};
	static s32 s_buttonPressed = -1;
	static s32 s_keyPressed = -1;
	static JBool s_buttonHover = JFALSE;

	static Vec2i s_cursorPosAccum = { 0 };
	static Vec2i s_cursorPos = { 0 };

	static JBool s_briefingOpen = JFALSE;
	static s32 s_skill = 0;
	static LfdArchive s_archive;
	static s32 s_briefingMaxY;
	static LRect s_briefRect = { 25, 15, 155, 305 };
	static LRect s_overlayRect;
	static LRect s_missionTextRect;
	static LRect s_viewBounds;
	static s16 s_briefY;
	static LActor* s_briefActor = nullptr;
	static LActor* s_menuActor = nullptr;
	static LPalette* s_palette = nullptr;
	static u8* s_framebuffer = nullptr;

	enum
	{
		MENU_TYPE_BACK = 200,
		MENU_TYPE_OVERLAY = 201,
	};

	static const s16 c_frameTypes[] =
	{
		MENU_TYPE_BACK,
		MENU_TYPE_OVERLAY,
		-1, //		MENU_TYPE_BUTTON,
		-1, //		MENU_TYPE_BUTTON_UP,
		-1, //		MENU_TYPE_REPEAT_BUTTON,
		-1, //		MENU_TYPE_BUTTON_UP,
		-1, //		MENU_TYPE_REPEAT_BUTTON,
		-1, //		MENU_TYPE_BUTTON_UP,
		-1, //		MENU_TYPE_BUTTON,
		-1, //		MENU_TYPE_BUTTON_UP,
		-1, //		MENU_TYPE_BUTTON,
		-1, //		MENU_TYPE_BUTTON_UP,
		-1, //		MENU_TYPE_BUTTON,
		-1, //		MENU_TYPE_BUTTON_UP,
		-1, //		MENU_TYPE_BUTTON,
		-1, //		MENU_TYPE_BUTTON_UP,
	};

	void missionBriefing_resetCursor();

	///////////////////////////////////////////
	// API Implementation
	///////////////////////////////////////////
	void missionBriefing_start(const char* archive, const char* bgAnim, const char* mission, const char* palette, s32 skill)
	{
		s_briefingOpen = JFALSE;
		s_skill = skill;

		FilePath lfdPath;
		if (!TFE_Paths::getFilePath(archive, &lfdPath))
		{
			return;
		}

		// Load the mission briefing text.
		if (!s_archive.open(lfdPath.path))
		{
			return;
		}
		TFE_Paths::addLocalArchive(&s_archive);

		// Mission specific text and images.
		s_briefActor = lactorDelt_load(mission, &s_briefRect, 0, 0, 0);
		if (!s_briefActor)
		{
			TFE_Paths::removeLastArchive();
			s_archive.close();
		}

		// Menu Items
		LRect bounds;
		lcanvas_getBounds(&bounds);
		s_menuActor = lactorAnim_load(bgAnim, &bounds, 0, 0, 0);
		lactor_setTime(s_menuActor, -1, -1);
		s_palette = lpalette_load(palette);
		lpalette_setScreenPal(s_palette);

		s16 state_btn_index = 600;
		s16 button_index = 0;
		s16 slider_index = 0;
		s16 back_state = -1;
		s16 overlay_state = -1;
			   
		// Buttons
		for (s32 i = 0; i < s_menuActor->arraySize; i++)
		{
			LRect rect;
			lactor_setState(s_menuActor, i, 0);
			lactor_getFrame(s_menuActor, &rect);

			if (c_frameTypes[i] == MENU_TYPE_BACK)
			{
				back_state = i;
			}
			else if (c_frameTypes[i] == MENU_TYPE_OVERLAY)
			{
				overlay_state = i;
				lactorAnim_getFrame(s_menuActor, &s_missionTextRect);
			}
		}
		
		s_archive.close();
		TFE_Paths::removeLastArchive();
		
		LRect rect;
		lactor_setTime(s_briefActor, -1, -1);
		lactorDelt_getFrame(s_briefActor, &rect);

		s_briefingMaxY = (rect.bottom - rect.top) - (s_briefRect.bottom - s_briefRect.top) + BRIEF_VERT_MARGIN;
		// Round to a factor of the line scrolling value.
		s_briefingMaxY = s_briefingMaxY + BRIEF_LINE_SCROLL - (s_briefingMaxY % BRIEF_LINE_SCROLL);

		if (s_briefingMaxY < -BRIEF_VERT_MARGIN)
		{
			s_briefingMaxY = -BRIEF_VERT_MARGIN;
		}
		s_overlayRect = s_briefRect;
		s_briefY = -BRIEF_VERT_MARGIN;

		s_briefingOpen = JTRUE;

		s_framebuffer = ldraw_getBitmap();
		lcanvas_getBounds(&s_viewBounds);

		ltime_setFrameRate(20);
		missionBriefing_resetCursor();
	}
		
	void drawButton(BriefingButton id)
	{
		s32 pressed = 0;
		if ((s_buttonHover && id == s_buttonPressed) || (id == s_keyPressed))
		{
			pressed = 1;
		}
		else if (id >= BRIEF_BTN_EASY && s_skill == id - BRIEF_BTN_EASY)
		{
			pressed = 1;
		}

		lactor_setState(s_menuActor, 2*(1+id) + (pressed ? 0 : 1), 0);
		lactorAnim_draw(s_menuActor, &s_viewBounds, &s_viewBounds, 0, 0, JTRUE);
	}

	// TODO: Merge these with the agent menu and other menus.
	void missionBriefing_resetCursor()
	{
		// Reset the cursor.
		u32 width, height;
		vfb_getResolution(&width, &height);

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		s_cursorPosAccum = { (s32)displayInfo.width >> 1, (s32)displayInfo.height >> 1 };
		s_cursorPos.x = clamp(s_cursorPosAccum.x * (s32)height / (s32)displayInfo.height, 0, (s32)width - 3);
		s_cursorPos.z = clamp(s_cursorPosAccum.z * (s32)height / (s32)displayInfo.height, 0, (s32)height - 3);
	}
		
	void missionBriefing_handleMousePosition()
	{
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		u32 width, height;
		vfb_getResolution(&width, &height);

		s32 dx, dy;
		TFE_Input::getAccumulatedMouseMove(&dx, &dy);

		s_cursorPosAccum.x = clamp(s_cursorPosAccum.x + dx, 0, displayInfo.width);
		s_cursorPosAccum.z = clamp(s_cursorPosAccum.z + dy, 0, displayInfo.height);
		if (displayInfo.width >= displayInfo.height)
		{
			s_cursorPos.x = clamp(s_cursorPosAccum.x * (s32)height / (s32)displayInfo.height, 0, (s32)width - 3);
			s_cursorPos.z = clamp(s_cursorPosAccum.z * (s32)height / (s32)displayInfo.height, 0, (s32)height - 3);
		}
		else
		{
			s_cursorPos.x = clamp(s_cursorPosAccum.x * (s32)width / (s32)displayInfo.width, 0, (s32)width - 3);
			s_cursorPos.z = clamp(s_cursorPosAccum.z * (s32)width / (s32)displayInfo.width, 0, (s32)height - 3);
		}
	}

	void missionBriefing_scroll(s32 amt)
	{
		if (amt < 0 && s_briefY > -BRIEF_VERT_MARGIN)
		{
			s_briefY += amt;
			if (s_briefY < -BRIEF_VERT_MARGIN) { s_briefY = -BRIEF_VERT_MARGIN; }
		}
		else if (amt > 0 && s_briefY != s_briefingMaxY)
		{
			s_briefY += amt;
			if (s_briefY > s_briefingMaxY) { s_briefY = s_briefingMaxY; }
		}
	}
		
	JBool missionBriefing_handleInput(JBool* abort)
	{
		JBool exitBriefing = JFALSE;
		
		// Mouse interactions.
		missionBriefing_handleMousePosition();
		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			s_buttonPressed = -1;
			for (s32 i = 0; i < BRIEF_BTN_COUNT; i++)
			{
				lactor_setState(s_menuActor, 2 * (1 + i), 0);
				LRect buttonRect;
				lactorAnim_getFrame(s_menuActor, &buttonRect);

				if (s_cursorPos.x >= buttonRect.left && s_cursorPos.x < buttonRect.right &&
					s_cursorPos.z >= buttonRect.top && s_cursorPos.z < buttonRect.bottom)
				{
					s_buttonPressed = s32(i);
					s_buttonHover = JTRUE;
					break;
				}
			}
		}
		else if (TFE_Input::mouseDown(MBUTTON_LEFT) && s_buttonPressed >= 0)
		{
			lactor_setState(s_menuActor, 2 * (1 + s_buttonPressed), 0);
			LRect buttonRect;
			lactorAnim_getFrame(s_menuActor, &buttonRect);

			// Verify that the mouse is still over the button.
			if (s_cursorPos.x >= buttonRect.left && s_cursorPos.x < buttonRect.right &&
				s_cursorPos.z >= buttonRect.top && s_cursorPos.z < buttonRect.bottom)
			{
				s_buttonHover = JTRUE;
				if (ltime_isFrameReady())
				{
					if (s_buttonPressed == BRIEF_BTN_UP)
					{
						missionBriefing_scroll(-BRIEF_LINE_SCROLL);
					}
					else if (s_buttonPressed == BRIEF_BTN_DOWN)
					{
						missionBriefing_scroll(BRIEF_LINE_SCROLL);
					}
				}
			}
			else
			{
				s_buttonHover = JFALSE;
			}
		}
		else
		{
			if (s_buttonPressed >= 0 && s_buttonHover)
			{
				switch (s_buttonPressed)
				{
					case BRIEF_BTN_OK:
					{
						*abort = JFALSE;
						exitBriefing = JTRUE;
					} break;
					case BRIEF_BTN_UP:
					{
						missionBriefing_scroll(-BRIEF_LINE_SCROLL);
					} break;
					case BRIEF_BTN_DOWN:
					{
						missionBriefing_scroll(BRIEF_LINE_SCROLL);
					} break;
					case BRIEF_BTN_CANCEL:
					{
						*abort = JTRUE;
						exitBriefing = JTRUE;
					} break;
					case BRIEF_BTN_EASY:
					{
						s_skill = 0;
					} break;
					case BRIEF_BTN_MEDIUM:
					{
						s_skill = 1;
					} break;
					case BRIEF_BTN_HARD:
					{
						s_skill = 2;
					} break;
				}
			}
			// Reset.
			s_buttonPressed = -1;
			s_buttonHover = JFALSE;
		}

		// Keyboard shortcuts.
		if (ltime_isFrameReady())
		{
			s_keyPressed = -1;
			if (TFE_Input::keyDown(KEY_UP))
			{
				s_keyPressed = BRIEF_BTN_UP;
				missionBriefing_scroll(-BRIEF_LINE_SCROLL);
			}
			else if (TFE_Input::keyDown(KEY_DOWN))
			{
				s_keyPressed = BRIEF_BTN_DOWN;
				missionBriefing_scroll(BRIEF_LINE_SCROLL);
			}

			if (TFE_Input::keyDown(KEY_E))
			{
				s_keyPressed = BRIEF_BTN_EASY;
				s_skill = 0;
			}
			else if (TFE_Input::keyDown(KEY_M))
			{
				s_keyPressed = BRIEF_BTN_MEDIUM;
				s_skill = 1;
			}
			else if (TFE_Input::keyDown(KEY_H))
			{
				s_keyPressed = BRIEF_BTN_HARD;
				s_skill = 2;
			}

			if (TFE_Input::keyDown(KEY_C) || TFE_Input::keyDown(KEY_ESCAPE))
			{
				*abort = JTRUE;
				exitBriefing = JTRUE;
			}
			else if (TFE_Input::keyDown(KEY_O) || TFE_Input::keyDown(KEY_RETURN))
			{
				*abort = JFALSE;
				exitBriefing = JTRUE;
			}

			if (TFE_Input::keyDown(KEY_PAGEUP))
			{
				missionBriefing_scroll(-BRIEF_PAGE_SCROLL);
			}
			else if (TFE_Input::keyDown(KEY_PAGEDOWN))
			{
				missionBriefing_scroll(BRIEF_PAGE_SCROLL);
			}
		}

		return exitBriefing;
	}

	JBool missionBriefing_update(s32* skill, JBool* abort)
	{
		if (!s_briefingOpen)
		{
			*skill = s_skill;
			*abort = JFALSE;
			return JFALSE;
		}
		tfe_updateLTime();

		// Input
		if (missionBriefing_handleInput(abort))
		{
			s_briefingOpen = JFALSE;
			*skill = s_skill;
			return JFALSE;
		}

		// Background
		lcanvas_eraseRect(&s_viewBounds);
		lactor_setState(s_menuActor, 0, 0);
		lactorAnim_draw(s_menuActor, &s_viewBounds, &s_viewBounds, 0, 0, JTRUE);

		// Buttons
		for (s32 i = 0; i < BRIEF_BTN_COUNT; i++)
		{
			drawButton(BriefingButton(i));
		}

		// Briefing Text.
		LRect rect = s_missionTextRect;
		s16 diff = ((rect.right - rect.left) - s_briefActor->w) >> 1;
		s16 x = diff + rect.left - s_briefActor->x;
		s16 y = (rect.top - s_briefActor->y) - s_briefY;

		lcanvas_setClip(&s_missionTextRect);
		lactorDelt_draw(s_briefActor, &rect, &s_missionTextRect, x, y, JTRUE);
		lcanvas_clearClipRect();

		menu_blitCursor(s_cursorPos.x, s_cursorPos.z, s_framebuffer);

		memcpy(vfb_getCpuBuffer(), s_framebuffer, 320 * 200);
		vfb_swap();
		return JTRUE;
	}
	
	///////////////////////////////////////////
	// Internal Implementation
	///////////////////////////////////////////
}