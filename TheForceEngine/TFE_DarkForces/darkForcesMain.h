#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_Game/igame.h>

namespace TFE_DarkForces
{
	struct DarkForces : IGame
	{
		bool runGame(s32 argCount, const char* argv[], Stream* stream) override;
		void pauseGame(bool pause) override;
		void exitGame() override;
		void loopGame() override;
		bool serializeGameState(const char* filename, bool writeState) override;
	};

	extern void saveLevelStatus();
}
