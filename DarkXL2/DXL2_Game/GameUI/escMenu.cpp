#include "gameUi.h"
#include "editBox.h"
#include <DXL2_Game/gameLoop.h>
#include <DXL2_Game/gameHud.h>
#include <DXL2_Game/player.h>
#include <DXL2_System/system.h>
#include <DXL2_Renderer/renderer.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Asset/textureAsset.h>
#include <DXL2_Asset/fontAsset.h>
#include <DXL2_Asset/paletteAsset.h>
#include <DXL2_Asset/colormapAsset.h>
#include <DXL2_Asset/levelList.h>
#include <DXL2_Input/input.h>
#include <DXL2_RenderBackend/renderBackend.h>
#include <assert.h>
#include <vector>
#include <algorithm>

namespace DXL2_EscapeMenu
{
	///////////////////////////////////////
	// UI Definitions
	// TODO: Move these to game data.
	///////////////////////////////////////
	enum EscapeButtons
	{
		ESC_ABORT,
		ESC_CONFIG,
		ESC_QUIT,
		ESC_RETURN,
		ESC_COUNT
	};
	static const Vec2i c_escButtons[ESC_COUNT] =
	{
		{64, 35},	// ESC_ABORT
		{64, 55},	// ESC_CONFIG
		{64, 75},	// ESC_QUIT
		{64, 99},	// ESC_RETURN
	};
	static const Vec2i c_escButtonDim = { 96, 16 };

	static Palette256* s_pal = nullptr;
	static ColorMap* s_cmap = nullptr;
	static Texture* s_escMenu = nullptr;
		
	bool load()
	{
		s_pal = DXL2_Palette::get256("SECBASE.PAL");
		s_cmap = DXL2_ColorMap::get("SECBASE.CMP");
		s_escMenu = DXL2_Texture::getFromAnim("escmenu.anim", "LFD/MENU.LFD");
		return s_pal && s_cmap && s_escMenu;
	}

	void unload()
	{
		DXL2_Texture::free(s_escMenu);
		s_escMenu = nullptr;
	}

	void open(DXL2_Renderer* renderer)
	{
		renderer->setPalette(s_pal);
		renderer->setColorMap(s_cmap);
		DXL2_RenderCommon::enableGrayScale(true);
	}

	void close()
	{
		DXL2_RenderCommon::enableGrayScale(false);
	}
		
	void draw(DXL2_Renderer* renderer, s32 scaleX, s32 scaleY, s32 buttonPressed, bool buttonHover, bool nextMission)
	{
		renderer->applyColorEffect();
		// TODO: Figure out the real formula for this - this works for now though.
		const s32 x0 = ((s_escMenu->frames[0].width >> 2) - s_escMenu->frames[0].offsetX) * scaleX >> 8;
		const s32 y0 = ((s_escMenu->frames[0].height >> 2) - s_escMenu->frames[0].offsetY - 8) * scaleY >> 8;

		// Draw the menu
		renderer->blitImage(&s_escMenu->frames[0], x0, y0, scaleX, scaleY, 31, TEX_LAYOUT_HORZ);

		// Draw the next mission button if the mission has been completed.
		if (nextMission)
		{
			if (buttonPressed == ESC_ABORT && buttonHover)
			{
				renderer->blitImage(&s_escMenu->frames[3], x0, y0, scaleX, scaleY, 31, TEX_LAYOUT_HORZ);
			}
			else
			{
				renderer->blitImage(&s_escMenu->frames[4], x0, y0, scaleX, scaleY, 31, TEX_LAYOUT_HORZ);
			}
		}
		if ((buttonPressed > ESC_ABORT || (buttonPressed == ESC_ABORT && !nextMission)) && buttonHover)
		{
			// Draw the highlight button
			const s32 highlightIndices[] = { 1, 7, 9, 5 };
			renderer->blitImage(&s_escMenu->frames[highlightIndices[buttonPressed]], x0, y0, scaleX, scaleY, 31, TEX_LAYOUT_HORZ);
		}
	}

	GameUiResult update(const Vec2i* cursor, bool nextMission, s32* buttonPressed, bool* buttonHover)
	{
		GameUiResult result = GAME_CONTINUE;
		
		const s32 x = cursor->x;
		const s32 z = cursor->z;
		if (DXL2_Input::mousePressed(MBUTTON_LEFT))
		{
			*buttonPressed = -1;
			for (u32 i = 0; i < ESC_COUNT; i++)
			{
				if (x >= c_escButtons[i].x && x < c_escButtons[i].x + c_escButtonDim.x &&
					z >= c_escButtons[i].z && z < c_escButtons[i].z + c_escButtonDim.z)
				{
					*buttonPressed = s32(i);
					*buttonHover = true;
					break;
				}
			}
		}
		else if (DXL2_Input::mouseDown(MBUTTON_LEFT) && *buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (x >= c_escButtons[*buttonPressed].x && x < c_escButtons[*buttonPressed].x + c_escButtonDim.x &&
				z >= c_escButtons[*buttonPressed].z && z < c_escButtons[*buttonPressed].z + c_escButtonDim.z)
			{
				*buttonHover = true;
			}
			else
			{
				*buttonHover = false;
			}
		}
		else
		{
			// Activate button 's_escButtonPressed'
			if ((*buttonPressed) >= ESC_ABORT && (*buttonHover))
			{
				switch (*buttonPressed)
				{
				case ESC_ABORT:
					result = nextMission ? GAME_NEXT_LEVEL : GAME_ABORT;
					close();
					break;
				case ESC_CONFIG:
					// TODO
					break;
				case ESC_QUIT:
					result = GAME_QUIT;
					close();
					break;
				case ESC_RETURN:
					result = GAME_CLOSE;
					break;
				};
			}

			// Reset.
			*buttonPressed = -1;
			*buttonHover = false;
		}
		return result;
	}
}
