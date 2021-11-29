#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru Fade System
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "lrect.h"
#include "lview.h"

namespace TFE_DarkForces
{
	enum LFadeConstants
	{
		LFADE_PALETTE = 1,
		LFADE_LOCK = 1,
	};

	enum FadeType
	{
		ftype_noFade = 0,
		ftype_snapFade,
		ftype_shortSnapFade,
		ftype_longSnapFade,
		ftype_slideLeftFade,
		ftype_slideRightFade,
		ftype_slideUpFade,
		ftype_slideDownFade,
		ftype_slideTLeftFade,
		ftype_slideTRightFade,
		ftype_slideBLeftFade,
		ftype_slideBRightFade,
		ftype_wipeLeftFade,
		ftype_wipeRightFade,
		ftype_wipeUpFade,
		ftype_wipeDownFade,
		ftype_wipeTLeftFade,
		ftype_wipeTRightFade,
		ftype_wipeBLeftFade,
		ftype_wipeBRightFade,
		ftype_horizCenterFade,
		ftype_vertCenterFade,
		ftype_centerFade,
		ftype_thinSliceFade,
		ftype_sliceFade,
		ftype_thickSliceFade,
		ftype_count
	};

	enum FadeColorType
	{
		fcolorType_noColorFade,
		fcolorType_snapColorFade,
		fcolorType_monoColorFade,
		fcolorType_toMonoColorFade,
		fcolorType_fromMonoColorFade,
		fcolorType_paletteColorFade,
		fcolorType_count
	};

	struct LFade
	{
		FadeType type[LVIEW_COUNT];
		s16 delay[LVIEW_COUNT];
		s16 lock[LVIEW_COUNT];
		s16 step[LVIEW_COUNT];
		s16 len[LVIEW_COUNT];

		FadeType fullType;
		s16 fullDelay;
		s16 fullLock;
		s16 fullStep;
		s16 fullLen;

		FadeColorType colorType;
		s16 colorDelay;
		s16 colorLock;
		s16 colorStep;
		s16 colorLen;
		u8 r;
		u8 g;
		u8 b;
		u8 pad8;
	};

	void lfade_init();
	void lfade_destroy();
	void lfade_start(s16 viewId, FadeType type, FadeColorType colorType, JBool paletteChange, s16 delay, s16 lock);
	void lfade_startFull(FadeType type, FadeColorType colorType, JBool paletteChange, s16 delay, s16 lock);
	void lfade_startColorFade(FadeColorType colorType, JBool paletteChange, s16 len, s16 delay, s16 lock);
	void lfade_setColor(s16 r, s16 g, s16 b);

	JBool lfade_isActive();
	JBool lfade_applyFadeLoop(LRect* rect, JBool dialog);
}  // namespace TFE_DarkForces