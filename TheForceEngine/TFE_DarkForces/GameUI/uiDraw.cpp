#include <cstring>

#include "uiDraw.h"
#include <TFE_DarkForces/Landru/lfont.h>

namespace TFE_DarkForces
{
	// TODO: Factor out to the Landru system.
	DeltFrame s_cursor;

	void print(const char* text, s32 x0, s32 y0, u8 color, u8* framebuffer)
	{
		LRect frame;
		frame.left   = x0;
		frame.top    = y0;
		frame.right  = 320;
		frame.bottom = 200;

		lfont_setFrame(&frame);
		lfont_setColor(color);
		lfont_drawText(text, framebuffer);
	}

	s32 getStringPixelLength(const char* str)
	{
		if (!str) { return 0; }
		return lfont_getTextWidth(str);
	}

	void drawColoredQuad(s32 x0, s32 y0, s32 w, s32 h, u8 color, u8* framebuffer)
	{
		u8* output = &framebuffer[y0 * 320];
		s32 x1 = x0 + w - 1;
		s32 y1 = y0 + h - 1;

		// Skip if out of bounds.
		if (x0 >= 320 || x1 < 0 || y0 >= 200 || y1 < 0)
		{
			return;
		}
		// Clip
		if (x0 < 0)
		{
			x0 = 0;
		}
		if (x1 >= 320)
		{
			x1 = 319;
		}
		if (y0 < 0)
		{
			output -= y0 * 320;
			y0 = 0;
		}
		if (y1 >= 200)
		{
			y1 = 199;
		}

		s32 width = x1 - x0 + 1;
		for (s32 y = y0; y <= y1; y++)
		{
			memset(&output[x0], color, width);
			output += 320;
		}
	}

	void drawHorizontalLine(s32 x0, s32 x1, s32 y0, u8 color, u8* framebuffer)
	{
		// Skip if out of bounds.
		if (x0 >= 320 || x1 < 0 || y0 < 0 || y0 >= 200)
		{
			return;
		}

		if (x0 < 0) { x0 = 0; }
		if (x1 >= 320) { x1 = 319; }

		s32 width = x1 - x0 + 1;
		memset(&framebuffer[y0*320 + x0], color, width);
	}

	void drawVerticalLine(s32 y0, s32 y1, s32 x0, u8 color, u8* framebuffer)
	{
		// Skip if out of bounds.
		if (x0 >= 320 || x0 < 0 || y1 < 0 || y0 >= 200)
		{
			return;
		}

		if (y0 < 0) { y0 = 0; }
		if (y1 >= 320) { y1 = 319; }

		u8* output = &framebuffer[y0 * 320 + x0];
		for (s32 y = y0; y <= y1; y++, output += 320)
		{
			*output = color;
		}
	}
}