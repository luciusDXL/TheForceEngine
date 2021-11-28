#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru Palette
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "lrect.h"

namespace TFE_DarkForces
{
	struct LRGB
	{
		u8 r, g, b;
	};

	struct Cycle
	{
		s16 dir;
		s16 rate;
		u8 active;
		u8 current;
		u8 low;
		u8 high;
	};

	enum LPaletteConstants : u32
	{
		PALETTE_TYPE = 0x504c5454,		// 'PLTT'
		MAX_PALETTE_CYCLES = 4,
		MAX_CYCLE_RATE = 4915,
		MAX_COLORS = 256,
		PALETTE_KEEPABLE = 0x0040,
		PALETTE_KEEP = 0x0080,

		LPAL_BUILD_PALETTE  = 0,
		LPAL_BUILD_MONOSRC  = 1,
		LPAL_BUILD_MONODEST = 2,
	};

	struct LPalette
	{
		u32 resType;
		char name[16];
		LPalette* next;

		LRGB* colors;

		Cycle cycles[MAX_PALETTE_CYCLES];
		u8 cycle_count;
		u8 cycle_active;

		s16 start;
		s16 end;
		s16 len;

		u16 flags;
		u8* varptr;
	};

	void lpalette_init();
	void lpalette_destroy();

	LPalette* lpalette_load(const char* name);
	LPalette* lpalette_alloc(s16 start, s16 length);
	void lpalette_free(LPalette* pal);
	void lpalette_freeList(LPalette* pal);

	LPalette* lpalette_getList();
	void lpalette_add(LPalette* pal);
	void lpalette_remove(LPalette* pal);
	void lpalette_copy(LPalette* dst, LPalette* src, s16 start, s16 length, s16 offset);
	void lpalette_copyScreenToDest(s16 offset, s16 start, s16 stop);
	void lpalette_copyScreenToSrc(s16 offset, s16 start, s16 stop);
	void lpalette_copyDstToScreen(s16 offset, s16 start, s16 stop);

	JBool lpalette_compare(LPalette* dstPal, LPalette* srcPal, s16 start, s16 length, s16 offset);
	JBool lpalette_compareSrcDst();
	
	void lpalette_setRGB(LPalette* pal, s16 r, s16 g, s16 b, s16 start, s16 length);
	void lpalette_putScreenPalRange(s16 start, s16 length);
	void lpalette_putScreenPal();

	void lpalette_cycleScreen();
	void lpalette_cyclesToStart();
	void lpalette_stopAllCycles();
	void lpalette_setScreenPal(LPalette* pal);
	void lpalette_setScreenRGB(s16 start, s16 stop, s16 r, s16 g, s16 b);
	void lpalette_setDstPal(LPalette* pal);
	void lpalette_setSrcPal(LPalette* pal);
	void lpalette_setDstColor(s16 start, s16 stop, s16 r, s16 g, s16 b);

	void lpalette_buildFadePalette(s16 type, s16 start, s16 stop, s16 step, s16 fr, s16 fg, s16 fb);
}  // namespace TFE_DarkForces