#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru Canvas - this is drawn to by the system.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "lrect.h"

namespace TFE_DarkForces
{
	void lcanvas_init(s16 w, s16 h);
	void lcanvas_destroy();

	void  lcanvas_getBounds(LRect* rect);
	void  lcanvas_setClip(LRect* rect);
	void  lcanvas_getClip(LRect* rect);
	JBool lcanvas_clipRectToCanvas(LRect* rect);
	void  lcanvas_clearClipRect();

	void  lcanvas_eraseRect(LRect* rect);
	JBool lcanvas_applyFade(JBool dialog);

	void  lcanvas_clear();
	void  lcanvas_showNextFrame();
	void  lcanvas_copyScreenToVideo(LRect* rect);
	void  lcanvas_copyPortionToVideo(LRect* rect, s16 x, s16 y);
}  // namespace TFE_DarkForces