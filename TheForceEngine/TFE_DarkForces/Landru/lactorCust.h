#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru Actor
// Landru was the cutscene system developed for XWing and Tie Fighter
// and is also used in Dark Forces.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "lactor.h"

namespace TFE_DarkForces
{
	void lactorCust_init();
	void lactorCust_destroy();

	LActor* lactorCust_alloc(u8* custom, LRect* frame, s16 xOffset, s16 yOffset, s16 zPlane);
}  // namespace TFE_DarkForces