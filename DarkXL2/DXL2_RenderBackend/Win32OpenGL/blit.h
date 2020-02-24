#pragma once
//////////////////////////////////////////////////////////////////////
// An OpenGL system to blit a texture to the screen. Uses a hardcoded
// basic shader + fullscreen triangle.
//
// Additional optional features can be added later such as filtering,
// bloom and GPU color conversion.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_RenderBackend/textureGpu.h>

namespace BlitOpenGL
{
	bool init();
	void destroy();

	void blitToScreen(TextureGpu* texture, s32 x, s32 y, s32 width, s32 height);
}
