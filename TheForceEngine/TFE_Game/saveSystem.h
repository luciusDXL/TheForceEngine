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
		SAVE_IMAGE_WIDTH  = 426,
		SAVE_IMAGE_HEIGHT = 240,
	};
	struct SaveHeader
	{
		char fileName[256];
		char saveName[SAVE_MAX_NAME_LEN];
		char dateTime[256];
		char levelName[256];
		char levelId[256];
		char modNames[256];
		u32  imageData[SAVE_IMAGE_WIDTH * SAVE_IMAGE_HEIGHT];
		s32  saveVersion; 
		int  replayCounter;
	};

	void init();
	void destroy();

	void setCurrentGame(IGame* game);
	void setCurrentGame(GameID id);

	IGame * getCurrentGame();
	void update();
	bool saveGame(const char* filename, const char* saveName);
	bool loadGame(const char* filename);
	// Load only the header for UI.
	bool loadGameHeader(const char* filename, SaveHeader* header);

	bool versionValid(s32 version);
	void saveHeader(Stream* stream, const char* saveName);
	void loadHeader(Stream* stream, SaveHeader* header, const char* fileName);

	void postLoadRequest(const char* filename);
	void postSaveRequest(const char* filename, const char* saveName, s32 delay = 0);
	const char* loadRequestFilename();
	const char* saveRequestFilename();

	void getSaveFilenameFromIndex(s32 index, char* name);

	void populateSaveDirectory(std::vector<SaveHeader>& dir);
}