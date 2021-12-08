#include "lrect.h"
#include <TFE_Jedi/Math/core_math.h>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	void lrect_clear(LRect* rect)
	{
		rect->left  = 0;
		rect->top   = 0;
		rect->right = 0;
		rect->bottom = 0;
	}

	void lrect_set(LRect* rect, s16 left, s16 top, s16 right, s16 bottom)
	{
		rect->left = left;
		rect->top = top;
		rect->right = right;
		rect->bottom = bottom;
	}

	void lrect_setMax(LRect* rect)
	{
		rect->left   = -32767;
		rect->top    = -32767;
		rect->right  =  32767;
		rect->bottom =  32767;
	}

	JBool lrect_equal(LRect* r1, LRect* r2)
	{
		s16 notEqual = 0;
		notEqual |= (r1->left != r2->left);
		notEqual |= (r1->top != r2->top);
		notEqual |= (r1->right != r2->right);
		notEqual |= (r1->bottom != r2->bottom);

		return notEqual ? JFALSE : JTRUE;
	}
		
	void lrect_offset(LRect* rect, s16 dx, s16 dy)
	{
		rect->left += dx;
		rect->right += dx;
		rect->top += dy;
		rect->bottom += dy;
	}

	void lrect_origin(LRect* rect)
	{
		lrect_offset(rect, -rect->left, -rect->top);
	}
		
	// Align rect relative to its 'parent'.
	void lrect_align(LRect* rect, LRect* parent, LAlignType xAlign, LAlignType yAlign)
	{
		switch (xAlign)
		{
			case LORIGIN:
			{
				rect->left  += parent->left;
				rect->right += parent->left;
			} break;
			case LCENTER:
			{
				s16 temp = (parent->right - parent->left) - (rect->right - rect->left);
				temp = (temp >> 1) + parent->left;

				rect->left  += temp;
				rect->right += temp;
			} break;
			case LEXTENT:
			{
				s16 temp = rect->left;
				rect->left  = parent->right - rect->right;
				rect->right = parent->right - temp;
			} break;
			default:
			{
				assert(0);
			}
		}

		switch (yAlign)
		{
			case LORIGIN:
			{
				rect->top += parent->top;
				rect->bottom += parent->top;
			} break;
			case LCENTER:
			{
				s16 temp = (parent->bottom - parent->top) - (rect->bottom - rect->top);
				temp = (temp >> 1) + parent->top;

				rect->top  += temp;
				rect->bottom += temp;
			} break;
			case LEXTENT:
			{
				s16 temp = rect->top;
				rect->top = parent->bottom - rect->bottom;
				rect->bottom = parent->bottom - temp;
			} break;
			default:
			{
				assert(0);
			}
		}
	}
		
	// Flips within the parent.
	void lrect_flip(LRect* rect, LRect* parent, JBool xFlip, JBool yFlip)
	{
		if (xFlip)
		{
			s16 x0 = (parent->right - rect->right) + parent->left;
			s16 x1 = x0 + (rect->right - rect->left);
			rect->left  = x0;
			rect->right = x1;
		}
		if (yFlip)
		{
			s16 y0 = (parent->bottom - rect->bottom) + parent->top;
			s16 y1 = y0 + (rect->bottom - rect->top);
			rect->top = y0;
			rect->bottom = y1;
		}
	}
		
	void lrect_inset(LRect* rect, s16 dx, s16 dy)
	{
		rect->left   += dx;
		rect->right  -= dx;
		rect->top    += dy;
		rect->bottom -= dy;
	}

	JBool lrect_isEmpty(LRect* rect)
	{
		if (rect->left >= rect->right)  { return JTRUE; }
		if (rect->top  >= rect->bottom) { return JTRUE; }
		return JFALSE;
	}

	JBool lrect_intersect(LRect* r1, LRect* r2)
	{
		if (lrect_isEmpty(r1) || lrect_isEmpty(r2)) { return JFALSE; }

		if (r1->right <= r2->left) { return JFALSE; }
		if (r1->left >= r2->right) { return JFALSE; }
		if (r1->bottom <= r2->top) { return JFALSE; }
		if (r1->top >= r2->bottom) { return JFALSE; }

		return JTRUE;
	}
		
	void lrect_enclose(LRect* r1, LRect* r2)
	{
		if (lrect_isEmpty(r1))
		{
			*r1 = *r2;
		}
		else if (!lrect_isEmpty(r2))
		{
			if (r1->left > r2->left)     { r1->left   = r2->left;   }
			if (r1->right < r2->right)   { r1->right  = r2->right;  }
			if (r1->top > r2->top)       { r1->top    = r2->top;    }
			if (r1->bottom < r2->bottom) { r1->bottom = r2->bottom; }
		}
	}

	JBool lrect_clip(LRect* r1, LRect* r2)
	{
		if (r1->left   < r2->left)   { r1->left   = r2->left;   }
		if (r1->right  > r2->right)  { r1->right  = r2->right;  }
		if (r1->top    < r2->top)    { r1->top    = r2->top;    }
		if (r1->bottom > r2->bottom) { r1->bottom = r2->bottom; }

		if (r1->left > r1->right)  { r1->right  = r1->left; }
		if (r1->top  > r1->bottom) { r1->bottom = r1->top;  }

		return (r1->left < r1->right && r1->top < r1->bottom) ? JTRUE : JFALSE;
	}

	void lrect_clipPoint(LRect* rect, s16* x, s16* y)
	{
		*x = clamp(*x, rect->left, rect->right);
		*y = clamp(*y, rect->top,  rect->bottom);
	}

	JBool lrect_isPointInside(LRect* rect, s16 x, s16 y)
	{
		if (x < rect->left || x >= rect->right)  { return JFALSE; }
		if (y < rect->top  || y >= rect->bottom) { return JFALSE; }
		return JTRUE;
	}
}  // namespace TFE_DarkForces