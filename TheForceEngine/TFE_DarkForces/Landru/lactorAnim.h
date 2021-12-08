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
	void lactorAnim_init();
	void lactorAnim_destroy();

	LActor* lactorAnim_alloc(u8** array, LRect* frame, s16 xOffset, s16 yOffset, s16 zPlane);
	LActor* lactorAnim_load(const char* name, LRect* rect, s16 x, s16 y, s16 zPlane);
	JBool   lactorAnim_draw(LActor* actor, LRect* rect, LRect* clipRect, s16 x, s16 y, JBool refresh);
	void    lactorAnim_getFrame(LActor* actor, LRect* rect);
}  // namespace TFE_DarkForces