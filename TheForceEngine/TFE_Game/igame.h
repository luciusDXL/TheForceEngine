#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_Settings/gameSourceData.h>
#include <TFE_Memory/memoryRegion.h>

extern MemoryRegion* s_gameRegion;
extern MemoryRegion* s_levelRegion;
extern MemoryRegion* s_resRegion;	// Region for level-specific resources.

#define game_alloc(size) TFE_Memory::region_alloc(s_gameRegion, size)
#define game_realloc(ptr, size) TFE_Memory::region_realloc(s_gameRegion, ptr, size)
#define game_free(ptr) TFE_Memory::region_free(s_gameRegion, ptr)

#define level_alloc(size) TFE_Memory::region_alloc(s_levelRegion, size)
#define level_realloc(ptr, size) TFE_Memory::region_realloc(s_levelRegion, ptr, size)
#define level_free(ptr) TFE_Memory::region_free(s_levelRegion, ptr)

#define res_alloc(size) TFE_Memory::region_alloc(s_resRegion, size)
#define res_realloc(ptr, size) TFE_Memory::region_realloc(s_resRegion, ptr, size)
#define res_free(ptr) TFE_Memory::region_free(s_resRegion, ptr)

struct IGame
{
	virtual bool runGame(s32 argCount, const char* argv[]) = 0;
	virtual void exitGame() = 0;
	virtual void loopGame() {};

	GameID id;
};

IGame* createGame(GameID id);
void   freeGame(IGame* game);

void game_init();
void game_destroy();
