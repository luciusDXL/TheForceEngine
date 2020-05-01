#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Main
// Main entry point for the game itself.
// TODO: Split these per game, rather than assuming Dark Forces.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/levelAsset.h>
#include "gameLoop.h"

class TFE_Renderer;

namespace TFE_GameMain
{
	void init(TFE_Renderer* renderer);
	GameTransition loop(bool consoleOpen);
}
