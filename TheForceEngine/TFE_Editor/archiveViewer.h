#pragma once
#include <TFE_System/types.h>
#include <TFE_Renderer/renderer.h>

namespace ArchiveViewer
{
	void init(TFE_Renderer* renderer);
	void draw(bool* isActive);
	bool isFullscreen();

	// Returns true if a fullscreen blit is required.
	bool render3dView();
}
