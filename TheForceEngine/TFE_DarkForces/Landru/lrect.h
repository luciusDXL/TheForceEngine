#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru Actor
// Landru was the cutscene system developed for XWing and Tie Fighter
// and is also used in Dark Forces.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

namespace TFE_DarkForces
{
	enum LAlignType
	{
		LORIGIN = 0,
		LCENTER,
		LEXTENT
	};

	struct LRect
	{
		s16 top;
		s16 left;
		s16 bottom;
		s16 right;
	};

	void lrect_clear(LRect* rect);
	void lrect_set(LRect* rect, s16 left, s16 top, s16 right, s16 bottom);
	void lrect_setMax(LRect* rect);
	void lrect_offset(LRect* rect, s16 dx, s16 dy);
	void lrect_origin(LRect* rect);
	void lrect_align(LRect* rect, LRect* parent, LAlignType xAlign, LAlignType yAlign);
	void lrect_flip(LRect* rect, LRect* parent, JBool xFlip, JBool yFlip);
	void lrect_inset(LRect* rect, s16 dx, s16 dy);
	void lrect_enclose(LRect* r1, LRect* r2);
	void lrect_clipPoint(LRect* rect, s16* x, s16* y);

	JBool lrect_equal(LRect* r1, LRect* r2);
	JBool lrect_isEmpty(LRect* rect);
	JBool lrect_intersect(LRect* r1, LRect* r2);
	JBool lrect_clip(LRect* r1, LRect* r2);
	JBool lrect_isPointInside(LRect* rect, s16 x, s16 y);
}  // namespace TFE_DarkForces