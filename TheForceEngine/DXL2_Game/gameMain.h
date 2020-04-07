#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Main
// Main entry point for the game itself.
// TODO: Split these per game, rather than assuming Dark Forces.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Asset/levelAsset.h>
#include "gameLoop.h"

class DXL2_Renderer;

namespace DXL2_GameMain
{
	void init(DXL2_Renderer* renderer);
	GameTransition loop();
}
