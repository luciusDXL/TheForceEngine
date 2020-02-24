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

namespace TriTexturedDraw2d
{
	bool init();
	void destroy();

	void begin(u32 width, u32 height, f32 pixelsToWorldUnits, f32 offsetX_WorldUnits, f32 offsetY_WorldUnits);
	void addTriangles(u32 count, const Vec2f* triVtx, const Vec2f* triUv, const u32* triColors, const TextureGpu* texture);
	void addTriangle(const Vec2f* triVtx, const Vec2f* triUv, u32 triColor, const TextureGpu* texture);

	void drawTriangles();
}
