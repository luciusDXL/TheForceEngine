#include <cstring>

#include "escapeMenu.h"
#include "delt.h"
#include "uiDraw.h"
#include <TFE_DarkForces/agent.h>
#include <TFE_DarkForces/util.h>
#include <TFE_DarkForces/hud.h>
#include <TFE_DarkForces/config.h>
#include <TFE_Game/reticle.h>
#include <TFE_Archive/archive.h>
#include <TFE_Settings/settings.h>
#include <TFE_Input/inputMapping.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Jedi/Renderer/RClassic_GPU/screenDrawGPU.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Level/roffscreenBuffer.h>
#include <TFE_System/system.h>

using namespace TFE_Jedi;
using namespace TFE_Input;

namespace TFE_DarkForces
{
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

	struct EscapeMenuState
	{
		JBool escMenuOpen = JFALSE;

		u32 escMenuFrameCount = 0;
		DeltFrame* escMenuFrames = nullptr;
		OffScreenBuffer* framebufferCopy = nullptr;
		u8* framebuffer = nullptr;

		Vec2i cursorPosAccum = { 0 };
		Vec2i cursorPos = { 0 };
		s32   buttonPressed = -1;
		bool  buttonHover = false;

		RenderTargetHandle renderTarget = nullptr;
	};
	static EscapeMenuState s_emState = {};

	void escMenu_resetCursor();
	void escMenu_handleMousePosition();
	bool escapeMenu_getTextures(TextureInfoList& texList, AssetPool pool);
	void escapeMenu_draw(JBool drawMouse, JBool drawBackground);
	EscapeMenuAction escapeMenu_updateUI();

	extern void pauseLevelSound();
	extern void resumeLevelSound();

	void escapeMenu_resetState()
	{
		// TFE: GPU Support.
		if (s_emState.renderTarget)
		{
			TFE_RenderBackend::freeRenderTarget(s_emState.renderTarget);
		}

		// Free memory
		freeOffScreenBuffer(s_emState.framebufferCopy);

		// Clear State.
		s_emState = {};
	}

	void escapeMenu_load()
	{
		if (!s_emState.escMenuFrames)
		{
			FilePath filePath;
			if (!TFE_Paths::getFilePath("MENU.LFD", &filePath)) { return; }
			Archive* archive = Archive::getArchive(ARCHIVE_LFD, "MENU", filePath.path);
			TFE_Paths::addLocalArchive(archive);
			s_emState.escMenuFrameCount = getFramesFromAnim("escmenu.anim", &s_emState.escMenuFrames);
			TFE_Paths::removeLastArchive();

			// TFE
			TFE_Jedi::renderer_addHudTextureCallback(escapeMenu_getTextures);
		}
	}

	void escapeMenu_copyBackground(u8* framebuffer, u8* palette)
	{
		u32 dispWidth, dispHeight;
		vfb_getResolution(&dispWidth, &dispHeight);

		// Draw the menu background, so it can be grayed out as well.
		s_emState.framebuffer = framebuffer;
		escapeMenu_draw(JFALSE, JFALSE);

		if (TFE_Jedi::getSubRenderer() == TSR_CLASSIC_GPU)
		{
			// 1. Create a render target to hold the frame.
			u32 prevWidth = 0, prevHeight = 0;
			if (s_emState.renderTarget)
			{
				TFE_RenderBackend::getRenderTargetDim(s_emState.renderTarget, &prevWidth, &prevHeight);
			}

			if (!s_emState.renderTarget || prevWidth != dispWidth || prevHeight != dispHeight)
			{
				TFE_RenderBackend::freeRenderTarget(s_emState.renderTarget);
				s_emState.renderTarget = TFE_RenderBackend::createRenderTarget(dispWidth, dispHeight);
			}

			// 2. Blit current frame to the new render target.
			TFE_Jedi::endRender();
			TFE_RenderBackend::swap(true);

			TFE_RenderBackend::copyBackbufferToRenderTarget(s_emState.renderTarget);
			TFE_RenderBackend::unbindRenderTarget();
		}
		else // Software renderer code.
		{
			if (s_emState.framebufferCopy && (s_emState.framebufferCopy->width != dispWidth || s_emState.framebufferCopy->height != dispHeight))
			{
				freeOffScreenBuffer(s_emState.framebufferCopy);
				s_emState.framebufferCopy = nullptr;
			}

			if (!s_emState.framebufferCopy)
			{
				s_emState.framebufferCopy = createOffScreenBuffer(dispWidth, dispHeight, OBF_NONE);
			}
			memcpy(s_emState.framebufferCopy->image, framebuffer, s_emState.framebufferCopy->size);
			s_emState.framebuffer = framebuffer;

			// Post process to convert sceen capture to grayscale.
			for (s32 i = 0; i < s_emState.framebufferCopy->size; i++)
			{
				u8 color = s_emState.framebufferCopy->image[i];
				u8* rgb = &palette[color * 3];
				u8 luminance = ((rgb[1] >> 1) + (rgb[0] >> 2) + (rgb[2] >> 2)) >> 1;
				s_emState.framebufferCopy->image[i] = 63 - luminance;
			}
		}
	}

