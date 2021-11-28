#include "lcanvas.h"
#include "lfade.h"
#include <TFE_System/system.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
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

		vfb_setResolution(w, h);
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

	void lcanvas_eraseRect(LRect* rect)
	{
		LRect clipRect;
		lcanvas_getClip(&clipRect);
		lcanvas_clearClipRect();

		// TODO: Actual drawing API.
		// PaintClippedRect(rect, 0);
		// Hack.
		u8* buffer = vfb_getCpuBuffer();
		for (s32 i = 0; i < 320 * 200; i++)
		{
			buffer[i] = 0;
		}

		lcanvas_setClip(&clipRect);
	}

	void lcanvas_applyFade(JBool dialog)
	{
		LRect rect;
		lcanvas_getBounds(&rect);

		/*
		if (cursor is visible)
		{
			TODO: Cursor is visible.
		}
		else
		{
			...
		}
		*/
		lfade_applyFade(&rect, dialog);
	}

	void lcanvas_copyScreenToVideo(LRect* rect)
	{
	}

	void lcanvas_copyPortionToVideo(LRect* rect, s16 x, s16 y)
	{

	}
}  // namespace TFE_DarkForces