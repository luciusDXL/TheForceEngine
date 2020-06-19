#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Level Objects
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "logicTypes.h"
#include "vueAsset.h"
#include <string>
#include <vector>

typedef std::vector<std::string> StringList;

enum LogicCommonFlags
{
	LCF_EYE   = (1 << 0),
	LCF_BOSS  = (1 << 1),
	LCF_PAUSE = (1 << 2),
	LCF_LOOP  = (1 << 3),
};

enum Difficulty
{
	DIFF_EASY = (1 << 0),
	DIFF_MEDIUM = (1 << 1),
	DIFF_HARD = (1 << 2),
};

// Generates enemies once its sector is activated ("MASTER: ON" message). Continues until the numTerminate is reached or "MASTER: OFF" message is sent.
// Will not generate enemies when the player can see the generator.
struct EnemyGenerator
{
	LogicType type;		// type of enemy to be generated.
	f32 delay;			// time in seconds that needs to pass from the start of a level before the generator starts operating.
	f32 interval;		// time in seconds between each generation.
	f32 minDist;		// min/max distance from player when enemy can be generated.
	f32 maxDist;
	s32 maxAlive;		// maximum number of enemies from the generator allowed alive at the same time.
	s32 numTerminate;	// number of enemies to generate after which the generator will terminate. -1 = infinite spawns.
	f32 wanderTime;		// time in seconds that a generate enemy walks around before becoming inactive.
};
// For Scripts
typedef EnemyGenerator GenParam;

struct Logic
{
	LogicType type = LOGIC_INVALID;
	u32 flags      = 0;		// logic specific flags.
	f32 frameRate  = 0.0f;
	Vec3f rotation = { 0.0f, 0.0f, 0.0f };

	VueAsset* vue = nullptr;
	VueAsset* vueAppend = nullptr;
	s32 vueId = -1;
	s32 vueAppendId = -1;
};
// For scripts
typedef Logic LogicParam;

struct LevelObject
{
	// Render Data
	ObjectClass oclass;
	u32	dataOffset;
	Vec3f pos;
	Vec3f orientation;
	s32 difficulty;

	// Common Variables.
	u32 comFlags = 0;		// common flags - see LogicCommonFlags.
	f32 radius = -1.0f;
	f32 height = 0.0f;

	// Logics
	std::vector<Logic> logics;
	std::vector<EnemyGenerator> generators;
};

struct LevelObjectData
{
	StringList pods;
	StringList sprites;
	StringList frames;
	StringList sounds;

	u32 objectCount;
	std::vector<LevelObject> objects;
};

namespace TFE_LevelObjects
{
	bool load(const char* name);
	void unload();

	void save(const char* name, const char* path);

	LevelObjectData* getLevelObjectData();
};
