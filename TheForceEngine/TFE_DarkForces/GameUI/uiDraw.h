#pragma once
//////////////////////////////////////////////////////////////////////
// Note this is still the original TFE code, reverse-engineered game
// UI code will not be available until future releases.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Level/rtexture.h>
#include "delt.h"

namespace TFE_DarkForces
{
	void createFont();

	void drawColoredQuad(s32 x0, s32 y0, s32 w, s32 h, u8 color, u8* framebuffer);
	void drawHorizontalLine(s32 x0, s32 x1, s32 y0, u8 color, u8* framebuffer);
	void drawVerticalLine(s32 y0, s32 y1, s32 x0, u8 color, u8* framebuffer);

	void print(const char* text, s32 x0, s32 y0, u8 color, u8* framebuffer);
	s32  getStringPixelLength(const char* str);

	extern DeltFrame s_cursor;
}
