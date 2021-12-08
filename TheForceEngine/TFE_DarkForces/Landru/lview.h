#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru View
// Used by the cutscene/UI system to setup different clipping/render
// regions.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "lrect.h"

namespace TFE_DarkForces
{
	struct LActor;

	enum LViewConstants
	{
		LVIEW_COUNT = 4,
		VIEW_LOOP_RUNNING = INT_MAX,
		// Flags
		LVIEW_FLAG_CLEAR = FLAG_BIT(0),
		LVIEW_FLAG_COPY  = FLAG_BIT(1),
	};

	typedef s32(*LViewUpdateFunc)(s32);

	struct LView
	{
		LRect frame[LVIEW_COUNT];
		s16   zStart[LVIEW_COUNT];
		s16   zStop[LVIEW_COUNT];
		s16   xRel[LVIEW_COUNT];
		s16   yRel[LVIEW_COUNT];
		s16   xVel[LVIEW_COUNT];
		s16   yVel[LVIEW_COUNT];

		LRect trackFrame[LVIEW_COUNT];
		LActor* trackActor[LVIEW_COUNT];
		s16 maxTrackXVel[LVIEW_COUNT];
		s16 maxTrackYVel[LVIEW_COUNT];
		s16 trackActive[LVIEW_COUNT];
		s16 trackXOffset[LVIEW_COUNT];
		s16 trackYOffset[LVIEW_COUNT];
		s16 clearView[LVIEW_COUNT];

		LRect clipFrame;
		s32   time;

		s16   refreshWorld;
		s16   clear;
		s16   step;
		s16   stepCount;
		s16   coords;
		s16   frameCount;

		LViewUpdateFunc updateFunc;
	};

	void lview_init();
	void lview_destroy();

	LView* lview_alloc();
	void lview_free(LView* view);
	void lview_initView(LView* view);

	void lview_startLoop();
	void lview_endLoop();
	s32  lview_loop();

	void lview_setCurrent(LView* view);
	LView* lview_getCurrent();

	void lview_move();
	void lview_originToView(s16 viewIndex, s16* x, s16* y);
	void lview_viewToOrigin(s16 viewIndex, s16* x, s16* y);
	void lview_trackView(s16 viewIndex, s16 snap);
	void lview_enableTracking(s16 viewIndex);
	void lview_disableTracking(s16 viewIndex);
	void lview_setTrackingFrame(s16 viewIndex, LRect* rect);
	void lview_getTrackingFrame(s16 viewIndex, LRect* rect);
	void lview_setTrackingMaxVelocity(s16 viewIndex, s16 xVel, s16 yVel);
	void lview_getTrackingMaxVelocity(s16 viewIndex, s16* xVel, s16* yVel);
	void lview_setTrackingOffset(s16 viewIndex, s16 xOffset, s16 yOffset);
	void lview_getTrackingOffset(s16 viewIndex, s16* xOffset, s16* yOffset);
	void lview_trackActor(s16 viewIndex, LActor* actor, LRect* rect, s16 maxXVel, s16 maxYVel);
	void lview_setFrame(s16 viewIndex, LRect* rect);
	void lview_getFrame(s16 viewIndex, LRect* rect);
	void lview_getViewClipFrame(s16 viewIndex, LRect* rect);
	void lview_setClipFrame(LRect* rect);
	void lview_getClipFrame(LRect* rect);
	void lview_restoreClipFrame();
	void lview_setZPlane(s16 viewIndex, s16 zStart, s16 zStop);
	void lview_getZPlane(s16 viewIndex, s16* zStart, s16* zStop);
	void lview_setPos(s16 viewIndex, s16 x, s16 y);
	void lview_getPos(s16 viewIndex, s16* x, s16* y);
	void lview_setSpeed(s16 viewIndex, s16 xVel, s16 yVel);
	void lview_getSpeed(s16 viewIndex, s16* xVel, s16* yVel);
	void lview_setUpdateFunc(LViewUpdateFunc updateFunc);
	void lview_getUpdateFunc(LViewUpdateFunc* updateFunc);
	void lview_clearUpdateFunc();
	void lview_refreshWorld();
	void lview_enableClear(s16 viewIndex);
	void lview_disableClear(s16 viewIndex);
	void lview_enableGlobalClear();
	void lview_disableGlobalClear();
	void lview_enableClearAll();
	void lview_disableClearAll();
	void lview_enableCopy(s16 viewIndex);
	void lview_disableCopy(s16 viewIndex);

	JBool lview_clearEnabled(s16 viewIndex);
	JBool lview_copyEnabled(s16 viewIndex);
	JBool lview_clipObjToView(s16 viewIndex, s16 zPlane, LRect* frame, LRect* rect, LRect* clip_rect);
	s32 lview_getTime();
}  // namespace TFE_DarkForces