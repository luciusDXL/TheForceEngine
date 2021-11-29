#include "crawl.h"
#include "lcanvas.h"
#include "ldraw.h"
#include "lview.h"
#include "ltimer.h"
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Game/igame.h>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	static Film* s_crawlFilm = nullptr;
	static u8* s_crawlImage = nullptr;

	JBool drawCrawl(LActor* actor, LRect* rect, LRect* clipRect, s16 xOffset, s16 yOffset, JBool refresh);
	void crawlInit(u8* textBitmap, s32 textBitmapWidth, s32 textBitmapHeight);
	void crawlDisplay(fixed16_16 offset);

	void openCrawl(LActor* actor, Film* film)
	{
		lactor_setDrawFunc(actor, drawCrawl);
		s_crawlFilm = film;

		// Decompress the image into its own bitmap.
		s16* data16 = (s16*)actor->data;
		data16 += 4;

		s32 bitmapWidth  = actor->w + 1;
		s32 bitmapHeight = actor->h + 1;

		s_crawlImage = (u8*)game_alloc(bitmapWidth * bitmapHeight);
		memset(s_crawlImage, 0, bitmapWidth * bitmapHeight);
		drawDeltaIntoBitmap(data16, 0, 0, s_crawlImage, bitmapWidth);

		crawlInit(s_crawlImage, bitmapWidth, bitmapHeight);
	}

	void closeCrawl(LActor* actor)
	{
		if (!s_crawlImage) { return; }
		game_free(s_crawlImage);

		s_crawlFilm  = nullptr;
		s_crawlImage = nullptr;
	}

	JBool drawCrawl(LActor* actor, LRect* rect, LRect* clipRect, s16 xOffset, s16 yOffset, JBool refresh)
	{
		fixed16_16 offset = floatToFixed16(0.25f) * (200 - actor->y) - intToFixed16(430);
		crawlDisplay(offset);
		return JTRUE;
	}

	static u8* s_bitmap = nullptr;
	static s32 s_bitmapWidth = 0;
	static s32 s_bitmapHeight = 0;

	enum TextCrawlConst
	{
		TCRAWL_EYEALT   = FIXED(60),
		TCRAWL_VIEWDIST = FIXED(210),
		TCRAWL_RWWIDTH	= FIXED(2020),
		TCRAWL_STARTCUEDIST = FIXED(100),
		TCRAWL_YSTART   =  30,
		TCRAWL_YEND     = 199,
		TCRAWL_HEIGHT   = (TCRAWL_YEND - TCRAWL_YSTART + 1),
		TCRAWL_XORIGIN  = 160,
		TCRAWL_YORIGIN  = (TCRAWL_YSTART - 1),
		TCRAWL_BRIGHTEST = 254,
		TCRAWL_DARKEST   = 224,
	};
	static const fixed16_16 TCRAWL_PIXELSIZE  = floatToFixed16(0.9f);
	static const fixed16_16 TCRAWL_CUEPERUNIT = floatToFixed16(0.1f);

	fixed16_16 s_aRowOffset[TCRAWL_HEIGHT];
	fixed16_16 s_aRowScale[TCRAWL_HEIGHT];
	fixed16_16 s_aRowXStart[TCRAWL_HEIGHT];
	s32 s_aRowStart[TCRAWL_HEIGHT];
	s32 s_aRowWidth[TCRAWL_HEIGHT];
	s32 s_aRowCue[TCRAWL_HEIGHT];

	void crawlInit(u8* textBitmap, s32 textBitmapWidth, s32 textBitmapHeight)
	{
		s_bitmap = textBitmap;
		s_bitmapWidth = textBitmapWidth;
		s_bitmapHeight = textBitmapHeight;

		fixed16_16 numerator = mul16(TCRAWL_EYEALT, TCRAWL_VIEWDIST);

		// Precompute tables.
		s32 index = 0;
		for (s32 sy = TCRAWL_YSTART; sy <= TCRAWL_YEND; sy++, index++)
		{
			fixed16_16 z = div16(div16(numerator, intToFixed16(sy - TCRAWL_YORIGIN)), TCRAWL_PIXELSIZE);
			s_aRowOffset[index] = intToFixed16(s_bitmapHeight) - z;

			s32 xl = div16(mul16(-TCRAWL_RWWIDTH >> 1, TCRAWL_VIEWDIST), z);
			s32 xr = div16(mul16( TCRAWL_RWWIDTH >> 1, TCRAWL_VIEWDIST), z);
			s_aRowWidth[index] = round16(xr - xl);
			s_aRowStart[index] = round16(xl) + TCRAWL_XORIGIN;

			s_aRowScale[index] = div16(intToFixed16(s_bitmapWidth - 1), xr - xl + ONE_16);
			fixed16_16 xlRounded = intToFixed16(round16(xl));
			s_aRowXStart[index] = mul16(xlRounded - xl, s_aRowScale[index]);

			if (z > TCRAWL_STARTCUEDIST)
			{
				s_aRowCue[index] = round16(mul16(TCRAWL_CUEPERUNIT, z - TCRAWL_STARTCUEDIST));
			}
			else
			{
				s_aRowCue[index] = 0;
			}
		}
	}

	void crawlDisplay(fixed16_16 offset)
	{
		u8* outBitmap = ldraw_getBitmap();
		s32 stride = 320;

		u8* displayBase = &outBitmap[TCRAWL_YSTART * stride];
		for (s32 y = TCRAWL_YSTART, index = 0; y < TCRAWL_YEND; y++, index++, displayBase += stride)
		{
			s32 yOffset = round16(s_aRowOffset[index] + offset);
			if (yOffset >= 0 && yOffset <= s_bitmapHeight - 1)
			{
				u8* tex = s_bitmap + (yOffset * s_bitmapWidth);
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

				u8* display = displayBase + xStart;
				for (s32 x = xStart; x <= xEnd; x++, display++)
				{
					s32 texLookup = round16(offset);
					s32 color = tex[texLookup];
					if (color)
					{
						color -= s_aRowCue[index];
						if (color < TCRAWL_DARKEST)
						{
							color = TCRAWL_DARKEST;
						}

						*display = color;
					}
					offset += s_aRowScale[index];
				}
			}
		}
	}
}  // namespace TFE_DarkForces
