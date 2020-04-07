#pragma once
//////////////////////////////////////////////////////////////////////
// An OpenGL system to blit a texture to the screen. Uses a hardcoded
// basic shader + fullscreen triangle.
//
// Additional optional features can be added later such as filtering,
// bloom and GPU color conversion.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>

namespace LineDraw2d
{
	bool init();
	void destroy();

	void begin(u32 width, u32 height, f32 pixelsToWorldUnits, f32 offsetX_WorldUnits, f32 offsetY_WorldUnits);
	void addLines(u32 count, f32 width, const Vec2f* lines, const u32* lineColors);
	void addLine(f32 width, const Vec2f* vertices, const u32* colors);

	void drawLines();
}
