#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_Game/igame.h>
#include <TFE_FileSystem/physfswrapper.h>

namespace TFE_DarkForces
{
	struct DarkForces : IGame
	{
		bool runGame(s32 argCount, const char* argv[], vpFile* stream) override;
		void pauseGame(bool pause) override;
		void pauseSound(bool pause) override;
		void restartMusic() override;
		void exitGame() override;
		void loopGame() override;
		bool serializeGameState(vpFile* stream, const char* filename, bool writeState) override;
		bool canSave() override;
		bool isPaused() override;
		void getLevelName(char* name) override;
		void getModList(char* modList) override;
	};

	extern void saveLevelStatus();
	bool validateSourceData(const char *path);
}
