#include "igame.h"
#include <TFE_DarkForces/darkForcesMain.h>
#include <TFE_Outlaws/outlawsMain.h>

enum GameConstants
{
	GAME_MEMORY_BASE  = 2 * 1024 * 1024, // 2 MB
	LEVEL_MEMORY_BASE = 8 * 1024 * 1024, // 8 MB
};

using namespace TFE_Memory;
MemoryRegion* s_gameRegion = nullptr;
MemoryRegion* s_levelRegion = nullptr;

void game_init()
{
	s_gameRegion  = region_create("game",  GAME_MEMORY_BASE);	// Region for "permanent" game allocations.
	s_levelRegion = region_create("level", LEVEL_MEMORY_BASE);	// Region for "per-level" game allocations.
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
