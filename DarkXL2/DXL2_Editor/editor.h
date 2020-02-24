#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Renderer/renderer.h>

namespace DXL2_Editor
{
	void enable(DXL2_Renderer* renderer);
	void disable();
	void update();
	bool render();

	void showPerf(u32 frame);
}
