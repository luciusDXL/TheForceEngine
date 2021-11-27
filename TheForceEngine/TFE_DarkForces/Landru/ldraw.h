#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru Palette
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "lrect.h"

namespace TFE_DarkForces
{
	void deltaImage(s16* data, s16 x, s16 y);
	void deltaClip(s16* data, s16 x, s16 y);
	void deltaFlip(s16* data, s16 x, s16 y, s16 w);
	void deltaFlipClip(s16* data, s16 x, s16 y, s16 w);
}  // namespace TFE_DarkForces