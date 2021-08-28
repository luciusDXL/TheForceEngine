#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Automap functionality
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

namespace TFE_DarkForces
{
	void automap_computeScreenBounds();

	extern JBool s_pdaActive;
	extern JBool s_drawAutomap;
}  // namespace TFE_DarkForces