#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru Timer
// Timer used for updating Landru loops.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

namespace TFE_DarkForces
{
	#define LTICKS_PER_SECOND 240
	typedef u32 LTick;	// Landru tick = 240 / second.

	// TFE Specific - this replaces the timer interrupt.
	void tfe_updateLTime();

	// Landru timer API.
	void ltime_setFrameRate(u16 delay);
	LTick ltime_elapsed();
	JBool ltime_checkTimeElapsed();
	JBool ltime_often();

	// This is also altered from DOS to return JTRUE when the frame is ready instead of 
	// looping.
	JBool ltime_isFrameReady();
}  // namespace TFE_DarkForces