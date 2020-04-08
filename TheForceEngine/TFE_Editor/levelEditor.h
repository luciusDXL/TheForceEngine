#pragma once
#include <TFE_System/types.h>

class TFE_Renderer;
namespace LevelEditor
{
	void init(TFE_Renderer* renderer);
	void disable();

	void draw(bool* isActive);
	bool isFullscreen();

	void menu();

	// Returns true if a fullscreen blit is required.
	bool render3dView();
}
