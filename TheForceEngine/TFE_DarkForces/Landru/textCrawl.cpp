#include "textCrawl.h"
#include "lcanvas.h"
#include "ldraw.h"
#include "lsystem.h"
#include "lview.h"
#include "ltimer.h"
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Game/igame.h>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	enum TextCrawlConst
	{
		TCRAWL_EYEALT = FIXED(60),
		TCRAWL_VIEWDIST = FIXED(210),
		TCRAWL_WIDTH = FIXED(2020),
		TCRAWL_STARTFADEDIST = FIXED(100),
		TCRAWL_YSTART = 30,
		TCRAWL_YEND = 199,
		TCRAWL_HEIGHT = (TCRAWL_YEND - TCRAWL_YSTART + 1),
		TCRAWL_XORIGIN = 160,
		TCRAWL_YORIGIN = (TCRAWL_YSTART - 1),
		TCRAWL_DARKEST = 224,
	};
	static const fixed16_16 TCRAWL_PIXELSIZE   = floatToFixed16(0.9f);
	static const fixed16_16 TCRAWL_FADEPERUNIT = floatToFixed16(0.1f);

	static Film* s_crawlFilm = nullptr;
	static u8* s_bitmap = nullptr;
	static s32 s_bitmapWidth = 0;
	static s32 s_bitmapHeight = 0;

	static fixed16_16 s_aRowOffset[TCRAWL_HEIGHT];
	static fixed16_16 s_aRowScale[TCRAWL_HEIGHT];
	static fixed16_16 s_aRowXStart[TCRAWL_HEIGHT];
	static s32 s_aRowStart[TCRAWL_HEIGHT];
	static s32 s_aRowWidth[TCRAWL_HEIGHT];
	static s32 s_aRowFade[TCRAWL_HEIGHT];

	JBool drawTextCrawl(LActor* actor, LRect* rect, LRect* clipRect, s16 xOffset, s16 yOffset, JBool refresh);
	void textCrawlBuildTables();

	void openTextCrawl(LActor* actor, Film* film)
	{
		lactor_setDrawFunc(actor, drawTextCrawl);
		s_crawlFilm = film;

		// Decompress the image into its own bitmap.
		s16* data16 = (s16*)actor->data;
		data16 += 4;

		s_bitmapWidth  = actor->w + 1;
		s_bitmapHeight = actor->h + 1;

		s_bitmap = (u8*)landru_alloc(s_bitmapWidth * s_bitmapHeight);
		memset(s_bitmap, 0, s_bitmapWidth * s_bitmapHeight);
		drawDeltaIntoBitmap(data16, 0, 0, s_bitmap, s_bitmapWidth);

		textCrawlBuildTables();
	}

	void closeTextCrawl(LActor* actor)
	{
		if (!s_bitmap) { return; }
		landru_free(s_bitmap);

		s_crawlFilm  = nullptr;
		s_bitmap = nullptr;
	}

	JBool drawTextCrawl(LActor* actor, LRect* rect, LRect* clipRect, s16 xOffset, s16 yOffset, JBool refresh)
	{
		fixed16_16 offset = floatToFixed16(0.25f) * (200 - actor->y) - intToFixed16(430);
		
		u8* outBitmap = ldraw_getBitmap();
		s32 stride = 320;

		u8* displayBase = &outBitmap[TCRAWL_YSTART * stride];
		for (s32 y = TCRAWL_YSTART, index = 0; y < TCRAWL_YEND; y++, index++, displayBase += stride)
		{
			s32 yOffset = round16(s_aRowOffset[index] + offset);
			if (yOffset >= 0 && yOffset < s_bitmapHeight)
			{
				const u8* tex = s_bitmap + (yOffset * s_bitmapWidth);
				fixed16_16 offset = s_aRowXStart[index];

				s32 xStart = s_aRowStart[index];
				s32 xEnd = xStart + s_aRowWidth[index] - 1;
				// Clip
				if (xStart < 0)
				{
					offset += mul16(s_aRowScale[index], intToFixed16(-xStart));
					xStart = 0;
				}
				if (xEnd >= stride)
				{
					xEnd = stride - 1;
				}

				// Draw the current scanline.
				u8* display = displayBase + xStart;
				for (s32 x = xStart; x <= xEnd; x++, display++)
				{
					const u8 color = tex[round16(offset)];
					if (color)
					{
						*display = max(TCRAWL_DARKEST, s32(color) - s_aRowFade[index]);
					}
					offset += s_aRowScale[index];
				}
			}
		}

		return JTRUE;
	}

	void textCrawlBuildTables()
	{
		const fixed16_16 numerator = mul16(TCRAWL_EYEALT, TCRAWL_VIEWDIST);

		// Precompute tables.
		s32 index = 0;
		for (s32 sy = TCRAWL_YSTART; sy <= TCRAWL_YEND; sy++, index++)
		{
			fixed16_16 z = div16(div16(numerator, intToFixed16(sy - TCRAWL_YORIGIN)), TCRAWL_PIXELSIZE);
			s_aRowOffset[index] = intToFixed16(s_bitmapHeight) - z;

			s32 xl = div16(mul16(-TCRAWL_WIDTH >> 1, TCRAWL_VIEWDIST), z);
			s32 xr = div16(mul16( TCRAWL_WIDTH >> 1, TCRAWL_VIEWDIST), z);
			s_aRowWidth[index] = round16(xr - xl);
			s_aRowStart[index] = round16(xl) + TCRAWL_XORIGIN;

			s_aRowScale[index] = div16(intToFixed16(s_bitmapWidth - 1), xr - xl + ONE_16);
			fixed16_16 xlRounded = intToFixed16(round16(xl));
			s_aRowXStart[index] = mul16(xlRounded - xl, s_aRowScale[index]);

			if (z > TCRAWL_STARTFADEDIST)
			{
				s_aRowFade[index] = round16(mul16(TCRAWL_FADEPERUNIT, z - TCRAWL_STARTFADEDIST));
			}
			else
			{
				s_aRowFade[index] = 0;
			}
		}
	}
}  // namespace TFE_DarkForces
