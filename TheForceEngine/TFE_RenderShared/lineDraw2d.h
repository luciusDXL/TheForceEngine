#pragma once
//////////////////////////////////////////////////////////////////////
// GPU 2D antialiased line drawing
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "lineDrawMode.h"

namespace TFE_RenderShared
{
	bool init(bool allowBloom = true);
	void destroy();

	void lineDraw2d_setLineDrawMode(LineDrawMode mode = LINE_DRAW_SOLID);

	void lineDraw2d_begin(u32 width, u32 height);
	void lineDraw2d_addLines(u32 count, f32 width, const Vec2f* lines, const u32* lineColors);
	void lineDraw2d_addLine(f32 width, const Vec2f* vertices, const u32* colors);

	void lineDraw2d_drawLines();
}
