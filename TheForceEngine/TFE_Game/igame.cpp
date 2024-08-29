#include "igame.h"
#include <TFE_FrontEndUI/console.h>
#include <TFE_DarkForces/darkForcesMain.h>
#include <TFE_Outlaws/outlawsMain.h>

enum GameConstants
{
	GAME_MEMORY_BASE  = 8 * 1024 * 1024, // 8 MB
	LEVEL_MEMORY_BASE = 8 * 1024 * 1024, // 8 MB
	RES_MEMORY_BASE   = 8 * 1024 * 1024, // 8 MB
};

using namespace TFE_Memory;
MemoryRegion* s_gameRegion = nullptr;
MemoryRegion* s_levelRegion = nullptr;

void displayMemoryUsage(const ConsoleArgList& args)
{
	char res[256];
	u64 blockCount, blockSize;
	region_getBlockInfo(s_gameRegion, &blockCount, &blockSize);
	TFE_Console::addToHistory("-------------------------------------------------------------------");
	TFE_Console::addToHistory("Region   | Memory Used | Current Capacity | Block Count | BlockSize");
	TFE_Console::addToHistory("-------------------------------------------------------------------");
	sprintf(res, "Game     | %11zu | %16zu | %11zu | %9zu", region_getMemoryUsed(s_gameRegion), region_getMemoryCapacity(s_gameRegion), blockCount, blockSize);
	TFE_Console::addToHistory(res);

	region_getBlockInfo(s_levelRegion, &blockCount, &blockSize);
	sprintf(res, "Level    | %11zu | %16zu | %11zu | %9zu", region_getMemoryUsed(s_levelRegion), region_getMemoryCapacity(s_levelRegion), blockCount, blockSize);
	TFE_Console::addToHistory(res);
	TFE_Console::addToHistory("-------------------------------------------------------------------");
}

void game_init()
{
	s_gameRegion  = region_create("game",  GAME_MEMORY_BASE);	// Region for "permanent" game allocations.
	s_levelRegion = region_create("level", LEVEL_MEMORY_BASE);	// Region for "per-level" game allocations.

	CCMD("displayMemoryUsage", displayMemoryUsage, 0, "Display memory usage.");
}

void game_destroy()
{
	region_destroy(s_gameRegion);
	region_destroy(s_levelRegion);

	s_gameRegion  = nullptr;
	s_levelRegion = nullptr;
}

void game_clearLevelData()
{
	region_clear(s_levelRegion);
}

IGame* createGame(GameID id)
{
	IGame* game = nullptr;
	switch (id)
	{
		case Game_Dark_Forces:
		{
			game = new TFE_DarkForces::DarkForces();
		} break;
		case Game_Outlaws:
		{
			game = new TFE_Outlaws::Outlaws();
		} break;
	}
	if (game)
	{
		game->id = id;
	}

	return game;
}

void freeGame(IGame* game)
{
	if (game)
	{
		game->exitGame();
		delete game;
	}
	region_clear(s_gameRegion);
	region_clear(s_levelRegion);
}

bool game_validateSourceData(GameID id, const char *path)
{
	switch (id)
	{
	case Game_Dark_Forces:
		return TFE_DarkForces::validateSourceData(path);
	case Game_Outlaws:
		return TFE_Outlaws::validateSourceData(path);
	}
	return false;	
}
