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
	struct DrawRect
	{
		s32 x0;
		s32 y0;
		s32 x1;
		s32 y1;
	};

	void screen_drawPoint(ScreenRect* rect, s32 x, s32 z, u8 color, u8* framebuffer);
	void screen_drawLine(ScreenRect* rect, s32 x0, s32 z0, s32 x1, s32 z1, u8 color, u8* framebuffer);
	void screen_drawCircle(ScreenRect* rect, s32 x, s32 z, s32 r, s32 stepAngle, u8 color, u8* framebuffer);

	JBool screen_clipLineToRect(ScreenRect* rect, s32* x0, s32* z0, s32* x1, s32* z1);

	void blitTextureToScreen(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, u8* output);
	void blitTextureToScreenLit(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, const u8* atten, u8* output);
}