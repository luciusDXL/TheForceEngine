#pragma once
//////////////////////////////////////////////////////////////////////
// Screen Line Drawing
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

struct ScreenRect;

namespace TFE_Jedi
{
	void screen_drawPoint(ScreenRect* rect, s32 x, s32 z, u8 color, u8* framebuffer);
	void screen_drawLine(ScreenRect* rect, s32 x0, s32 z0, s32 x1, s32 z1, u8 color, u8* framebuffer);
	void screen_drawCircle(ScreenRect* rect, s32 x, s32 z, s32 r, s32 stepAngle, u8 color, u8* framebuffer);

	JBool screen_clipLineToRect(ScreenRect* rect, s32* x0, s32* z0, s32* x1, s32* z1);
}
