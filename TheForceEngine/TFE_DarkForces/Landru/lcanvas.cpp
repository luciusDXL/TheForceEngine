#include "lcanvas.h"
#include <TFE_System/system.h>
#include <TFE_Jedi/Math/core_math.h>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	static s16 s_lcanvasSize[2];
	static LRect s_lcanvasRect;
	static LRect s_lcanvasClipRect;
		
	void lcanvas_init(s16 w, s16 h)
	{
		s_lcanvasSize[0] = w;
		s_lcanvasSize[1] = h;
		lrect_set(&s_lcanvasRect, 0, 0, w, h);
		s_lcanvasClipRect = s_lcanvasRect;
	}

	void lcanvas_destroy()
	{
	}

	void lcanvas_getBounds(LRect* rect)
	{
		*rect = s_lcanvasRect;
	}

	void lcanvas_setClip(LRect* rect)
	{
		s_lcanvasClipRect = *rect;
	}

	void lcanvas_getClip(LRect* rect)
	{
		*rect = s_lcanvasClipRect;
	}

	JBool lcanvas_clipRectToCanvas(LRect* rect)
	{
		return lrect_clip(rect, &s_lcanvasClipRect);
	}

	void lcanvas_clearClipRect()
	{
		s_lcanvasClipRect = s_lcanvasRect;
	}
}  // namespace TFE_DarkForces