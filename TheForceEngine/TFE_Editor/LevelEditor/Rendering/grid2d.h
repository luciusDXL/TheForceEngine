#pragma once
//////////////////////////////////////////////////////////////////////
// An OpenGL system to blit a texture to the screen. Uses a hardcoded
// basic shader + fullscreen triangle.
//
// Additional optional features can be added later such as filtering,
// bloom and GPU color conversion.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/textureGpu.h>

namespace LevelEditor
{
	bool grid2d_init();
	void grid2d_destroy();

	void grid2d_computeScale(Vec2i viewportSize, f32 baseGridScale, f32 pixelsToWorldUnits, Vec3f viewPos);
	void grid2d_snap(const Vec2f& worldPos, s32 layer, Vec2f& snappedPos);
	void grid2d_blitToScreen(f32 gridOpacity);

	f32  grid2d_getGrid(s32 layer);
}
