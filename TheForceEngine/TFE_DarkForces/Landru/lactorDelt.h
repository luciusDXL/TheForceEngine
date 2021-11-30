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
	void lactorDelt_init();
	void lactorDelt_destroy();

	void lactorDelt_getFrame(LActor* actor, LRect* rect);

	LActor* lactorDelt_alloc(u8* delta,LRect* frame, s16 xOffset, s16 yOffset, s16 zPlane);
	LActor* lactorDelt_load(const char* name, LRect* rect, s16 x, s16 y, s16 zPlane);

	// Draw
	JBool lactorDelt_draw(LActor* actor, LRect* rect, LRect* clipRect, s16 x, s16 y, JBool refresh);
	JBool lactorDelt_drawClipped(u8* data, s16 x, s16 y, JBool dirty);
	JBool lactorDelt_drawFlippedClipped(LActor* actor, u8* data, s16 x, s16 y, JBool dirty);
}  // namespace TFE_DarkForces