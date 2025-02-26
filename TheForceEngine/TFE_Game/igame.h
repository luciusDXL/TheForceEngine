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

#define game_alloc(size) TFE_Memory::region_alloc(s_gameRegion, size)
#define game_realloc(ptr, size) TFE_Memory::region_realloc(s_gameRegion, ptr, size)
#define game_free(ptr) TFE_Memory::region_free(s_gameRegion, ptr)

#define level_alloc(size) TFE_Memory::region_alloc(s_levelRegion, size)
#define level_realloc(ptr, size) TFE_Memory::region_realloc(s_levelRegion, ptr, size)
#define level_free(ptr) TFE_Memory::region_free(s_levelRegion, ptr)

struct IGame
{
	virtual bool runGame(s32 argCount, const char* argv[], Stream* stream) = 0;
	virtual void exitGame() = 0;
	virtual void pauseGame(bool pause) = 0;
	virtual void pauseSound(bool pause) = 0;
	virtual void restartMusic() = 0;
	virtual void loopGame() {};
	virtual bool serializeGameState(Stream* stream, const char* filename, bool writeState) { return false; };
	virtual bool canSave() { return false; }
	virtual bool isPaused() { return false; }
	virtual void getLevelName(char* name) {};
	virtual void getLevelId(char* name) {};
	virtual void getModList(char* modList) {};

	GameID id;
};

IGame* createGame(GameID id);
void   freeGame(IGame* game);

void game_init();
void game_destroy();
