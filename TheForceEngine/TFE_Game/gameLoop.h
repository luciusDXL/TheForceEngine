#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/levelAsset.h>
#include "gameState.h"
#include "view.h"

class TFE_Renderer;
struct Model;
struct Palette256;
struct LevelObjectData;

struct StartLocation
{
	bool overrideStart;	// true to override the starting position.
	Vec2f pos;
	s32 layer;
};

namespace TFE_GameLoop
{
	bool startLevel(LevelData* level, const StartLocation& start, TFE_Renderer* renderer, s32 w=320, s32 h=200, bool enableViewStats=true);
	bool startLevelFromExisting(const Vec3f* startPos, f32 yaw, s32 startSectorId, const Palette256* pal, LevelObjectData* levelObj, TFE_Renderer* renderer, s32 w=320, s32 h=200);
	void endLevel();

	void changeResolution(s32 width, s32 height);

	GameTransition update(bool consoleOpen, GameState curState = GAME_LEVEL, GameOverlay curOverlay = OVERLAY_NONE);
	void draw();

	void startRenderer(TFE_Renderer* renderer, s32 w, s32 h);
	void drawModel(const Model* model, const Vec3f* orientation);

	const ViewStats* getViewStats();
}
