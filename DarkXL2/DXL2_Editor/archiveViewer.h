#pragma once
#include <DXL2_System/types.h>
#include <DXL2_Renderer/renderer.h>

namespace ArchiveViewer
{
	void init(DXL2_Renderer* renderer);
	void draw(bool* isActive);
	bool isFullscreen();

	// Returns true if a fullscreen blit is required.
	bool render3dView();
}