	void escapeMenu_open(u8* framebuffer, u8* palette)
	{
		// TFE
		reticle_enable(false);

		pauseLevelSound();
		s_emState.escMenuOpen = JTRUE;

		escapeMenu_copyBackground(framebuffer, palette);

		escMenu_resetCursor();
		s_emState.buttonPressed = -1;
		s_emState.buttonHover = false;
	}

	void escapeMenu_close()
	{
		s_emState.escMenuOpen = JFALSE;
		resumeLevelSound();

		// TFE
		reticle_enable(true);
	}

	JBool escapeMenu_isOpen()
	{
		return s_emState.escMenuOpen;
	}

	void escapeMenu_addDeltFrame(TextureInfoList& texList, DeltFrame* frame)
	{
		TextureInfo texInfo = {};
		texInfo.type = TEXINFO_DF_DELT_TEX;
		texInfo.texData = &frame->texture;
		texList.push_back(texInfo);
	}

	bool escapeMenu_getTextures(TextureInfoList& texList, AssetPool pool)
	{
		for (u32 i = 0; i < s_emState.escMenuFrameCount; i++)
		{
			escapeMenu_addDeltFrame(texList, &s_emState.escMenuFrames[i]);
		}
		escapeMenu_addDeltFrame(texList, &s_cursor);
		return true;
	}

	void escapeMenu_drawGpu(JBool drawMouse, JBool drawBackground)
	{
		u32 dispWidth, dispHeight;
		vfb_getResolution(&dispWidth, &dispHeight);

		const fixed16_16 xScale = vfb_getXScale();
		const fixed16_16 yScale = vfb_getYScale();
		const s32 xOffset = vfb_getWidescreenOffset();

		// Draw the background.
		if (drawBackground)
		{
			screenGPU_addImageQuad(0, 0, dispWidth, dispHeight, (TextureGpu*)TFE_RenderBackend::getRenderTargetTexture(s_emState.renderTarget));
		}

		// Draw the menu.
		screenGPU_blitTextureScaled(&s_emState.escMenuFrames[0].texture, nullptr, intToFixed16(xOffset), 0, xScale, yScale, 31);

		if (s_levelComplete)
		{
			// Attempt to clean up the button positions, note this is only a problem at non-vanilla resolutions.
			fixed16_16 yOffset = (dispHeight == 200 || dispHeight == 400) ? 0 : round16(yScale / 2);

			if (s_emState.buttonPressed == ESC_BTN_ABORT && s_emState.buttonHover)
			{
				screenGPU_blitTextureScaled(&s_emState.escMenuFrames[3].texture, nullptr, intToFixed16(xOffset), yOffset, xScale, yScale, 31);
			}
			else
			{
				screenGPU_blitTextureScaled(&s_emState.escMenuFrames[4].texture, nullptr, intToFixed16(xOffset), yOffset, xScale, yScale, 31);
			}
		}
		if ((s_emState.buttonPressed > ESC_BTN_ABORT || (s_emState.buttonPressed == ESC_BTN_ABORT && !s_levelComplete)) && s_emState.buttonHover)
		{
			// Attempt to clean up the button positions, note this is only a problem at non-vanilla resolutions.
			fixed16_16 yOffset = (dispHeight == 200 || dispHeight == 400) ? 0 : round16(yScale / 2);
			yOffset = min(yOffset, 3 - s_emState.buttonPressed);

			// Draw the highlight button
			const s32 highlightIndices[] = { 1, 7, 9, 5 };
			screenGPU_blitTextureScaled(&s_emState.escMenuFrames[highlightIndices[s_emState.buttonPressed]].texture, nullptr, intToFixed16(xOffset), yOffset, xScale, yScale, 31);
		}
		// Draw the mouse.
		if (drawMouse)
		{
			screenGPU_blitTextureScaled(&s_cursor.texture, nullptr, intToFixed16(s_emState.cursorPos.x), intToFixed16(s_emState.cursorPos.z), xScale, yScale, 31);
		}
	}

