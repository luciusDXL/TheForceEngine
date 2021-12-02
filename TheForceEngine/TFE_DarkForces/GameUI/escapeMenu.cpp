#include <cstring>

#include "escapeMenu.h"
#include "delt.h"
#include "uiDraw.h"
#include <TFE_DarkForces/agent.h>
#include <TFE_DarkForces/util.h>
#include <TFE_DarkForces/hud.h>
#include <TFE_DarkForces/config.h>
#include <TFE_Archive/archive.h>
#include <TFE_Settings/settings.h>
#include <TFE_Input/inputMapping.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Level/roffscreenBuffer.h>
#include <TFE_System/system.h>

using namespace TFE_Jedi;
using namespace TFE_Input;

namespace TFE_DarkForces
{
	static JBool s_escMenuOpen = JFALSE;

	enum EscapeButtons
	{
		ESC_BTN_ABORT,
		ESC_BTN_CONFIG,
		ESC_BTN_QUIT,
		ESC_BTN_RETURN,
		ESC_BTN_COUNT
	};
	static const Vec2i c_escButtons[ESC_BTN_COUNT] =
	{
		{64, 35},	// ESC_ABORT
		{64, 55},	// ESC_CONFIG
		{64, 75},	// ESC_QUIT
		{64, 99},	// ESC_RETURN
	};
	static const Vec2i c_escButtonDim = { 96, 16 };
	static u32 s_escMenuFrameCount;
	static DeltFrame* s_escMenuFrames = nullptr;
	static OffScreenBuffer* s_framebufferCopy = nullptr;
	static u8* s_framebuffer = nullptr;

	static Vec2i s_cursorPosAccum;
	static Vec2i s_cursorPos;
	static s32  s_buttonPressed;
	static bool s_buttonHover;

	void escMenu_resetCursor();
	void escMenu_handleMousePosition();
	EscapeMenuAction escapeMenu_updateUI();

	extern void pauseLevelSound();
	extern void resumeLevelSound();

	void escapeMenu_resetState()
	{
		s_escMenuFrames = nullptr;
		s_framebufferCopy = nullptr;
		s_framebuffer = nullptr;
		s_escMenuOpen = JFALSE;
	}

	void escapeMenu_open(u8* framebuffer, u8* palette)
	{
		pauseLevelSound();
		s_escMenuOpen = JTRUE;
		if (!s_escMenuFrames)
		{
			FilePath filePath;
			if (!TFE_Paths::getFilePath("MENU.LFD", &filePath)) { return; }
			Archive* archive = Archive::getArchive(ARCHIVE_LFD, "MENU", filePath.path);
			TFE_Paths::addLocalArchive(archive);

			s_escMenuFrameCount = getFramesFromAnim("escmenu.anim", &s_escMenuFrames);
			
			TFE_Paths::removeLastArchive();
		}

		u32 dispWidth, dispHeight;
		vfb_getResolution(&dispWidth, &dispHeight);
		if (s_framebufferCopy && (s_framebufferCopy->width != dispWidth || s_framebufferCopy->height != dispHeight))
		{
			freeOffScreenBuffer(s_framebufferCopy);
			s_framebufferCopy = nullptr;
		}

		if (!s_framebufferCopy)
		{
			s_framebufferCopy = createOffScreenBuffer(dispWidth, dispHeight, OBF_NONE);
		}
		memcpy(s_framebufferCopy->image, framebuffer, s_framebufferCopy->size);
		s_framebuffer = framebuffer;

		// Post process to convert sceen capture to grayscale.
		for (s32 i = 0; i < s_framebufferCopy->size; i++)
		{
			u8 color = s_framebufferCopy->image[i];
			u8* rgb = &palette[color * 3];
			u8 luminance = ((rgb[1] >> 1) + (rgb[0] >> 2) + (rgb[2] >> 2)) >> 1;
			s_framebufferCopy->image[i] = 63 - luminance;
		}

		escMenu_resetCursor();
		s_buttonPressed = -1;
		s_buttonHover = false;
	}

	JBool escapeMenu_isOpen()
	{
		return s_escMenuOpen;
	}

