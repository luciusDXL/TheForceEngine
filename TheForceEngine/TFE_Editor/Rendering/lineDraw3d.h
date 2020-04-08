#pragma once
//////////////////////////////////////////////////////////////////////
// An OpenGL system to blit a texture to the screen. Uses a hardcoded
// basic shader + fullscreen triangle.
//
// Additional optional features can be added later such as filtering,
// bloom and GPU color conversion.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

namespace LineDraw3d
{
	bool init();
	void destroy();

	void addLines(u32 count, f32 width, const Vec3f* lines, const u32* lineColors);
	void addLine(f32 width, const Vec3f* vertices, const u32* colors);

	void draw(const Vec3f* camPos, const Mat3* viewMtx, const Mat4* projMtx, bool depthTest = true, bool useBias = true);
}
