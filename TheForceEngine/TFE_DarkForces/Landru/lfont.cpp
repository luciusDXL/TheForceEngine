#include "lfont.h"
#include "lcanvas.h"
#include "ldraw.h"
#include "lpalette.h"
#include "lsystem.h"
#include "ltimer.h"
#include <TFE_System/endian.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <cassert>
#include <cstring>
#include <map>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	enum
	{
		MAX_FONT_HEIGHT		= 22,
		FONT_SPACE_BETWEEN	= 1,
		FONT_LINES_BETWEEN	= 1,
		FONT_INDENT			= 16,
		NUM_FONTS = 6,
	};

	struct LFont
	{
		u16 firstChar;
		u16 numChars;

		u16 spaceBetween;
		u16 linesBetween;
		u16 indent;

		u16 width;
		u16 height;
		u16 baseline;

		u16 isColor;

		u16 charSize;

		u8* widthArray;
		u8* data;

		s16 unused[2];
	};

	static LFont* s_fonts[NUM_FONTS];
	static LFont* s_curFont;
	static LRect  s_fontFrame = { 0 };
	static s16 s_curFontId;
	static s16 s_fontX, s_fontY;
	static u8 s_fontColor;

	void lfont_initFont(LFont* font);
	JBool lfont_add(s16 id, LFont* font);
	void lfont_remove(s16 id);

	void lfont_init()
	{
		for (s32 i = 0; i < NUM_FONTS; i++)
		{
			s_fonts[i] = nullptr;
		}

		s_curFont = nullptr;
		s_curFontId = -1;
		s_fontX = 0;
		s_fontY = 0;

		s_fontColor = 15;
	}

	void lfont_destroy()
	{
		for (s32 i = 0; i < NUM_FONTS; i++)
		{
			if (s_fonts[i])
			{
				lfont_remove(i);
			}
		}
	}

	LFont* lfont_alloc(s16 id)
	{
		LFont* font = (LFont*)landru_alloc(sizeof(LFont));
		if (font)
		{
			lfont_initFont(font);
			lfont_add(id, font);
		}
		return font;
	}

	void lfont_free(LFont* font)
	{
		if (!font) { return; }

		if (font->widthArray)
		{
			landru_free(font->widthArray);
		}
		if (font->data)
		{
			landru_free(font->data);
		}
		landru_free(font);
	}

	void lfont_initFont(LFont* font)
	{
		font->firstChar = ' ';
		font->numChars = 0;
		font->spaceBetween = FONT_SPACE_BETWEEN;
		font->linesBetween = FONT_LINES_BETWEEN;
		font->indent = FONT_INDENT;

		font->width = 0;
		font->height = 0;
		font->baseline = 0;
		font->charSize = 0;
		font->widthArray = nullptr;
		font->data = nullptr;
	}
		
	JBool lfont_add(s16 id, LFont* font)
	{
		if (id < 0 || id >= NUM_FONTS) { return JFALSE; }
		if (s_fonts[id])
		{
			lfont_remove(id);
		}
		s_fonts[id] = font;
		return JTRUE;
	}

	void lfont_remove(s16 id)
	{
		if (s_fonts[id])
		{
			lfont_free(s_fonts[id]);
			s_fonts[id] = nullptr;
		}
	}

	JBool lfont_load(const char* name, s32 id)
	{
		char fontFile[32];
		sprintf(fontFile, "%s.FONT", name);
		FilePath fontPath;
		FileStream file;
		if (!TFE_Paths::getFilePath(fontFile, &fontPath)) { return JFALSE; }
		if (!file.open(&fontPath, Stream::MODE_READ)) { return JFALSE; }

		LFont* font = lfont_alloc(id);
		file.read(&font->firstChar);
		file.read(&font->numChars);
		file.read(&font->width);
		file.read(&font->height);
		font->firstChar = TFE_Endian::swapLE16(font->firstChar);
		font->numChars = TFE_Endian::swapLE16(font->numChars);
		font->width = TFE_Endian::swapLE16(font->width);
		font->height = TFE_Endian::swapLE16(font->height);
		font->charSize = (font->width >> 3) * font->height;

		if (font->height > MAX_FONT_HEIGHT)
		{
			file.close();
			lfont_remove(id);
			return JFALSE;
		}

		file.read(&font->baseline);
		file.read(&font->isColor);
		font->baseline = TFE_Endian::swapLE16(font->baseline);
		font->isColor = TFE_Endian::swapLE16(font->isColor);
		
		font->widthArray = (u8*)landru_alloc(256);
		memset(font->widthArray, 0, 256);
		if (font->widthArray)
		{
			file.readBuffer(font->widthArray + font->firstChar, font->numChars);
		}

		u32 fontSize = font->charSize * font->numChars;
		font->data = (u8*)landru_alloc(fontSize);
		file.readBuffer(font->data, fontSize);

		file.close();
		return JTRUE;
	}

	s16 lfont_set(s16 id)
	{
		s16 lastFont = s_curFontId;
		if (id >= 0 && id < NUM_FONTS)
		{
			s_curFont = s_fonts[id];
			s_curFontId = id;
		}
		else
		{
			s_curFont = nullptr;
			s_curFontId = -1;
		}
		return lastFont;
	}

	s16 lfont_getCurrent()
	{
		return s_curFontId;
	}

	u8 lfont_setColor(u8 color)
	{
		u8 lastColor = s_fontColor;
		s_fontColor = color;
		return lastColor;
	}

	u8 lfont_getColor()
	{
		return s_fontColor;
	}

	void lfont_setFrame(LRect* frame)
	{
		s_fontFrame = *frame;
		lfont_homePosition();
	}

	void lfont_getFrame(LRect* frame)
	{
		*frame = s_fontFrame;
	}

	void lfont_setPosition(s16 x, s16 y)
	{
		s_fontX = x;
		s_fontY = y;
		s_fontFrame.left = x;
		s_fontFrame.top = y;
	}

	void lfont_getPosition(s16* x, s16* y)
	{
		*x = s_fontX;
		*y = s_fontY;
	}

	void lfont_movePosition(s16 dx, s16 dy)
	{
		s_fontX += dx;
		s_fontY += dy;
	}

	void lfont_returnPosition(s16 dy)
	{
		s_fontX = s_fontFrame.left;
		s_fontY += dy;
	}

	void lfont_homePosition()
	{
		s_fontX = s_fontFrame.left;
		s_fontY = s_fontFrame.top;
	}

	s16 lfont_getTextWidth(const char* text)
	{
		if (!s_curFont) { return 0; }

		u8* widthArray = s_curFont->widthArray;
		s16 len = 0;
		for (; *text; text++)
		{
			u8 value = u8(text[0]);
			len += widthArray[value];
			if (text[1])
			{
				len += s_curFont->spaceBetween;
			}
		}

		return len;
	}

	void lfont_getRect(const char* text, LRect* rect)
	{
		if (s_curFont)
		{
			s16 w = lfont_getTextWidth(text);
			lrect_set(rect, 0, 0, w, s_curFont->height);
		}
		else
		{
			lrect_clear(rect);
		}
	}

	void lfont_drawText(const char* text, u8* bitmap/* = nullptr*/)
	{
		if (!s_curFont || !text) { return; }

		u8* widthArray = s_curFont->widthArray;
		u8* data = s_curFont->data;
		if (!bitmap) { bitmap = ldraw_getBitmap();	}
		
		const s16 stepY = (s_curFont->width >> 3);
		const s16 step = s_curFont->charSize;
		s16 xGlyph = s_fontX;
		for (; *text; text++)
		{
			u8 value = u8(text[0]);
			u8* curGlyph = &data[(value - s_curFont->firstChar) * step];
			for (s32 y = 0; y < s_curFont->height; y++, curGlyph += stepY)
			{
				u8* output = &bitmap[(y + s_fontY) * 320 + xGlyph];
				for (s32 x = 0; x < s_curFont->width; x++)
				{
					u32 mask = 1 << ((7-x) & 7);
					if (curGlyph[x >> 3] & mask)
					{
						output[x] = s_fontColor;
					}
				}
			}

			xGlyph += widthArray[value];
			if (text[1])
			{
				xGlyph += s_curFont->spaceBetween;
			}
		}
	}

	JBool lfont_drawTextClipped(const char* text)
	{
		LRect textRect;
		lfont_getRect(text, &textRect);
		lrect_offset(&textRect, s_fontX, s_fontY);

		JBool adjust   = JFALSE;
		JBool retValue = JFALSE;
		LRect clipRect = textRect;
		if (lcanvas_clipRectToCanvas(&clipRect))
		{
			if (!lrect_equal(&textRect, &clipRect))
			{
				// TODO
			}
			else
			{
				lfont_drawText(text);
				retValue = JTRUE;
			}
		}
		else
		{
			adjust = JTRUE;
		}

		if (adjust)
		{
			s_fontX += textRect.right - textRect.left;
		}

		return retValue;
	}
}  // namespace TFE_DarkForces