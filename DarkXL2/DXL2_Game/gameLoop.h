#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Asset/levelAsset.h>
#include "gameState.h"
#include "view.h"

class DXL2_Renderer;
struct Model;
struct Palette256;
struct LevelObjectData;

struct StartLocation
{
	bool overrideStart;	// true to override the starting position.
	Vec2f pos;
	s32 layer;
};

namespace DXL2_GameLoop
{
	bool startLevel(LevelData* level, const StartLocation& start, DXL2_Renderer* renderer, s32 w=320, s32 h=200, bool enableViewStats=true);
	bool startLevelFromExisting(const Vec3f* startPos, f32 yaw, s32 startSectorId, const Palette256* pal, LevelObjectData* levelObj, DXL2_Renderer* renderer, s32 w=320, s32 h=200);
	void endLevel();

	GameTransition update(GameState* curState = nullptr, GameOverlay* curOverlay = nullptr);
	void draw();

	void startRenderer(DXL2_Renderer* renderer, s32 w, s32 h);
	void drawModel(const Model* model, const Vec3f* orientation);

	const ViewStats* getViewStats();
}
