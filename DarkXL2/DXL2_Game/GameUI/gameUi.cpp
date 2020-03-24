#include "gameUi.h"
#include "editBox.h"
#include "escMenu.h"
#include "agentMenu.h"
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

namespace DXL2_GameUi
{
	static Texture* s_cursor;

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

	static bool s_drawMouseCursor = false;
	static bool s_escMenuOpen = false;
	static bool s_agentMenuOpen = false;
	static bool s_nextMission = false;

	static s32 s_selectedLevel = 0;
	static s32 s_selectedDifficulty = 1;

	void drawAgentMenu();
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
		DXL2_LevelList::load();
		DXL2_EscapeMenu::load();
		DXL2_AgentMenu::load();
				
		s_cursor = DXL2_Texture::getFromDelt("cursor.delt", "LFD/AGENTMNU.LFD");
	}

	void openAgentMenu()
	{
		s_drawGame = false;
		s_escMenuOpen = false;
		s_agentMenuOpen = true;
		s_drawMouseCursor = true;
		DXL2_AgentMenu::open(s_renderer);

		s_buttonPressed = -1;
		s_buttonHover = false;
	}

	void closeAgentMenu()
	{
		s_agentMenuOpen = false;
		s_drawMouseCursor = false;
		DXL2_AgentMenu::close();
	}

	void openEscMenu()
	{
		s_escMenuOpen = true;
		s_drawMouseCursor = true;
		s_prevPal = s_renderer->getPalette();
		s_prevCMap = s_renderer->getColorMap();
		DXL2_EscapeMenu::open(s_renderer);

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
		s_drawMouseCursor = false;
		DXL2_EscapeMenu::close();
		s_renderer->setPalette(&s_prevPal);
		s_renderer->setColorMap(s_prevCMap);
	}

	void reset()
	{
		s_escMenuOpen = false;
		s_agentMenuOpen = false;
		s_drawMouseCursor = false;
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
			result = DXL2_AgentMenu::update(&s_virtualCursorPos, &s_buttonPressed, &s_buttonHover, &s_selectedLevel);
			if (result == GAME_SELECT_LEVEL)
			{
				closeAgentMenu();
			}
		}
		else if (s_escMenuOpen)
		{
			result = DXL2_EscapeMenu::update(&s_virtualCursorPos, s_nextMission, &s_buttonPressed, &s_buttonHover);
			if (result == GAME_CLOSE)
			{
				closeEscMenu();
				result = GAME_CONTINUE;
			}
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
			DXL2_AgentMenu::draw(s_renderer, s_scaleX, s_scaleY, s_buttonPressed, s_buttonHover, s_nextMission);
		}
		else if (s_escMenuOpen)
		{
			DXL2_EscapeMenu::draw(s_renderer, s_scaleX, s_scaleY, s_buttonPressed, s_buttonHover, s_nextMission);
		}
		
		if (s_drawMouseCursor)
		{
			s_renderer->blitImage(&s_cursor->frames[0], s_cursorPos.x, s_cursorPos.z, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
		}
	}
		
	//////////////////////////////////////////////////////////////////
	// Internal
	//////////////////////////////////////////////////////////////////
}
