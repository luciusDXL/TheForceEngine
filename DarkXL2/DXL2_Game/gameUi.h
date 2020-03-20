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

namespace DXL2_GameUi
{
	void init(DXL2_Renderer* renderer);
	void openEscMenu();
	bool isEscMenuOpen();

	void update(Player* player);
	void draw(Player* player);
}