	void escapeMenu_draw(JBool drawMouse, JBool drawBackground)
	{
		// TFE Note: handle GPU drawing differently, though the UI update is exactly the same.
		if (TFE_Jedi::getSubRenderer() == TSR_CLASSIC_GPU)
		{
			escapeMenu_drawGpu(drawMouse, drawBackground);
			return;
		}

		// Draw the screen capture.
		ScreenRect* drawRect = vfb_getScreenRect(VFB_RECT_UI);
		u32 dispWidth, dispHeight;
		vfb_getResolution(&dispWidth, &dispHeight);

		if (dispWidth == 320 && dispHeight == 200)
		{
			if (drawBackground)
			{
				hud_drawElementToScreen(s_emState.framebufferCopy, drawRect, 0, 0, s_emState.framebuffer);
			}
			// Draw the menu background.
			blitDeltaFrame(&s_emState.escMenuFrames[0], 0, 0, s_emState.framebuffer);

			if (s_levelComplete)
			{
				if (s_emState.buttonPressed == ESC_BTN_ABORT && s_emState.buttonHover)
				{
					blitDeltaFrame(&s_emState.escMenuFrames[3], 0, 0, s_emState.framebuffer);
				}
				else
				{
					blitDeltaFrame(&s_emState.escMenuFrames[4], 0, 0, s_emState.framebuffer);
				}
			}
			if ((s_emState.buttonPressed > ESC_BTN_ABORT || (s_emState.buttonPressed == ESC_BTN_ABORT && !s_levelComplete)) && s_emState.buttonHover)
			{
				// Draw the highlight button
				const s32 highlightIndices[] = { 1, 7, 9, 5 };
				blitDeltaFrame(&s_emState.escMenuFrames[highlightIndices[s_emState.buttonPressed]], 0, 0, s_emState.framebuffer);
			}

			// Draw the mouse.
			blitDeltaFrame(&s_cursor, s_emState.cursorPos.x, s_emState.cursorPos.z, s_emState.framebuffer);
		}
		else
		{
			const fixed16_16 xScale = vfb_getXScale();
			const fixed16_16 yScale = vfb_getYScale();
			const s32 xOffset = vfb_getWidescreenOffset();

			if (drawBackground)
			{
				hud_drawElementToScreen(s_emState.framebufferCopy, drawRect, 0, 0, s_emState.framebuffer);
			}
			// Draw the menu background.
			blitDeltaFrameScaled(&s_emState.escMenuFrames[0], xOffset, 0, xScale, yScale, s_emState.framebuffer);

			if (s_levelComplete)
			{
				// Attempt to clean up the button positions, note this is only a problem at non-vanilla resolutions.
				fixed16_16 yOffset = (dispHeight == 200 || dispHeight == 400) ? 0 : round16(yScale / 2);

				if (s_emState.buttonPressed == ESC_BTN_ABORT && s_emState.buttonHover)
				{
					blitDeltaFrameScaled(&s_emState.escMenuFrames[3], xOffset, yOffset, xScale, yScale, s_emState.framebuffer);
				}
				else
				{
					blitDeltaFrameScaled(&s_emState.escMenuFrames[4], xOffset, yOffset, xScale, yScale, s_emState.framebuffer);
				}
			}
			if ((s_emState.buttonPressed > ESC_BTN_ABORT || (s_emState.buttonPressed == ESC_BTN_ABORT && !s_levelComplete)) && s_emState.buttonHover)
			{
				// Attempt to clean up the button positions, note this is only a problem at non-vanilla resolutions.
				fixed16_16 yOffset = (dispHeight == 200 || dispHeight == 400) ? 0 : round16(yScale / 2);
				yOffset = min(yOffset, 3 - s_emState.buttonPressed);

				// Draw the highlight button
				const s32 highlightIndices[] = { 1, 7, 9, 5 };
				blitDeltaFrameScaled(&s_emState.escMenuFrames[highlightIndices[s_emState.buttonPressed]], xOffset, yOffset, xScale, yScale, s_emState.framebuffer);
			}

			// Draw the mouse.
			if (drawMouse)
			{
				blitDeltaFrameScaled(&s_cursor, s_emState.cursorPos.x, s_emState.cursorPos.z, xScale, yScale, s_emState.framebuffer);
			}
		}
	}