	EscapeMenuAction escapeMenu_update()
	{
		EscapeMenuAction action = escapeMenu_updateUI();
		if (action != ESC_CONTINUE)
		{
			s_escMenuOpen = JFALSE;
			resumeLevelSound();
		}
		
		// Draw the screen capture.
		ScreenRect* drawRect = vfb_getScreenRect(VFB_RECT_UI);
		u32 dispWidth, dispHeight;
		vfb_getResolution(&dispWidth, &dispHeight);

		if (dispWidth == 320 && dispHeight == 200)
		{
			hud_drawElementToScreen(s_framebufferCopy, drawRect, 0, 0, s_framebuffer);
			// Draw the menu background.
			blitDeltaFrame(&s_escMenuFrames[0], 0, 0, s_framebuffer);

			if (s_levelComplete)
			{
				if (s_buttonPressed == ESC_BTN_ABORT && s_buttonHover)
				{
					blitDeltaFrame(&s_escMenuFrames[3], 0, 0, s_framebuffer);
				}
				else
				{
					blitDeltaFrame(&s_escMenuFrames[4], 0, 0, s_framebuffer);
				}
			}
			if ((s_buttonPressed > ESC_BTN_ABORT || (s_buttonPressed == ESC_BTN_ABORT && !s_levelComplete)) && s_buttonHover)
			{
				// Draw the highlight button
				const s32 highlightIndices[] = { 1, 7, 9, 5 };
				blitDeltaFrame(&s_escMenuFrames[highlightIndices[s_buttonPressed]], 0, 0, s_framebuffer);
			}

			// Draw the mouse.
			blitDeltaFrame(&s_cursor, s_cursorPos.x, s_cursorPos.z, s_framebuffer);
		}
		else
		{
			const fixed16_16 xScale = vfb_getXScale();
			const fixed16_16 yScale = vfb_getYScale();
			const s32 xOffset = vfb_getWidescreenOffset();

			hud_drawElementToScreen(s_framebufferCopy, drawRect, 0, 0, s_framebuffer);
			// Draw the menu background.
			blitDeltaFrameScaled(&s_escMenuFrames[0], xOffset, 0, xScale, yScale, s_framebuffer);

			if (s_levelComplete)
			{
				if (s_buttonPressed == ESC_BTN_ABORT && s_buttonHover)
				{
					blitDeltaFrameScaled(&s_escMenuFrames[3], xOffset, 0, xScale, yScale, s_framebuffer);
				}
				else
				{
					blitDeltaFrameScaled(&s_escMenuFrames[4], xOffset, 0, xScale, yScale, s_framebuffer);
				}
			}
			if ((s_buttonPressed > ESC_BTN_ABORT || (s_buttonPressed == ESC_BTN_ABORT && !s_levelComplete)) && s_buttonHover)
			{
				// Draw the highlight button
				const s32 highlightIndices[] = { 1, 7, 9, 5 };
				blitDeltaFrameScaled(&s_escMenuFrames[highlightIndices[s_buttonPressed]], xOffset, 0, xScale, yScale, s_framebuffer);
			}

			// Draw the mouse.
			blitDeltaFrameScaled(&s_cursor, s_cursorPos.x, s_cursorPos.z, xScale, yScale, s_framebuffer);
		}
		return action;
	}

	///////////////////////////////////////
	// Internal
	///////////////////////////////////////
	EscapeMenuAction escapeMenu_handleAction(EscapeMenuAction action, s32 actionPressed)
	{
		if (actionPressed < 0)
		{
			// Handle keyboard shortcuts.
			if ((TFE_Input::keyPressed(KEY_A) && !s_levelComplete) || (TFE_Input::keyPressed(KEY_N) && s_levelComplete))
			{
				actionPressed = ESC_BTN_ABORT;
			}
			if (TFE_Input::keyPressed(KEY_C))
			{
				actionPressed = ESC_BTN_CONFIG;
			}
			if (TFE_Input::keyPressed(KEY_Q))
			{
				actionPressed = ESC_BTN_QUIT;
			}
			if (TFE_Input::keyPressed(KEY_R))
			{
				actionPressed = ESC_BTN_RETURN;
			}
		}
		
		switch (actionPressed)
		{
		case ESC_BTN_ABORT:
			action = ESC_ABORT_OR_NEXT;
			break;
		case ESC_BTN_CONFIG:
			action = ESC_CONFIG;
			break;
		case ESC_BTN_QUIT:
			action = ESC_QUIT;
			break;
		case ESC_BTN_RETURN:
			action = ESC_RETURN;
			break;
		};
		return action;
	}

	EscapeMenuAction escapeMenu_updateUI()
	{
		EscapeMenuAction action = ESC_CONTINUE;
		escMenu_handleMousePosition();
		if (inputMapping_getActionState(IADF_MENU_TOGGLE) == STATE_PRESSED)
		{
			action = ESC_RETURN;
			s_escMenuOpen = JFALSE;
		}

		s32 x = s_cursorPos.x;
		s32 z = s_cursorPos.z;

		// Move into "UI space"
		fixed16_16 xScale = vfb_getXScale();
		fixed16_16 yScale = vfb_getYScale();
		x = floor16(div16(intToFixed16(x - vfb_getWidescreenOffset()), xScale));
		z = floor16(div16(intToFixed16(z), yScale));

		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			s_buttonPressed = -1;
			for (u32 i = 0; i < ESC_BTN_COUNT; i++)
			{
				if (x >= c_escButtons[i].x && x < c_escButtons[i].x + c_escButtonDim.x &&
					z >= c_escButtons[i].z && z < c_escButtons[i].z + c_escButtonDim.z)
				{
					s_buttonPressed = s32(i);
					s_buttonHover = true;
					break;
				}
			}
		}
		else if (TFE_Input::mouseDown(MBUTTON_LEFT) && s_buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (x >= c_escButtons[s_buttonPressed].x && x < c_escButtons[s_buttonPressed].x + c_escButtonDim.x &&
				z >= c_escButtons[s_buttonPressed].z && z < c_escButtons[s_buttonPressed].z + c_escButtonDim.z)
			{
				s_buttonHover = true;
			}
			else
			{
				s_buttonHover = false;
			}
		}
		else
		{
			action = escapeMenu_handleAction(action, (s_buttonPressed >= ESC_BTN_ABORT && s_buttonHover) ? s_buttonPressed : -1);
			// Reset.
			s_buttonPressed = -1;
			s_buttonHover = false;
		}

		return action;
	}

	// The cursor is handled independently for the Escape Menu for now so it can later handle
	// widescreen. However, it may be better to merge these functions anyway.
	void escMenu_resetCursor()
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

	void escMenu_handleMousePosition()
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
}