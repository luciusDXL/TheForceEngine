#pragma once
//////////////////////////////////////////////////////////////////////
// GPU 2D antialiased line drawing
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "lineDrawMode.h"
#include "camera3d.h"

namespace TFE_RenderShared
{
	bool line3d_init();
	void line3d_destroy();

	void lineDraw3d_setLineDrawMode(LineDrawMode mode = LINE_DRAW_SOLID);

	void lineDraw3d_begin(s32 width, s32 height);
	void lineDraw3d_addLines(u32 count, f32 width, const Vec3f* lines, const u32* lineColors);
	void lineDraw3d_addLine(f32 width, const Vec3f* vertices, const u32* colors);

	void lineDraw3d_drawLines(const Camera3d* camera, bool depthTest, bool useBias);
}
