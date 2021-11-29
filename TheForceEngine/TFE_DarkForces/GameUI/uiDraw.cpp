#include <cstring>

#include "uiDraw.h"
#include <TFE_Asset/fontAsset.h>

namespace TFE_DarkForces
{
	// This is pretty hacky, but only until actual UI reverse-engineering is done.
	static FontTFE* s_font = nullptr;
	DeltFrame s_cursor;

	void createFont()
	{
		s_font = TFE_Font::createSystemFont6x8();
	}

	void print(const char* text, s32 x0, s32 y0, u8 color, u8* framebuffer)
	{
		if (!text || !s_font) { return; }

		const size_t len = strlen(text);
		s32 dx = s_font->maxWidth;
		s32 h = s_font->height;
		s32 y1 = y0 + s_font->height - 1;

		for (size_t i = 0; i < len; i++, text++)
		{
			const char c = *text;
			if (c < s_font->startChar || c > s_font->endChar) { x0 += dx;  continue; }

			const s32 index = s32(c) - s_font->startChar;
			const s32 w = s_font->width[index];
			const u8* image = s_font->imageData + s_font->imageOffset[index];
			u8* output = &framebuffer[y0 * 320];
			
			s32 x1 = x0 + w - 1;
			s32 yOffset = h - 1;
			for (s32 y = y0; y <= y1; y++, yOffset--)
			{
				for (s32 x = x0, offset = yOffset; x <= x1; x++, offset += h)
				{
					if (image[offset])
					{
						output[x] = color;
					}
				}
				output += 320;
			}

			x0 += s_font->step[index];
		}
	}

	s32 getStringPixelLength(const char* str)
	{
		if (!str || !s_font) { return 0; }
		const s32 len = (s32)strlen(str);
		if (len < 1) { return 0; }

		s32 pixelLen = 0;
		s32 dx = s_font->maxWidth;
		for (s32 i = 0; i < len; i++, str++)
		{
			const char c = *str;
			if (c < s_font->startChar || c > s_font->endChar) { pixelLen += dx;  continue; }

			const s32 index = s32(c) - s_font->startChar;
			pixelLen += s_font->step[index];
		}
		return pixelLen;
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