#pragma once
//////////////////////////////////////////////////////////////////////
// Shared save/load functionality used  by all games.
//////////////////////////////////////////////////////////////////////
#include "igame.h"
#include <TFE_Asset/imageAsset.h>

namespace TFE_SaveSystem
{
	static const char* c_quickSaveName = "quicksave.tfe";
	enum SaveSystemConst
	{
		SAVE_MAX_NAME_LEN = 64,
	};
	struct SaveHeader
	{
		char saveName[SAVE_MAX_NAME_LEN];
		char dateTime[256];
		char levelName[256];
		char modNames[256];
		Image image;
	};

	void init();
	void destroy();

	void setCurrentGame(IGame* game);
	void update();
	bool saveGame(const char* filename, const char* saveName);
	bool loadGame(const char* filename);
	// Load only the header for UI.
	bool loadGameHeader(const char* filename, SaveHeader* header);

	void postLoadRequest(const char* filename);
	void postSaveRequest(const char* filename, const char* saveName);
	const char* loadRequestFilename();
	const char* saveRequestFilename();
}