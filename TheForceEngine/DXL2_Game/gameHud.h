#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Asset/levelAsset.h>
#include "view.h"

class DXL2_Renderer;
class Player;

namespace DXL2_GameHud
{
	void init(DXL2_Renderer* renderer);

	void update(Player* player);
	void draw(Player* player);

	void setMessage(const char* msg);
	void clearMessage();
}
