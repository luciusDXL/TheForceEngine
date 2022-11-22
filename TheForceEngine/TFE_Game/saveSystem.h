#pragma once
//////////////////////////////////////////////////////////////////////
// Shared save/load functionality used  by all games.
//////////////////////////////////////////////////////////////////////
#include "igame.h"

namespace TFE_SaveSystem
{
	static const char* c_quickSaveName = "quicksave.tfe";

	void update(IGame* game);
	bool saveGame(IGame* game, const char* filename);
	bool loadGame(IGame* game, const char* filename);

	void postLoadRequest(const char* filename);
	void postSaveRequest(const char* filename);
	const char* loadRequestFilename();
	const char* saveRequestFilename();
}