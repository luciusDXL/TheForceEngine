#include "lview.h"
#include "lactor.h"
#include "lcanvas.h"
#include "lpalette.h"
#include "lsound.h"
#include "lsystem.h"
#include "lfade.h"
#include "cutscene_film.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <TFE_Jedi/Math/core_math.h>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	LView* s_view = nullptr;
	JBool s_lviewInit = JFALSE;
	JBool s_updateView = JTRUE;
	static LView* s_defaultView = nullptr;
	static JBool s_running = JFALSE;
	static s32 s_exitValue = VIEW_LOOP_RUNNING;

	void lview_freeData(LView* view);
	void lview_initView(LView* view);
	void lview_trackView(s16 viewIndex, s16 snap);
	void lview_update(s32 time);
	void lview_updateCallback(s32 time);

	void lview_init()
	{
		s_defaultView = lview_alloc();
		if (s_defaultView)
		{
			s_lviewInit = JTRUE;
			lview_initView(s_defaultView);

			s_defaultView->coords = 0;
			s_defaultView->frameCount = 0;
			lview_setCurrent(s_defaultView);
		}
		s_updateView = JTRUE;
	}

	void lview_destroy()
	{
		if (s_defaultView)
		{
			lview_free(s_defaultView);
			s_defaultView = nullptr;
		}
		s_lviewInit = JFALSE;
	}

	void lview_initView(LView* view)
	{
		for (s32 i = 0; i < LVIEW_COUNT; i++)
		{
			if (i == 0)
			{
				lcanvas_getBounds(&view->frame[i]);
				view->zStart[i] = -32000;
				view->zStop[i]  =  32000;
			}
			else
			{
				lrect_clear(&view->frame[i]);
				view->zStart[i] = -32767;
				view->zStop[i]  =  32767;
			}

			view->xRel[i] = 0;
			view->yRel[i] = 0;
			view->xVel[i] = 0;
			view->yVel[i] = 0;
			view->trackActor[i] = nullptr;
			view->trackActive[i] = 0;
			view->clearView[i] = LVIEW_FLAG_CLEAR | LVIEW_FLAG_COPY;
		}
		lcanvas_getBounds(&view->clipFrame);

		view->time = 0;
		view->refreshWorld = 0;
		view->clear = 1;

		view->step = 0;
		view->stepCount = 0;
		view->updateFunc = nullptr;
	}

	void lview_clear()
	{
		if (s_view->clear)
		{
			lcanvas_eraseRect(&s_view->clipFrame);
		}
		else
		{
			for (s32 i = 0; i < LVIEW_COUNT; i++)
			{
				if (s_view->clearView[i] & LVIEW_FLAG_CLEAR)
				{
					LRect rect;
					lview_getViewClipFrame(i, &rect);
					lcanvas_eraseRect(&rect);
				}
			}
		}
	}

	void lview_draw(JBool refresh)
	{
		cutsceneFilm_drawFilms(refresh);
		lactor_draw(refresh);
		// TODO: System dialogs.
	}

	void lview_blit()
	{
		// Note: in Dark Forces, the fade is a while loop, pausing the view code.
		// For TFE, we run once loop iteration at a time, meaning that we have to
		// pause the view code using internal state.
		s_updateView = lcanvas_applyFade(JFALSE);
		vfb_swap();
	}

	void lview_startLoop()
	{
		s_view->time = 0;
		s_view->step = 0;
		s_view->stepCount = 0;
		s_exitValue = VIEW_LOOP_RUNNING;
	}

	void lview_endLoop()
	{
		// lview_freeAll(s_view);
		lview_initView(s_view);
		lpalette_stopAllCycles();
	}
		
	// A single iteration of the view loop.
	s32 lview_loop()
	{
		if (s_exitValue != VIEW_LOOP_RUNNING)
		{
			return s_exitValue;
		}

		if (s_updateView)
		{
			if (!s_view->step)
			{
				lview_update(s_view->time);
				lview_updateCallback(s_view->time);
			}

			lview_clear();
			lview_draw(s_view->refreshWorld);
			s_view->refreshWorld = JFALSE;
		}

		lview_blit();

		if (s_updateView)
		{
			if (!s_view->step)
			{
				lpalette_cycleScreen();
				if (s_view->updateFunc) { s_exitValue = s_view->updateFunc(s_view->time); }
				s_view->time++;
			}
		}

		return s_exitValue;
	}
		
	void lview_update(s32 time)
	{
		ltime_often();
		lview_move();
		cutsceneFilm_updateFilms(time);
		lactor_update(time);
		lsound_update();
	}

	void lview_updateCallback(s32 time)
	{
		ltime_often();
		cutsceneFilm_updateCallbacks(time);
		lactor_updateCallbacks(time);
		lactor_updateZPlanes();
		// sound_updateCallbacks(time);
		// sound_freeUserSounds(sound_getList());
	}

	LView* lview_alloc()
	{
		LView* view = (LView*)landru_alloc(sizeof(LView));
		memset(view, 0, sizeof(LView));
		return view;
	}

	void lview_free(LView* view)
	{
		lview_freeData(view);
		landru_free(view);
	}

	void lview_setCurrent(LView* view)
	{
		s_view = view;
	}

	LView* lview_getCurrent()
	{
		return s_view;
	}

	void lview_move()
	{
		for (s32 i = 0; i < LVIEW_COUNT; i++)
		{
			if (s_view->trackActive[i])
			{
				lview_trackView(i, 0);
			}
			else
			{
				s_view->xRel[i] += s_view->xVel[i];
				s_view->yRel[i] += s_view->yVel[i];
			}
		}
	}
		
	void lview_originToView(s16 viewIndex, s16* x, s16* y)
	{
		*x -= s_view->xRel[viewIndex];
		*y -= s_view->yRel[viewIndex];
	}

	void lview_viewToOrigin(s16 viewIndex, s16* x, s16* y)
	{
		*x += s_view->xRel[viewIndex];
		*y += s_view->yRel[viewIndex];
	}

	// View tracks an actor and tries to keep it centered.
	void lview_trackView(s16 viewIndex, s16 snap)
	{
		if (!s_view->trackActor[viewIndex]) { return; }

		s16 x, y, xFract, yFract;
		lactor_getPos(s_view->trackActor[viewIndex], &x, &y, &xFract, &yFract);

		LRect frame;
		lview_getFrame(viewIndex, &frame);

		// Move the view so that the actor is in the center.
		x -= (frame.right - frame.left) >> 1;
		y -= (frame.bottom - frame.top) >> 1;
		// Handle the offset.
		x += s_view->trackXOffset[viewIndex];
		y += s_view->trackYOffset[viewIndex];
		// Then clip to the rect.
		lrect_clipPoint(&s_view->trackFrame[viewIndex], &x, &y);

		// Determine how much to move so it can be clamped.
		s16 xVel = x - s_view->xRel[viewIndex];
		s16 yVel = y - s_view->yRel[viewIndex];
		if (!snap)
		{
			xVel = clamp(xVel, -s_view->maxTrackXVel[viewIndex], s_view->maxTrackXVel[viewIndex]);
			yVel = clamp(yVel, -s_view->maxTrackYVel[viewIndex], s_view->maxTrackYVel[viewIndex]);
		}

		// Finally update the position.
		s_view->xRel[viewIndex] += xVel;
		s_view->yRel[viewIndex] += yVel;
	}

	void lview_enableTracking(s16 viewIndex)
	{
		s_view->trackActive[viewIndex] = 1;
	}

	void lview_disableTracking(s16 viewIndex)
	{
		s_view->trackActive[viewIndex] = 0;
	}

	void lview_setTrackingFrame(s16 viewIndex, LRect* rect)
	{
		s_view->trackFrame[viewIndex] = *rect;
	}
		
	void lview_getTrackingFrame(s16 viewIndex, LRect* rect)
	{
		*rect = s_view->trackFrame[viewIndex];
	}

	void lview_setTrackingMaxVelocity(s16 viewIndex, s16 xVel, s16 yVel)
	{
		s_view->maxTrackXVel[viewIndex] = xVel;
		s_view->maxTrackYVel[viewIndex] = yVel;
	}

	void lview_getTrackingMaxVelocity(s16 viewIndex, s16* xVel, s16* yVel)
	{
		*xVel = s_view->maxTrackXVel[viewIndex];
		*yVel = s_view->maxTrackYVel[viewIndex];
	}

	void lview_setTrackingOffset(s16 viewIndex, s16 xOffset, s16 yOffset)
	{
		s_view->trackXOffset[viewIndex] = xOffset;
		s_view->trackYOffset[viewIndex] = yOffset;
	}

	void lview_getTrackingOffset(s16 viewIndex, s16* xOffset, s16* yOffset)
	{
		*xOffset = s_view->trackXOffset[viewIndex];
		*yOffset = s_view->trackYOffset[viewIndex];
	}

	void lview_trackActor(s16 viewIndex, LActor* actor, LRect* rect, s16 maxXVel, s16 maxYVel)
	{
		s_view->trackActor[viewIndex] = actor;
		lview_setTrackingFrame(viewIndex, rect);
		lview_setTrackingMaxVelocity(viewIndex, maxXVel, maxYVel);

		s16 w, h;
		lactor_getSize(actor, &w, &h);
		lview_setTrackingOffset(viewIndex, w>>1, h>>1);
		lview_enableTracking(viewIndex);
	}
		
	void lview_setFrame(s16 viewIndex, LRect* rect)
	{
		s_view->frame[viewIndex] = *rect;
	}

	void lview_getFrame(s16 viewIndex, LRect* rect)
	{
		*rect = s_view->frame[viewIndex];
	}

	void lview_getViewClipFrame(s16 viewIndex, LRect* rect)
	{
		*rect = s_view->frame[viewIndex];
		lrect_clip(rect, &s_view->clipFrame);
	}

	void lview_setClipFrame(LRect* rect)
	{
		s_view->clipFrame = *rect;
	}

	void lview_getClipFrame(LRect* rect)
	{
		*rect = s_view->clipFrame;
	}

	void lview_restoreClipFrame()
	{
		lcanvas_getBounds(&s_view->clipFrame);
	}

	void lview_setZPlane(s16 viewIndex, s16 zStart, s16 zStop)
	{
		s_view->zStart[viewIndex] = zStart;
		s_view->zStop[viewIndex]  = zStop;
	}

	void lview_getZPlane(s16 viewIndex, s16* zStart, s16* zStop)
	{
		*zStart = s_view->zStart[viewIndex];
		*zStop  = s_view->zStop[viewIndex];
	}

	void lview_setPos(s16 viewIndex, s16 x, s16 y)
	{
		s_view->xRel[viewIndex] = x;
		s_view->yRel[viewIndex] = y;
	}
		
	void lview_getPos(s16 viewIndex, s16* x, s16* y)
	{
		*x = s_view->xRel[viewIndex];
		*y = s_view->yRel[viewIndex];
	}

	void lview_setSpeed(s16 viewIndex, s16 xVel, s16 yVel)
	{
		s_view->xVel[viewIndex] = xVel;
		s_view->yVel[viewIndex] = yVel;
	}

	void lview_getSpeed(s16 viewIndex, s16* xVel, s16* yVel)
	{
		*xVel = s_view->xVel[viewIndex];
		*yVel = s_view->yVel[viewIndex];
	}

	void lview_setUpdateFunc(LViewUpdateFunc updateFunc)
	{
		s_view->updateFunc = updateFunc;
	}

	void lview_getUpdateFunc(LViewUpdateFunc* updateFunc)
	{
		*updateFunc = s_view->updateFunc;
	}

	void lview_clearUpdateFunc()
	{
		s_view->updateFunc = nullptr;
	}

	void lview_refreshWorld()
	{
		s_view->refreshWorld = 1;
	}
		
	JBool lview_clearEnabled(s16 viewIndex)
	{
		return (s_view->clearView[viewIndex] & LVIEW_FLAG_CLEAR) ? JTRUE : JFALSE;
	}

	void lview_enableClear(s16 viewIndex)
	{
		s_view->clearView[viewIndex] |= LVIEW_FLAG_CLEAR;
	}

	void lview_disableClear(s16 viewIndex)
	{
		s_view->clearView[viewIndex] &= ~LVIEW_FLAG_CLEAR;
	}

	void lview_enableGlobalClear()
	{
		s_view->clear = 1;
	}

	void lview_disableGlobalClear()
	{
		s_view->clear = 0;
	}

	void lview_enableClearAll()
	{
		for (s32 i = 0; i < LVIEW_COUNT; i++)
		{
			lview_enableClear(i);
		}
	}

	void lview_disableClearAll()
	{
		for (s32 i = 0; i < LVIEW_COUNT; i++)
		{
			lview_disableClear(i);
		}
	}
		
	JBool lview_copyEnabled(s16 viewIndex)
	{
		return (s_view->clearView[viewIndex] & LVIEW_FLAG_COPY) ? JTRUE : JFALSE;
	}

	void lview_enableCopy(s16 viewIndex)
	{
		s_view->clearView[viewIndex] |= LVIEW_FLAG_COPY;
	}

	void lview_disableCopy(s16 viewIndex)
	{
		s_view->clearView[viewIndex] &= ~LVIEW_FLAG_COPY;
	}

	s32 lview_getTime()
	{
		return s_view->time;
	}

	// Clip the current rect to the view.
	JBool lview_clipObjToView(s16 viewIndex, s16 zPlane, LRect* frame, LRect* rect, LRect* clip_rect)
	{
		LRect clipFrame;

		// Check the z range.
		if (zPlane >= s_view->zStart[viewIndex] && zPlane <= s_view->zStop[viewIndex])
		{
			// view relative
			s16 xOffset = s_view->frame[viewIndex].left - s_view->xRel[viewIndex];
			s16 yOffset = s_view->frame[viewIndex].top  - s_view->yRel[viewIndex];

			*rect = *frame;
			lrect_offset(rect, xOffset, yOffset);
			*clip_rect = *rect;

			lview_getViewClipFrame(viewIndex, &clipFrame);
			lrect_clip(clip_rect, &clipFrame);
			
			// Is there anything left?
			return !lrect_isEmpty(clip_rect);
		}
		return JFALSE;
	}
	
	// For some reason a view is passed in but isn't actully used by any of this...
	void lview_freeData(LView* view)
	{
		// TODO:

		// free cutscenes.
		lactor_freeList(lactor_getList());
		// free palettes
		// free sounds.
	}
}