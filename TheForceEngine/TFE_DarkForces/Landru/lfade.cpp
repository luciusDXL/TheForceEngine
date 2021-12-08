#include "lfade.h"
#include "lcanvas.h"
#include "lpalette.h"
#include "lsystem.h"
#include "ltimer.h"
#include "ldraw.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <assert.h>
#include <map>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	JBool s_fadeInit = JFALSE;
	LFade* s_fade = nullptr;

	LFade* lfade_alloc();
	void lfade_free(LFade* fade);
	s16 lfade_calculateFadeLength(FadeType type);
	void lfade_startColorFade(FadeColorType colorType, JBool paletteChange, s16 len, s16 delay, s16 lock);

	void lfade_init()
	{
		s_fade = lfade_alloc();

		if (s_fade)
		{
			for (s32 i = 0; i < LVIEW_COUNT; i++)
			{
				s_fade->type[i] = ftype_noFade;
			}
			s_fade->fullType = ftype_noFade;
			s_fade->colorType = fcolorType_noColorFade;
			s_fade->r = 0;
			s_fade->g = 0;
			s_fade->b = 0;

			s_fadeInit = JTRUE;
		}
	}

	void lfade_destroy()
	{
		if (s_fadeInit && s_fade)
		{
			lfade_free(s_fade);
			s_fade = nullptr;
		}
		s_fadeInit = JFALSE;
	}

	LFade* lfade_alloc()
	{
		return (LFade*)landru_alloc(sizeof(LFade));
	}

	void lfade_free(LFade* fade)
	{
		landru_free(fade);
	}

	void lfade_start(s16 viewId, FadeType type, FadeColorType colorType, JBool paletteChange, s16 delay, s16 lock)
	{
		s_fade->type[viewId] = type;
		s_fade->delay[viewId] = delay;
		s_fade->lock[viewId] = lock;
		s_fade->len[viewId] = lfade_calculateFadeLength(type);
		s_fade->step[viewId] = 0;

		lfade_startColorFade(colorType, paletteChange, s_fade->len[viewId], delay, lock);
	}

	void lfade_startFull(FadeType type, FadeColorType colorType, JBool paletteChange, s16 delay, s16 lock)
	{
		s_fade->fullType = type;
		s_fade->fullDelay = delay;
		s_fade->fullLock = lock;
		s_fade->fullLen = lfade_calculateFadeLength(type);
		s_fade->fullStep = 0;

		lfade_startColorFade(colorType, paletteChange, s_fade->fullLen, delay, lock);
	}

	void lfade_startColorFade(FadeColorType colorType, JBool paletteChange, s16 len, s16 delay, s16 lock)
	{
		if (colorType != fcolorType_noColorFade)
		{
			s_fade->colorType = colorType;
			lpalette_copyScreenToSrc(0, 0, 255);
			if (!paletteChange)
			{
				lpalette_copyScreenToDest(0, 0, 255);
			}

			s_fade->colorDelay = delay;
			s_fade->colorLock = lock;
			s_fade->colorLen = len;
			s_fade->colorStep = 0;
		}
	}

	void lfade_setColor(s16 r, s16 g, s16 b)
	{
		s_fade->r = u8(r);
		s_fade->g = u8(g);
		s_fade->b = u8(b);
	}

	s16 lfade_calculateFadeLength(FadeType type)
	{
		s16 time;

		switch (type)
		{
			case ftype_snapFade:
				time = 1;
				break;
			case ftype_shortSnapFade:
				time = 16;
				break;
			default:
				time = 32;
				break;
		}

		return time;
	}

	JBool lfade_isStep(FadeType type, s16 lock, s16 delay, s32 time, s32 lockTime)
	{
		if (type == ftype_noFade) { return 0; }
		if (time == lockTime) { return !delay; }
		return (!delay) && lock;
	}

	JBool lfade_isDelayStep(FadeType type, s16 delay, s32 time, s32 lockTime)
	{
		if (delay && type != ftype_noFade && time == lockTime)
		{
			return JTRUE;
		}
		return JFALSE;
	}

	void lfade_calculateFadeLockTime(s32* time, s32* end)
	{
		s32 lockTime = lview_getTime();
		s32 lockEnd = lockTime;

		if (lfade_isStep(s_fade->fullType, s_fade->fullLock, s_fade->fullDelay, lockTime, lockTime))
		{
			if (s_fade->fullLock)
			{
				lockEnd = lockTime + s_fade->fullLen;
			}

			for (s32 i = 0; i < LVIEW_COUNT; i++)
			{
				s_fade->type[i] = ftype_noFade;
			}
		}
		else
		{
			for (s32 i = 0; i < LVIEW_COUNT; i++)
			{
				if (lfade_isStep(s_fade->type[i], s_fade->lock[i], s_fade->delay[i], lockTime, lockTime) && s_fade->lock[i])
				{
					if (lockTime + s_fade->len[i] > lockEnd)
					{
						lockEnd = lockTime + s_fade->fullLen;
					}
				}
			}
		}

		*time = lockTime;
		*end = lockEnd;
	}

	JBool lfade_isActive()
	{
		s32 time = lview_getTime();
		if (s_fade->fullType != ftype_noFade && !s_fade->fullDelay)
		{
			return JTRUE;
		}
		for (s32 i = 0; i < LVIEW_COUNT; i++)
		{
			if (s_fade->type[i] != ftype_noFade && !s_fade->delay[i])
			{
				return JTRUE;
			}
		}
		return JFALSE;
	}

	void lfade_fadeToVideoPalette(s32 time, s32 lockTime)
	{
		s16 applyFadeStep = 0;
		if (s_fade->colorType != fcolorType_noColorFade)
		{
			if (s_fade->colorLock)
			{
				if (time == lockTime)
				{
					if (s_fade->colorDelay)
					{
						s_fade->colorDelay--;
					}
					else
					{
						applyFadeStep = 1;
					}
				}
				else if (!s_fade->colorDelay)
				{
					applyFadeStep = 1;
				}
			}
			else if (time == lockTime)
			{
				if (s_fade->colorDelay)
				{
					s_fade->colorDelay--;
				}
				else
				{
					applyFadeStep = 1;
				}
			}
		}

		if (applyFadeStep)
		{
			s16 step = s_fade->colorStep;
			s16 len  = s_fade->colorLen;
			s16 mid  = len >> 1;

			switch (s_fade->colorType)
			{
				case fcolorType_snapColorFade:
				{
					if (step == mid)
					{
						// Make sure we are show the next frame *before* changing the palette.
						lcanvas_showNextFrame();

						lpalette_copyDstToScreen(0, 0, 255);
						lpalette_putScreenPal();
					}
				} break;
				case fcolorType_monoColorFade:
				{
					if (step < mid)
					{
						s16 fraction = step * 256 / mid;
						lpalette_buildFadePalette(LPAL_BUILD_MONODEST, 0, 255, fraction, (s16)s_fade->r, (s16)s_fade->g, (s16)s_fade->b);
					}
					else
					{
						s16 fraction = (step - mid) * 256 / (len - mid);
						lpalette_buildFadePalette(LPAL_BUILD_MONOSRC, 0, 255, fraction, (s16)s_fade->r, (s16)s_fade->g, (s16)s_fade->b);
					}
					lpalette_putScreenPal();
				} break;
				case fcolorType_toMonoColorFade:
				{
					s16 fraction = step * 256 / len;
					lpalette_buildFadePalette(LPAL_BUILD_MONODEST, 0, 255, fraction, (s16)s_fade->r, (s16)s_fade->g, (s16)s_fade->b);
					lpalette_putScreenPal();
				} break;
				case fcolorType_fromMonoColorFade:
				{
					s16 fraction = step * 256 / len;
					lpalette_buildFadePalette(LPAL_BUILD_MONOSRC, 0, 255, fraction, (s16)s_fade->r, (s16)s_fade->g, (s16)s_fade->b);
					lpalette_putScreenPal();
				} break;
				case fcolorType_paletteColorFade:
				{
					s16 fraction = step * 256 / len;
					lpalette_buildFadePalette(LPAL_BUILD_PALETTE, 0, 255, fraction, 0, 0, 0);
					lpalette_putScreenPal();
				} break;
			}

			s_fade->colorStep++;
			if (s_fade->colorStep > s_fade->colorLen)
			{
				s_fade->colorType = fcolorType_noColorFade;
			}
		}
	}

	void lfade_copyToVideo(LRect* frame, FadeType type, s16 step, s16 length, s16 lock)
	{
		if (lrect_isEmpty(frame)) { return; }

		s16 revStep = length - step;
		LRect fadeRect = *frame;
		s16 slice = 0;
		s16 xOffset = frame->left;
		s16 yOffset = frame->top;
		s16 cut = 0;
		s16 wOffset = 0;

		switch (type)
		{
			case ftype_shortSnapFade:
			case ftype_longSnapFade:
			{
				if ((step != length >> 1) && lock)
				{
					lrect_set(&fadeRect, 0, 0, 0, 0);
				}
			} break;
			case ftype_slideLeftFade:
			{
				fadeRect.left = frame->right - ((frame->right - frame->left) * step) / length;
			} break;
			case ftype_slideRightFade:
			{
				fadeRect.right = frame->left + ((frame->right - frame->left) * step) / length;
				xOffset = frame->right - (fadeRect.right - fadeRect.left);
			} break;
			case ftype_slideUpFade:
			{
				fadeRect.bottom = frame->top + ((frame->bottom - frame->top) * step) / length;
				yOffset = frame->bottom - (fadeRect.bottom - fadeRect.top);
			} break;
			case ftype_slideDownFade:
			{
				fadeRect.top = frame->bottom - ((frame->bottom - frame->top) * step) / length;
			} break;
			case ftype_slideTLeftFade:
			{
				fadeRect.top = frame->bottom - ((frame->bottom - frame->top) * step) / length;
				fadeRect.right = frame->left + ((frame->right - frame->left) * step) / length;
				xOffset = frame->right - (fadeRect.right - fadeRect.left);
			} break;
			case ftype_slideTRightFade:
			{
				fadeRect.top  = frame->bottom - ((frame->bottom - frame->top) * step) / length;
				fadeRect.left = frame->right  - ((frame->right - frame->left) * step) / length;
			} break;
			case ftype_slideBLeftFade:
			{
				fadeRect.bottom = frame->top + ((frame->bottom - frame->top) * step) / length;
				fadeRect.right = frame->left + ((frame->right - frame->left) * step) / length;
				xOffset = frame->right  - (fadeRect.right - fadeRect.left);
				yOffset = frame->bottom - (fadeRect.bottom - fadeRect.top);
			} break;
			case ftype_slideBRightFade:
			{
				fadeRect.bottom = frame->top + ((frame->bottom - frame->top) * step) / length;
				fadeRect.left = frame->right - ((frame->right - frame->left) * step) / length;
				yOffset = frame->bottom - (fadeRect.bottom - fadeRect.top);
			} break;
			case ftype_wipeLeftFade:
			{
				fadeRect.right = frame->left + ((frame->right - frame->left) * step) / length;
			} break;
			case ftype_wipeRightFade:
			{
				fadeRect.left = frame->right - ((frame->right - frame->left) * step) / length;
				xOffset = fadeRect.left;
			} break;
			case ftype_wipeUpFade:
			{
				fadeRect.top = frame->bottom - ((frame->bottom - frame->top) * step) / length;
				yOffset = fadeRect.top;
			} break;
			case ftype_wipeDownFade:
			{
				fadeRect.bottom = frame->top + ((frame->bottom - frame->top) * step) / length;
			} break;
			case ftype_wipeTLeftFade:
			{
				fadeRect.top = frame->bottom - ((frame->bottom - frame->top) * step) / length;
				fadeRect.right = frame->left + ((frame->right - frame->left) * step) / length;
				yOffset = fadeRect.top;
			} break;
			case ftype_wipeTRightFade:
			{
				fadeRect.top = frame->bottom - ((frame->bottom - frame->top) * step) / length;
				fadeRect.left = frame->right - ((frame->right - frame->left) * step) / length;
				xOffset = fadeRect.left;
				yOffset = fadeRect.top;
			} break;
			case ftype_wipeBLeftFade:
			{
				fadeRect.bottom = frame->top + ((frame->bottom - frame->top) * step) / length;
				fadeRect.right = frame->left + ((frame->right - frame->left) * step) / length;
			} break;
			case ftype_wipeBRightFade:
			{
				fadeRect.bottom = frame->top + ((frame->bottom - frame->top) * step) / length;
				fadeRect.left = frame->right - ((frame->right - frame->left) * step) / length;
				xOffset = fadeRect.left;
			} break;
			case ftype_vertCenterFade:
			{
				s16 val = (((frame->bottom - frame->top) >> 1) * revStep) / length;
				fadeRect.top = frame->top + val;
				fadeRect.bottom = frame->bottom - val;
				yOffset = fadeRect.top;
			} break;
			case ftype_horizCenterFade:
			{
				s16 val = (((frame->right - frame->left) >> 1) * revStep) / length;
				fadeRect.left = frame->left + val;
				fadeRect.right = frame->right - val;
				xOffset = fadeRect.left;
			} break;
			case ftype_centerFade:
			{
				s16 val = (((frame->bottom - frame->top) >> 1) * revStep) / length;
				fadeRect.top = frame->top + val;
				fadeRect.bottom = frame->bottom - val;
				yOffset = fadeRect.top;

				val = (((frame->right - frame->left) >> 1) * revStep) / length;
				fadeRect.left = frame->left + val;
				fadeRect.right = frame->right - val;
				xOffset = fadeRect.left;
			} break;
			case ftype_thinSliceFade:
			{
				wOffset = ((fadeRect.right - fadeRect.left) * step) / length;
				slice++;
				cut = 1;
			} break;
			case ftype_sliceFade:
			{
				wOffset = ((fadeRect.right - fadeRect.left) * step) / length;
				slice++;
				cut = 4;
			} break;
			case ftype_thickSliceFade:
			{
				wOffset = ((fadeRect.right - fadeRect.left) * step) / length;
				slice++;
				cut = 8;
			} break;
		}

		if (!lrect_isEmpty(&fadeRect))
		{
			if (type == ftype_noFade || type == ftype_snapFade || type == ftype_shortSnapFade || type == ftype_longSnapFade)
			{
				lcanvas_copyScreenToVideo(&fadeRect);
			}
			else if (slice)
			{
				// Code empty here...
			}
			else
			{
				lcanvas_copyPortionToVideo(&fadeRect, xOffset, yOffset);
			}
		}
	}

	static LRect s_fadeRect;
	static JBool s_fadeDialog;
	static s32 s_fadeTime;
	static s32 s_fadeLockTime = -1;
	static s32 s_fadeLockEnd = -1;
	static u16 s_baseRate;

	void lfade_loop(s32 time)
	{
		lfade_fadeToVideoPalette(time, s_fadeLockTime);

		if (lfade_isStep(s_fade->fullType, s_fade->fullLock, s_fade->fullDelay, time, s_fadeLockTime))
		{
			lfade_copyToVideo(&s_fadeRect, s_fade->fullType, s_fade->fullStep, s_fade->fullLen, time != s_fadeLockEnd);
			s_fade->fullStep++;
			if (s_fade->fullStep > s_fade->fullLen)
			{
				s_fade->fullType = ftype_noFade;
			}
		}
		else
		{
			if (lfade_isDelayStep(s_fade->fullType, s_fade->fullDelay, time, s_fadeLockTime))
			{
				s_fade->fullDelay--;
			}

			if (s_fadeDialog)
			{
				lfade_copyToVideo(&s_fadeRect, ftype_snapFade, 0, 0, JFALSE);
			}
			else
			{
				for (s32 i = 0; i < LVIEW_COUNT; i++)
				{
					if (lview_copyEnabled(i))
					{
						LRect frame;
						lview_getViewClipFrame(i, &frame);
						lrect_clip(&frame, &s_fadeRect);

						if (lfade_isStep(s_fade->type[i], s_fade->lock[i], s_fade->delay[i], time, s_fadeLockTime))
						{
							lfade_copyToVideo(&frame, s_fade->type[i], s_fade->step[i], s_fade->len[i], time != s_fadeLockEnd);
							s_fade->step[i]++;
							if (s_fade->step[i] > s_fade->len[i])
							{
								s_fade->type[i] = ftype_noFade;
							}
						}
						else
						{
							if (lfade_isDelayStep(s_fade->type[i], s_fade->delay[i], time, s_fadeLockTime))
							{
								s_fade->delay[i]--;
							}
							if (s_fade->type[i] == ftype_noFade)
							{
								lfade_copyToVideo(&frame, ftype_snapFade, 0, 0, time != s_fadeLockEnd);
							}
						}
					}
				}
			}
		}
	}
	
	JBool lfade_applyFadeLoop(LRect* rect, JBool dialog)
	{
		if (s_fadeLockTime < 0)
		{
			s_fadeRect = *rect;
			s_fadeDialog = dialog;
			lfade_calculateFadeLockTime(&s_fadeLockTime, &s_fadeLockEnd);
			s_fadeTime = s_fadeLockTime;
			s_baseRate = ltime_getDelay();
		}
		// We are not fading at all.
		if (s_fadeTime > s_fadeLockEnd)
		{
			return JTRUE;
		}
		// Fading is applied
		if (s_fadeTime <= s_fadeLockEnd)
		{
			lfade_loop(s_fadeTime);
			s_fadeTime++;

			if (s_fadeTime > s_fadeLockEnd)
			{
				s_fadeLockTime = -1;
				s_fadeLockEnd  = -1;
				s_fadeTime = 0;
				ltime_setFrameRate(s_baseRate);
				return JTRUE;
			}
			// Fading occurs at a higher framerate.
			ltime_setFrameRate(4);
		}
		return JFALSE;
	}
}