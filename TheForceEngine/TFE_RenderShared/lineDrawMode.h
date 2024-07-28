#pragma once
//////////////////////////////////////////////////////////////////////
// GPU 2D antialiased line drawing
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

namespace TFE_RenderShared
{
	#define MAX_LINE_DRAW_CALLS 64

	enum LineDrawMode
	{
		LINE_DRAW_SOLID = 0,
		LINE_DRAW_DASHED,
		LINE_DRAW_CURVE_DASHED,
		LINE_DRAW_COUNT
	};

	struct LineDraw
	{
		LineDrawMode mode;
		s32 offset;
		s32 count;
	};

	extern LineDraw s_lineDraw[];
	extern s32 s_lineDrawCount;
}