#include "gameLoop.h"
#include "gameUi.h"
#include "gameHud.h"
#include "player.h"
#include <DXL2_System/system.h>
#include <DXL2_Renderer/renderer.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Asset/textureAsset.h>
#include <DXL2_Asset/fontAsset.h>
#include <DXL2_Asset/paletteAsset.h>
#include <DXL2_Asset/colormapAsset.h>
#include <DXL2_Input/input.h>
#include <DXL2_RenderBackend/renderBackend.h>
#include <assert.h>
#include <vector>
#include <algorithm>

namespace DXL2_GameUi
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

	struct GameMenus
	{
		Palette256* pal  = nullptr;
		ColorMap* cmap   = nullptr;
		Texture* escMenu = nullptr;
		Texture* cursor  = nullptr;
		// state

		bool load()
		{
			pal     = DXL2_Palette::get256("SECBASE.PAL");
			cmap    = DXL2_ColorMap::get("SECBASE.CMP");
			escMenu = DXL2_Texture::getFromAnim("escmenu.anim", "LFD/MENU.LFD");
			cursor  = DXL2_Texture::getFromDelt("cursor.delt", "LFD/MENU.LFD");
			return (pal && cmap && escMenu && cursor);
		}

		void unload()
		{
			DXL2_Texture::free(escMenu);
			DXL2_Texture::free(cursor);
			escMenu = nullptr;
			cursor = nullptr;
		}
	};

	struct AgentMenu
	{
		Palette256* pal;
		Texture* agentMenu;
		Texture* agentDlg;
		Texture* cursor;
		// state

		bool load()
		{
			pal       = DXL2_Palette::getPalFromPltt("agentmnu.pltt", "LFD/AGENTMNU.LFD");
			agentMenu = DXL2_Texture::getFromAnim("agentmnu.anim", "LFD/AGENTMNU.LFD");
			agentDlg  = DXL2_Texture::getFromAnim("agentdlg.anim", "LFD/AGENTMNU.LFD");
			cursor    = DXL2_Texture::getFromDelt("cursor.delt", "LFD/AGENTMNU.LFD");
			return (pal && agentMenu && agentDlg && cursor);
		}

		void unload()
		{
			DXL2_Texture::free(agentMenu);
			DXL2_Texture::free(agentDlg);
			DXL2_Texture::free(cursor);
			agentMenu = nullptr;
			agentDlg = nullptr;
			cursor = nullptr;
		}
	};

	static s32 s_buttonPressed = -1;
	static bool s_buttonHover = false;
	static bool s_drawGame = true;

	static DXL2_Renderer* s_renderer;
	static s32 s_scaleX = 1, s_scaleY = 1;
	static s32 s_uiScale = 256;
	static Palette256 s_prevPal;
	static const ColorMap* s_prevCMap;
	// Visual Cursor position
	static Vec2i s_cursorPos = { 0 };
	// Ui cursor position (in the original resolution)
	static Vec2i s_virtualCursorPos = { 0 };
	// Accumlation, so the movement speed can be consistent across resolutions.
	static Vec2i s_cursorPosAccum = { 0 };

	static GameMenus s_gameMenus = {};
	static AgentMenu s_agentMenu = {};

	static bool s_escMenuOpen = false;
	static bool s_agentMenuOpen = false;
	static bool s_nextMission = false;

	static s32 s_selectedLevel = 0;
	static s32 s_selectedDifficulty = 1;

	void drawEscapeMenu();
	void drawAgentMenu();
	GameUiResult updateEscapeMenu();
	GameUiResult updateAgentMenu();

	void init(DXL2_Renderer* renderer)
	{
		s_renderer = renderer;
		u32 width, height;
		s_renderer->getResolution(&width, &height);

		s_scaleX = 256 * s_uiScale * width  / (320 * 256);
		s_scaleY = 256 * s_uiScale * height / (200 * 256);

		// Preload everything - the original games would only load what was needed at the moment to save memory.
		// But its pretty safe to just load it all at once for now.
		s_gameMenus.load();
		s_agentMenu.load();
	}

	void openAgentMenu()
	{
		s_drawGame = false;
		s_escMenuOpen = false;
		s_agentMenuOpen = true;
		s_renderer->setPalette(s_agentMenu.pal);

		s_buttonPressed = -1;
		s_buttonHover = false;
	}

	void closeAgentMenu()
	{
		s_agentMenuOpen = false;
	}

	void openEscMenu()
	{
		s_escMenuOpen = true;
		s_prevPal = s_renderer->getPalette();
		s_prevCMap = s_renderer->getColorMap();
		s_renderer->setPalette(s_gameMenus.pal);
		s_renderer->setColorMap(s_gameMenus.cmap);
		DXL2_RenderCommon::enableGrayScale(true);

		s_buttonPressed = -1;
		s_buttonHover = false;
	}

	bool isEscMenuOpen()
	{
		return s_escMenuOpen;
	}

	void toggleNextMission(bool enable)
	{
		s_nextMission = enable;
	}

	void closeEscMenu()
	{
		s_escMenuOpen = false;
		DXL2_RenderCommon::enableGrayScale(false);
		s_renderer->setPalette(&s_prevPal);
		s_renderer->setColorMap(s_prevCMap);
	}
				
	GameUiResult update(Player* player)
	{
		GameUiResult result = GAME_CONTINUE;
		s_drawGame = !s_agentMenuOpen;
		// Mouse Handling
		DisplayInfo displayInfo;
		DXL2_RenderBackend::getDisplayInfo(&displayInfo);

		u32 width, height;
		s_renderer->getResolution(&width, &height);

		// Only update the mouse when a menu or dialog is open.
		if (s_escMenuOpen || s_agentMenuOpen)
		{
			s32 dx, dy;
			DXL2_Input::getMouseMove(&dx, &dy);

			s_cursorPosAccum.x += dx;
			s_cursorPosAccum.z += dy;
			s_cursorPos.x = std::max(0, std::min(s_cursorPosAccum.x * (s32)height / (s32)displayInfo.height, (s32)width - 1));
			s_cursorPos.z = std::max(0, std::min(s_cursorPosAccum.z * (s32)height / (s32)displayInfo.height, (s32)height - 1));

			// Calculate the virtual (UI) cursor position.
			s_virtualCursorPos.x = s_cursorPos.x * 256 / s_scaleX;
			s_virtualCursorPos.z = s_cursorPos.z * 256 / s_scaleY;
		}

		// Menus
		if (DXL2_Input::keyPressed(KEY_ESCAPE) && s_escMenuOpen)
		{
			closeEscMenu();
		}

		if (s_agentMenuOpen)
		{
			result = updateAgentMenu();
		}
		else if (s_escMenuOpen)
		{
			result = updateEscapeMenu();
		}

		return result;
	}

	// Returns true if the game view should be drawn.
	// This will be false for fullscreen UI.
	bool shouldDrawGame()
	{
		return s_drawGame;
	}

	// Returns true if the game view should be updated.
	// This will be false for modal UI.
	bool shouldUpdateGame()
	{
		return !s_escMenuOpen && !s_agentMenuOpen;
	}
		
	// Get the level selected in the UI.
	s32 getSelectedLevel(s32* difficulty)
	{
		*difficulty = s_selectedDifficulty;
		return s_selectedLevel;
	}

	void draw(Player* player)
	{
		if (s_agentMenuOpen)
		{
			drawAgentMenu();
		}
		else if (s_escMenuOpen)
		{
			drawEscapeMenu();
		}
	}
		
	//////////////////////////////////////////////////////////////////
	// Internal
	//////////////////////////////////////////////////////////////////
	void drawEscapeMenu()
	{
		s_renderer->applyColorEffect();
		// TODO: Figure out the real formula for this - this works for now though.
		const s32 x0 = ((s_gameMenus.escMenu->frames[0].width >> 2) - s_gameMenus.escMenu->frames[0].offsetX) * s_scaleX >> 8;
		const s32 y0 = ((s_gameMenus.escMenu->frames[0].height >> 2) - s_gameMenus.escMenu->frames[0].offsetY - 8) * s_scaleY >> 8;

		// Draw the menu
		s_renderer->blitImage(&s_gameMenus.escMenu->frames[0], x0, y0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);

		// Draw the next mission button if the mission has been completed.
		if (s_nextMission)
		{
			if (s_buttonPressed == ESC_ABORT && s_buttonHover)
			{
				s_renderer->blitImage(&s_gameMenus.escMenu->frames[3], x0, y0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
			}
			else
			{
				s_renderer->blitImage(&s_gameMenus.escMenu->frames[4], x0, y0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
			}
		}
		if ((s_buttonPressed > ESC_ABORT || (s_buttonPressed == ESC_ABORT && !s_nextMission)) && s_buttonHover)
		{
			// Draw the highlight button
			const s32 highlightIndices[] = { 1, 7, 9, 5 };
			s_renderer->blitImage(&s_gameMenus.escMenu->frames[highlightIndices[s_buttonPressed]], x0, y0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
		}

		// draw the mouse cursor.
		s_renderer->blitImage(&s_gameMenus.cursor->frames[0], s_cursorPos.x, s_cursorPos.z, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
	}

	GameUiResult updateEscapeMenu()
	{
		GameUiResult result = GAME_CONTINUE;
		
		if (DXL2_Input::mousePressed(MBUTTON_LEFT))
		{
			const s32 x = s_virtualCursorPos.x;
			const s32 z = s_virtualCursorPos.z;
			s_buttonPressed = -1;
			for (u32 i = 0; i < ESC_COUNT; i++)
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
		else if (DXL2_Input::mouseDown(MBUTTON_LEFT) && s_buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			const s32 x = s_virtualCursorPos.x;
			const s32 z = s_virtualCursorPos.z;
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
			// Activate button 's_escButtonPressed'
			if (s_buttonPressed >= ESC_ABORT && s_buttonHover)
			{
				switch (s_buttonPressed)
				{
				case ESC_ABORT:
					result = s_nextMission ? GAME_NEXT_LEVEL : GAME_ABORT;
					closeEscMenu();
					break;
				case ESC_CONFIG:
					// TODO
					break;
				case ESC_QUIT:
					result = GAME_QUIT;
					closeEscMenu();
					break;
				case ESC_RETURN:
					closeEscMenu();
					break;
				};
			}

			// Reset.
			s_buttonPressed = -1;
			s_buttonHover = false;
		}
		return result;
	}

	void drawAgentMenu()
	{
		// Clear part of the background.
		// TODO: Fade out the game view when existing, so the screen is black when drawing the menu.
		s_renderer->drawColoredQuad(0, 0, 320, 200, 0);

		// Draw the menu
		s_renderer->blitImage(&s_agentMenu.agentMenu->frames[0], 0, 0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);

		// draw the mouse cursor.
		s_renderer->blitImage(&s_gameMenus.cursor->frames[0], s_cursorPos.x, s_cursorPos.z, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
	}

	GameUiResult updateAgentMenu()
	{
		// For now just continue...
		if (DXL2_Input::mouseDown(MBUTTON_LEFT))
		{
			closeAgentMenu();
			return GAME_SELECT_LEVEL;
		}
		return GAME_CONTINUE;
	}
}
