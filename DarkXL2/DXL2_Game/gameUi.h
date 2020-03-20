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

enum EscapeMenuResult
{
	ESC_MENU_CONTINUE,	// Continue as we have been, nothing to report.
	ESC_MENU_ABORT,		// Abort the mission!
	ESC_MENU_NEXT,		// Progress to the next mission.
	ESC_MENU_QUIT		// Quit the game.
};

namespace DXL2_GameUi
{
	void init(DXL2_Renderer* renderer);
	void openEscMenu();
	bool isEscMenuOpen();
	void toggleNextMission(bool enable);

	EscapeMenuResult update(Player* player);
	void draw(Player* player);
}
