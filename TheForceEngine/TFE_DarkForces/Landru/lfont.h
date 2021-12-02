#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru Font
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "lrect.h"

namespace TFE_DarkForces
{
	void lfont_init();
	void lfont_destroy();

	JBool lfont_load(const char* name, s32 id);

	s16 lfont_set(s16 id);
	s16 lfont_getCurrent();
	u8  lfont_setColor(u8 color);
	u8  lfont_getColor();

	void lfont_setFrame(LRect* frame);
	void lfont_getFrame(LRect* frame);
	void lfont_setPosition(s16 x, s16 y);
	void lfont_getPosition(s16* x, s16* y);
	void lfont_movePosition(s16 dx, s16 dy);
	void lfont_returnPosition(s16 dy);
	void lfont_homePosition();
	s16  lfont_getTextWidth(const char* text);

	void lfont_drawText(const char* text, u8* bitmap = nullptr);
	JBool lfont_drawTextClipped(const char* text);
}  // namespace TFE_DarkForces