	EscapeMenuAction escapeMenu_update()
	{
		EscapeMenuAction action = escapeMenu_updateUI();
		if (action != ESC_CONTINUE)
		{
			s_emState.escMenuOpen = JFALSE;
			resumeLevelSound();

			// TFE
			reticle_enable(true);
		}

		escapeMenu_draw(JTRUE, JTRUE);
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
			s_emState.escMenuOpen = JFALSE;
		}

		s32 x = s_emState.cursorPos.x;
		s32 z = s_emState.cursorPos.z;

		// Move into "UI space"
		fixed16_16 xScale = vfb_getXScale();
		fixed16_16 yScale = vfb_getYScale();
		x = floor16(div16(intToFixed16(x - vfb_getWidescreenOffset()), xScale));
		z = floor16(div16(intToFixed16(z), yScale));

		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			s_emState.buttonPressed = -1;
			for (u32 i = 0; i < ESC_BTN_COUNT; i++)
			{
				if (x >= c_escButtons[i].x && x < c_escButtons[i].x + c_escButtonDim.x &&
					z >= c_escButtons[i].z && z < c_escButtons[i].z + c_escButtonDim.z)
				{
					s_emState.buttonPressed = s32(i);
					s_emState.buttonHover = true;
					break;
				}
			}
		}
		else if (TFE_Input::mouseDown(MBUTTON_LEFT) && s_emState.buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (x >= c_escButtons[s_emState.buttonPressed].x && x < c_escButtons[s_emState.buttonPressed].x + c_escButtonDim.x &&
				z >= c_escButtons[s_emState.buttonPressed].z && z < c_escButtons[s_emState.buttonPressed].z + c_escButtonDim.z)
			{
				s_emState.buttonHover = true;
			}
			else
			{
				s_emState.buttonHover = false;
			}
		}
		else
		{
			action = escapeMenu_handleAction(action, (s_emState.buttonPressed >= ESC_BTN_ABORT && s_emState.buttonHover) ? s_emState.buttonPressed : -1);
			// Reset.
			s_emState.buttonPressed = -1;
			s_emState.buttonHover = false;
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

		s_emState.cursorPosAccum = { (s32)displayInfo.width >> 1, (s32)displayInfo.height >> 1 };
		s_emState.cursorPos.x = clamp(s_emState.cursorPosAccum.x * (s32)height / (s32)displayInfo.height, 0, (s32)width - 3);
		s_emState.cursorPos.z = clamp(s_emState.cursorPosAccum.z * (s32)height / (s32)displayInfo.height, 0, (s32)height - 3);
	}

	void escMenu_handleMousePosition()
	{
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		u32 width, height;
		vfb_getResolution(&width, &height);

		s32 dx, dy;
		TFE_Input::getAccumulatedMouseMove(&dx, &dy);

		MonitorInfo monitorInfo;
		TFE_RenderBackend::getCurrentMonitorInfo(&monitorInfo);

		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		s_emState.cursorPosAccum = { mx, my };

		if (displayInfo.width >= displayInfo.height)
		{
			s_emState.cursorPos.x = clamp(s_emState.cursorPosAccum.x * (s32)height / (s32)displayInfo.height, 0, (s32)width - 3);
			s_emState.cursorPos.z = clamp(s_emState.cursorPosAccum.z * (s32)height / (s32)displayInfo.height, 0, (s32)height - 3);
		}
		else
		{
			s_emState.cursorPos.x = clamp(s_emState.cursorPosAccum.x * (s32)width / (s32)displayInfo.width, 0, (s32)width - 3);
			s_emState.cursorPos.z = clamp(s_emState.cursorPosAccum.z * (s32)width / (s32)displayInfo.width, 0, (s32)height - 3);
		}
	}
}