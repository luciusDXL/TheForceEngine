#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_Game/igame.h>

namespace TFE
{
	struct DarkForces : IGame
	{
		bool runGame() override;
		void exitGame() override;
	};
}
