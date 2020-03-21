#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Asset/levelAsset.h>
#include "view.h"

class DXL2_Renderer;
class Player;

enum GameUiResult
{
	GAME_CONTINUE,		// Continue as we have been, nothing to report.
	GAME_ABORT,			// Abort the mission!
	GAME_NEXT_LEVEL,	// Progress to the next mission at the same difficulty level.
	GAME_SELECT_LEVEL,	// Level selected, time to start.
	GAME_QUIT,			// Quit the game.
};

namespace DXL2_GameUi
{
	void init(DXL2_Renderer* renderer);
	void openEscMenu();
	bool isEscMenuOpen();
	void toggleNextMission(bool enable);

	GameUiResult update(Player* player);
	void draw(Player* player);

	// Returns true if the game view should be drawn.
	// This will be false for fullscreen UI.
	bool shouldDrawGame();
	bool shouldUpdateGame();
	// Get the level selected in the UI.
	s32  getSelectedLevel(s32* difficulty);
}
