#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

class TFE_Renderer;
class Player;

namespace TFE_GameHud
{
	void init(TFE_Renderer* renderer);

	void update(Player* player);
	void draw(Player* player);

	void setMessage(const char* msg);
	void clearMessage();
}
