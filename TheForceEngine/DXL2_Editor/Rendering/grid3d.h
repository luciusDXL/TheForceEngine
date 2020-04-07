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

namespace Grid3d
{
	bool init();
	void destroy();

	void draw(f32 gridScale, f32 subGridSize, f32 gridOpacity, f32 pixelSize, const Vec3f* camPos, const Mat3* viewMtx, const Mat4* projMtx);
}
