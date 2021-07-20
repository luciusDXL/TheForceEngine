#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_Settings/gameSourceData.h>

namespace TFE
{
	struct IGame
	{
		virtual bool runGame() = 0;
		virtual void exitGame() = 0;
		virtual void loopGame() {};

		GameID id;
	};

	IGame* createGame(GameID id);
	void   freeGame(IGame* game);
}
