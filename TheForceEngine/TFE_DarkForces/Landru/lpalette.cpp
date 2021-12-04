#include "lpalette.h"
#include "lsystem.h"
#include "lview.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	// System palettes
	static LPalette* s_firstPal  = nullptr;
	static LPalette* s_workPal   = nullptr;
	static LPalette* s_screenPal = nullptr;
	static LPalette* s_srcPal    = nullptr;
	static LPalette* s_dstPal    = nullptr;
	static LPalette* s_videoPal  = nullptr;
	static JBool s_lpaletteInit  = JFALSE;

	void lpalette_setName(LPalette* pal, u32 resType, const char* name);
	void lpalette_restoreScreenPal(s16 low, s16 high);
	void lpalette_setVideoPal(LPalette* pal, s16 start, s16 length, s16 offset);
	void lpalette_setVgaPalette();
	void lpalette_setCycleRange(s16 start, s16 stop, s16 offset);

	void lpalette_init()
	{
		s_lpaletteInit = JTRUE;
		s_firstPal  = nullptr;
		s_screenPal = lpalette_alloc(0, MAX_COLORS);
		s_srcPal    = lpalette_alloc(0, MAX_COLORS);
		s_dstPal    = lpalette_alloc(0, MAX_COLORS);
		s_workPal   = lpalette_alloc(0, MAX_COLORS);
		s_videoPal  = lpalette_alloc(0, MAX_COLORS);
		lpalette_setRGB(s_videoPal, 0x3f, 0x3f, 0x3f, 0, MAX_COLORS);
	}

	void lpalette_destroy()
	{
		s_lpaletteInit = JFALSE;
		lpalette_free(s_screenPal);
		lpalette_free(s_srcPal);
		lpalette_free(s_dstPal);
		lpalette_free(s_workPal);
		lpalette_free(s_videoPal);

		s_firstPal  = nullptr;
		s_workPal   = nullptr;
		s_screenPal = nullptr;
		s_srcPal    = nullptr;
		s_dstPal    = nullptr;
		s_videoPal  = nullptr;
	}

	LPalette* lpalette_getList()
	{
		return s_firstPal;
	}

	void lpalette_add(LPalette* pal)
	{
		LPalette* curPal = s_firstPal;
		LPalette* lastPal = nullptr;
		while (curPal)
		{
			lastPal = curPal;
			curPal = curPal->next;
		}

		pal->next = curPal;
		if (!lastPal)
		{
			s_firstPal = pal;
		}
		else
		{
			lastPal->next = pal;
		}
	}

	void lpalette_remove(LPalette* pal)
	{
		LPalette* curPal = s_firstPal;
		LPalette* lastPal = nullptr;

		while (curPal && curPal != pal)
		{
			lastPal = curPal;
			curPal = curPal->next;
		}

		if (curPal == pal)
		{
			if (!lastPal)
			{
				s_firstPal = curPal->next;
			}
			else
			{
				lastPal->next = curPal->next;
			}
			pal->next = nullptr;
		}
	}

	void lpalette_setRGB(LPalette* pal, s16 r, s16 g, s16 b, s16 start, s16 length)
	{
		LRGB* rgbList = pal->colors;
		for (s32 slot = start; slot < start + length; slot++)
		{
			if (slot >= pal->start && slot <= pal->end)
			{
				rgbList[slot - pal->start].r = u8(r);
				rgbList[slot - pal->start].g = u8(g);
				rgbList[slot - pal->start].b = u8(b);
			}
		}
	}

	LPalette* lpalette_alloc(s16 start, s16 length)
	{
		if (!length) { return nullptr; }

		LPalette* pal = (LPalette*)landru_alloc(sizeof(LPalette));
		if (!pal) { return nullptr; }

		LRGB* rgb = (LRGB*)landru_alloc(sizeof(LRGB) * length);
		if (!rgb)
		{
			landru_free(pal);
			return nullptr;
		}
		memset(rgb, 0, sizeof(LRGB) * length);

		pal->next = nullptr;
		pal->colors = rgb;
		pal->start = start;
		pal->end = start + length - 1;
		pal->len = length;
		pal->cycle_count = 0;
		pal->cycle_active = 0;
		pal->varptr = nullptr;

		return pal;
	}

	void lpalette_free(LPalette* pal)
	{
		if (!pal) { return; }

		lpalette_remove(pal);
		if (pal->colors)
		{
			landru_free(pal->colors);
		}
		if (pal->varptr)
		{
			landru_free(pal->varptr);
		}
		landru_free(pal);
	}

	void lpalette_freeList(LPalette* pal)
	{
		while (pal)
		{
			LPalette* nextPal = pal->next;
			lpalette_remove(pal);
			lpalette_free(pal);
			pal = nextPal;
		}
	}

	LPalette* lpalette_load(const char* name)
	{
		char palName[32];
		sprintf(palName, "%s.PLTT", name);

		FilePath path;
		if (!TFE_Paths::getFilePath(palName, &path))
		{
			return nullptr;
		}

		FileStream file;
		if (!file.open(&path, FileStream::MODE_READ))
		{
			return nullptr;
		}

		u32 palSize = (u32)file.getSize();
		u8* data = (u8*)landru_alloc(palSize);
		file.readBuffer(data, palSize);
		file.close();

		s32 index = 0;
		JBool failed = JFALSE;

		s16 start = data[index]; index++;
		s16 stop = data[index]; index++;
		s16 count = stop - start + 1;
		LPalette* pal = lpalette_alloc(start, count);
		if (!pal)
		{
			landru_free(data);
			return nullptr;
		}

		LRGB* rgbList = pal->colors;
		for (s32 i = 0; i < count; i++)
		{
			rgbList[i].r = u8(data[index] >> 2); index++;
			rgbList[i].g = u8(data[index] >> 2); index++;
			rgbList[i].b = u8(data[index] >> 2); index++;
		}

		pal->cycle_count = data[index]; index++;
		if (pal->cycle_count > MAX_PALETTE_CYCLES) { pal->cycle_count = MAX_PALETTE_CYCLES; }

		if (pal->cycle_count)
		{
			for (s32 i = 0; i < (s32)pal->cycle_count; i++)
			{
				pal->cycles[i].rate = data[index]; index++;
				pal->cycles[i].rate += data[index] * MAX_COLORS; index++;
				pal->cycles[i].rate = (s16)MAX_CYCLE_RATE / pal->cycles[i].rate;

				s16 low = data[index]; index++;
				s16 high = data[index]; index++;

				if (low < high)
				{
					pal->cycles[i].low  = u8(low);
					pal->cycles[i].high = u8(high);
					pal->cycles[i].dir = 1;
				}
				else
				{
					pal->cycles[i].low  = u8(high);
					pal->cycles[i].high = u8(low);
					pal->cycles[i].dir = -1;
				}
				pal->cycles[i].active = 0;
			}
		}

		lpalette_setName(pal, PALETTE_TYPE, name);
		lpalette_add(pal);

		return pal;
	}

	void lpalette_startCycle(LPalette* pal)
	{
		if (!pal->cycle_active)
		{
			pal->cycle_active = 1;
			for (s32 i = 0; i < (s32)pal->cycle_count; i++)
			{
				pal->cycles[i].active = 1;
				pal->cycles[i].current = pal->cycles[i].low;
			}
		}
	}

	void lpalette_stopCycle(LPalette* pal)
	{
		if (pal->cycle_active)
		{
			pal->cycle_active = 0;
			for (s32 i = 0; i < (s32)pal->cycle_count; i++)
			{
				pal->cycles[i].active = 0;
				pal->cycles[i].current = pal->cycles[i].low;

				s16 low = (s16)pal->cycles[i].low;
				s16 high = (s16)pal->cycles[i].high;
				lpalette_restoreScreenPal(low, high);
			}
		}
	}

	void lpalette_cycleScreen()
	{
		u8 active = 0;
		LPalette* curPal = s_firstPal;
		while (curPal)
		{
			active |= curPal->cycle_active;
			curPal = curPal->next;
		}

		s16 min = 256;
		s16 max = -1;

		s32 time = lview_getTime();
		if (active)
		{
			curPal = s_firstPal;
			lpalette_copy(s_workPal, s_screenPal, 0, MAX_COLORS, 0);

			while (curPal)
			{
				if (curPal->cycle_active)
				{
					for (s32 i = 0; i < (s32)curPal->cycle_count; i++)
					{
						s16 low  = curPal->cycles[i].low;
						s16 high = curPal->cycles[i].high;
						s16 current = curPal->cycles[i].current;

						if (curPal->cycles[i].rate)
						{
							if ((time % curPal->cycles[i].rate) == 0)
							{
								if (curPal->cycles[i].dir < 0)
								{
									current--;
									if (current < low) { current = high; }
								}
								else  // dir > 0
								{
									current++;
									if (current > high) { current = low; }
								}
							}

							if (min > low)  min = low;
							if (max < high) max = high;
							lpalette_setCycleRange(low, high, current);
						}
					}
				}

				curPal = curPal->next;
			}

			if (min < max)
			{
				lpalette_setVideoPal(s_workPal, min, max - min + 1, min);
			}
		}
	}

	void lpalette_cyclesToStart()
	{
		LPalette* curPal = s_firstPal;
		while (curPal)
		{
			if (curPal->cycle_active)
			{
				for (s32 i = 0; i < (s32)curPal->cycle_count; i++)
				{
					s16 low  = curPal->cycles[i].low;
					s16 high = curPal->cycles[i].high;
					curPal->cycles[i].current = u8(low);
					lpalette_restoreScreenPal(low, high);
				}
			}

			curPal = curPal->next;
		}
	}

	void lpalette_stopAllCycles()
	{
		LPalette* curPal = s_firstPal;
		while (curPal)
		{
			curPal->cycle_active = 0;
			for (s32 i = 0; i < (s32)curPal->cycle_count; i++)
			{
				curPal->cycles[i].active = 0;
			}
			curPal = curPal->next;
		}

		lpalette_restoreScreenPal(0, 255);
	}

	JBool lpalette_compare(LPalette* dstPal, LPalette* srcPal, s16 start, s16 length, s16 offset)
	{
		const LRGB* src = srcPal->colors;
		const LRGB* dst = dstPal->colors;
		for (s32 srcSlot = start, dstSlot = offset; srcSlot < start + length; srcSlot++, dstSlot++)
		{
			if (srcSlot >= srcPal->start && srcSlot <= srcPal->end && dstSlot >= dstPal->start && dstSlot <= dstPal->end)
			{
				if (dst[dstSlot - dstPal->start].r != src[srcSlot - srcPal->start].r ||
					dst[dstSlot - dstPal->start].g != src[srcSlot - srcPal->start].g ||
					dst[dstSlot - dstPal->start].b != src[srcSlot - srcPal->start].b)
				{
					return JFALSE;
				}
			}
		}
		return JTRUE;
	}

	JBool lpalette_compareSrcDst()
	{
		return lpalette_compare(s_srcPal, s_dstPal, s_srcPal->start, s_srcPal->len, s_srcPal->start);
	}

	void lpalette_copyDstToScreen(s16 offset, s16 start, s16 stop)
	{
		lpalette_copy(s_screenPal, s_dstPal, start, stop - start + 1, offset);
	}

	void lpalette_putScreenPalRange(s16 start, s16 length)
	{
		lpalette_setVideoPal(s_screenPal, start, length, start);
	}
		
	void lpalette_putScreenPal()
	{
		lpalette_putScreenPalRange(0, MAX_COLORS);
	}

	void lpalette_setScreenPal(LPalette* pal)
	{
		lpalette_setVideoPal(pal, pal->start, pal->len, pal->start);
		lpalette_copy(s_screenPal, pal, pal->start, pal->len, pal->start);
	}

	void lpalette_setScreenRGB(s16 start, s16 stop, s16 r, s16 g, s16 b)
	{
		lpalette_setRGB(s_screenPal, r, g, b, start, stop - start + 1);
		lpalette_setVideoPal(s_screenPal, start, stop - start + 1, start);
	}

	void lpalette_setDstPal(LPalette* pal)
	{
		lpalette_copy(s_dstPal, pal, pal->start, pal->len, pal->start);
	}

	void lpalette_setSrcPal(LPalette* pal)
	{
		lpalette_copy(s_srcPal, pal, pal->start, pal->len, pal->start);
	}

	void lpalette_setDstColor(s16 start, s16 stop, s16 r, s16 g, s16 b)
	{
		lpalette_setRGB(s_dstPal, r, g, b, start, stop - start + 1);
	}

	void lpalette_setCycleRange(s16 start, s16 stop, s16 offset)
	{
		s16 vstart = start;
		s16 vlen = stop - offset + 1;
		s16 voff = offset;
		lpalette_copy(s_workPal, s_screenPal, vstart, vlen, voff);

		vstart += vlen;
		vlen = offset - start;
		voff = start;
		if (vlen)
		{
			lpalette_copy(s_workPal, s_screenPal, vstart, vlen, voff);
		}
	}
		
	void lpalette_copyScreenToDest(s16 offset, s16 start, s16 stop)
	{
		lpalette_copy(s_dstPal, s_screenPal, start, stop - start + 1, offset);
	}

	void lpalette_copyScreenToSrc(s16 offset, s16 start, s16 stop)
	{
		lpalette_copy(s_srcPal, s_screenPal, start, stop - start + 1, offset);
	}

	void lpalette_copy(LPalette* dstPal, LPalette* srcPal, s16 start, s16 length, s16 offset)
	{
		LRGB* src = srcPal->colors;
		LRGB* dst = dstPal->colors;

		s16 srcSlot, dstSlot, dir;
		if (start < offset)
		{
			srcSlot = start + length - 1;
			dstSlot = offset + length - 1;
			dir = -1;
		}
		else
		{
			srcSlot = start;
			dstSlot = offset;
			dir = 1;
		}

		while (srcSlot >= start && srcSlot < srcSlot + length)
		{
			if (srcSlot >= srcPal->start && srcSlot <= srcPal->end &&
				dstSlot >= dstPal->start && dstSlot <= dstPal->end)
			{
				dst[dstSlot - dstPal->start] = src[srcSlot - srcPal->start];
			}

			srcSlot += dir;
			dstSlot += dir;
		}
	}

	void lpalette_restoreScreenPal(s16 start, s16 stop)
	{
		lpalette_setVideoPal(s_screenPal, start, stop - start + 1, start);
	}

	void lpalette_setVideoPal(LPalette* pal, s16 start, s16 length, s16 offset)
	{
		LRGB* videoList = s_videoPal->colors;
		LRGB* rgbList = pal->colors;

		s16 palStart = pal->start;
		s16 palEnd = pal->end;
		LRGB* setList = nullptr;
		s16 setFirst = 0;
		s16 count = 0;

		for (s32 slot = start, vidSlot = offset; slot < start + length; slot++, vidSlot++)
		{
			if (slot >= palStart && slot <= palEnd)
			{
				// Always set it
				videoList[vidSlot].r = rgbList[slot - palStart].r;
				videoList[vidSlot].g = rgbList[slot - palStart].g;
				videoList[vidSlot].b = rgbList[slot - palStart].b;
				count++;
			}
		}
				
		if (count)
		{
			lpalette_setVgaPalette();
		}
	}

	void lpalette_buildPaletteToPalette(s16 start, s16 stop, s16 step)
	{
		LRGB* src1 = s_srcPal->colors;
		LRGB* src2 = s_dstPal->colors;
		LRGB* dst  = s_screenPal->colors;

		for (s32 slot = start; slot <= stop; slot++)
		{
			s32 r = s32(src2[slot].r) - s32(src1[slot].r);
			s32 g = s32(src2[slot].g) - s32(src1[slot].g);
			s32 b = s32(src2[slot].b) - s32(src1[slot].b);

			r = (r * step) / 256;
			g = (g * step) / 256;
			b = (b * step) / 256;

			dst[slot].r = u8(src1[slot].r + r);
			dst[slot].g = u8(src1[slot].g + g);
			dst[slot].b = u8(src1[slot].b + b);
		}
	}

	void lpalette_buildPaletteToMono(s16 start, s16 stop, s16 step, s16 fr, s16 fg, s16 fb)
	{
		LRGB* src = s_srcPal->colors;
		LRGB* dst = s_screenPal->colors;

		for (s32 slot = start; slot <= stop; slot++)
		{
			s32 r = s32(fr) - s32(src[slot].r);
			s32 g = s32(fg) - s32(src[slot].g);
			s32 b = s32(fb) - s32(src[slot].b);

			r = (r * step) / 256;
			g = (g * step) / 256;
			b = (b * step) / 256;

			dst[slot].r = u8(s32(src[slot].r) + r);
			dst[slot].g = u8(s32(src[slot].g) + g);
			dst[slot].b = u8(s32(src[slot].b) + b);
		}
	}

	void lpalette_buildMonoToPalette(s16 start, s16 stop, s16 step, s16 fr, s16 fg, s16 fb)
	{
		LRGB* src = s_dstPal->colors;
		LRGB* dst = s_screenPal->colors;

		for (s32 slot = start; slot <= stop; slot++)
		{
			s32 r = s32(src[slot].r) - s32(fr);
			s32 g = s32(src[slot].g) - s32(fg);
			s32 b = s32(src[slot].b) - s32(fb);

			r = (r * step) / 256;
			g = (g * step) / 256;
			b = (b * step) / 256;

			dst[slot].r = u8(s32(fr) + r);
			dst[slot].g = u8(s32(fg) + g);
			dst[slot].b = u8(s32(fb) + b);
		}
	}

	void lpalette_buildFadePalette(s16 type, s16 start, s16 stop, s16 step, s16 fr, s16 fg, s16 fb)
	{
		switch (type)
		{
			case LPAL_BUILD_PALETTE:
				lpalette_buildPaletteToPalette(start, stop, step);
				break;
			case LPAL_BUILD_MONOSRC:
				lpalette_buildMonoToPalette(start, stop, step, fr, fg, fb);
				break;
			case LPAL_BUILD_MONODEST:
				lpalette_buildPaletteToMono(start, stop, step, fr, fg, fb);
				break;
		}
	}

#define _CONV_6bitTo8bit(x) (((x)<<2) | ((x)>>4))

	void lpalette_setVgaPalette()
	{
		u32 palette[256];
		u32* outColor = palette;
		LRGB* srcColor = s_videoPal->colors;
		for (s32 i = 0; i < 256; i++, outColor++, srcColor++)
		{
			*outColor = _CONV_6bitTo8bit(srcColor->r) | (_CONV_6bitTo8bit(srcColor->g) << 8u) | (_CONV_6bitTo8bit(srcColor->b) << 16u) | (0xffu << 24u);
		}
		vfb_setPalette(palette);
		vfb_setPalette(palette);
	}

	void lpalette_setName(LPalette* pal, u32 resType, const char* name)
	{
		pal->resType = resType;
		strcpy(pal->name, name);
		_strlwr(pal->name);
	}
}