#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru Timer
// Timer used for updating Landru loops.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

namespace TFE_DarkForces
{
	//#define LTICKS_PER_SECOND 291.31
	#define LTICKS_PER_SECOND 309.52
	typedef u32 LTick;	// Landru tick = 291.3 / second.

	// TFE Specific - this replaces the timer interrupt.
	void tfe_updateLTime();

	// Landru timer API.
	void ltime_init();
	void ltime_setFrameRate(u16 delay);
	u16  ltime_getDelay();
	LTick ltime_elapsed();
	JBool ltime_checkTimeElapsed();
	JBool ltime_often();
	LTick ltime_curTick();

	// This is also altered from DOS to return JTRUE when the frame is ready instead of 
	// looping.
	JBool ltime_isFrameReady();
}  // namespace TFE_DarkForces