#include "lcanvas.h"
#include "lfade.h"
#include "ldraw.h"
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

	void lcanvas_copyToFramebuffer(LRect* rect, s16 x, s16 y);
		
	void lcanvas_init(s16 w, s16 h)
	{
		s_lcanvasSize[0] = w;
		s_lcanvasSize[1] = h;
		lrect_set(&s_lcanvasRect, 0, 0, w, h);
		s_lcanvasClipRect = s_lcanvasRect;

		vfb_setResolution(w, h);
		ldraw_init(w, h);
	}

	void lcanvas_destroy()
	{
		ldraw_destroy();
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

	void lcanvas_clear()
	{
		LRect bounds;
		lcanvas_getBounds(&bounds);
		s32 width  = bounds.right - bounds.left;
		s32 height = bounds.bottom - bounds.top;
		memset(ldraw_getBitmap(), 0, width * height);
	}

	void lcanvas_eraseRect(LRect* rect)
	{
		LRect clipRect;
		lcanvas_getClip(&clipRect);
		lcanvas_clearClipRect();

		drawClippedColorRect(rect, 0);

		lcanvas_setClip(&clipRect);
	}

	JBool lcanvas_applyFade(JBool dialog)
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
		return lfade_applyFadeLoop(&rect, dialog);
	}

	void lcanvas_showNextFrame()
	{
		LRect bounds;
		lcanvas_getBounds(&bounds);
		s32 width  = bounds.right - bounds.left;
		s32 height = bounds.bottom - bounds.top;

		memcpy(vfb_getCpuBuffer(), ldraw_getBitmap(), width * height);
		vfb_swap();
	}

	void lcanvas_copyScreenToVideo(LRect* rect)
	{
		lcanvas_copyPortionToVideo(rect, rect->left, rect->top);
	}
		
	void lcanvas_copyPortionToVideo(LRect* rect, s16 x, s16 y)
	{
		LRect bounds;
		lcanvas_getBounds(&bounds);

		LRect srcRect = *rect;
		if (!lrect_clip(&srcRect, &bounds)) { return; }

		LRect clipRect = srcRect;
		lrect_offset(&clipRect, x - rect->left, y - rect->top);
		LRect dstRect = clipRect;
		if (!lrect_clip(&dstRect, &bounds)) { return; }

		srcRect.left    += dstRect.left   - clipRect.left;
		srcRect.top     += dstRect.top    - clipRect.top;
		srcRect.right   += dstRect.right  - clipRect.right;
		srcRect.bottom  += dstRect.bottom - clipRect.bottom;

		lcanvas_copyToFramebuffer(&srcRect, rect->left, rect->top);
	}

	void lcanvas_copyToFramebuffer(LRect* srcRect, s16 x, s16 y)
	{
		const u8* srcData = ldraw_getBitmap();
		u8* dstData = vfb_getCpuBuffer();
		if (!srcData || !dstData) { return; }

		LRect bounds;
		lcanvas_getBounds(&bounds);
		s32 width = bounds.right - bounds.left;
				
		for (s32 fy = srcRect->top; fy < srcRect->bottom; fy++)
		{
			memcpy(&dstData[(y+fy-srcRect->top)*width + x], &srcData[fy*width + srcRect->left], srcRect->right - srcRect->left);
		}
	}
}  // namespace TFE_DarkForces