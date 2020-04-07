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

namespace TriColoredDraw2d
{
	bool init();
	void destroy();

	void begin(u32 width, u32 height, f32 pixelsToWorldUnits, f32 offsetX_WorldUnits, f32 offsetY_WorldUnits);
	void addTriangles(u32 count, const Vec2f* triVtx, const u32* triColors);
	void addTriangle(const Vec2f* triVtx, u32 triColor);

	void drawTriangles();
}
