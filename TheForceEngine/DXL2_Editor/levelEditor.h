#pragma once
#include <DXL2_System/types.h>

class DXL2_Renderer;
namespace LevelEditor
{
	void init(DXL2_Renderer* renderer);
	void disable();

	void draw(bool* isActive);
	bool isFullscreen();

	void menu();

	// Returns true if a fullscreen blit is required.
	bool render3dView();
}
