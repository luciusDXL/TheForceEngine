#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/levelAsset.h>
#include <TFE_Game/view.h>

class TFE_Renderer;
class Player;

enum GameUiResult
{
	GAME_UI_CONTINUE,		// Continue as we have been, nothing to report.
	GAME_UI_ABORT,			// Abort the mission!
	GAME_UI_NEXT_LEVEL,	// Progress to the next mission at the same difficulty level.
	GAME_UI_SELECT_LEVEL,	// Level selected, time to start.
	GAME_UI_QUIT,			// Quit the game.
	GAME_UI_CLOSE,			// Close the current dialog.
};

namespace TFE_GameUi
{
	void init(TFE_Renderer* renderer);
	void openEscMenu();
	void openAgentMenu();
	bool isEscMenuOpen();
	void toggleNextMission(bool enable);

	GameUiResult update(Player* player);
	void draw(Player* player);

	void reset();

	// Returns true if the game view should be drawn.
	// This will be false for fullscreen UI.
	bool shouldDrawGame();
	bool shouldUpdateGame();
	// Get the level selected in the UI.
	s32  getSelectedLevel(s32* difficulty);
}
