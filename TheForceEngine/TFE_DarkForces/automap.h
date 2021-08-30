#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Automap functionality
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

namespace TFE_DarkForces
{
	enum MapUpdateID
	{
		MAP_NORMAL      = 1,
		MAP_MOVE1_UP    = 2,
		MAP_MOVE1_DN    = 3,
		MAP_MOVE1_LEFT  = 4,
		MAP_MOVE1_RIGHT = 5,
		MAP_ZOOM_IN     = 6,
		MAP_ZOOM_OUT    = 7,
		MAP_LAYER_UP    = 8,
		MAP_LAYER_DOWN  = 9,
		MAP_MAX         = 14,
	};

	void automap_computeScreenBounds();
	void automap_updateMapData(MapUpdateID id);

	extern JBool s_pdaActive;
	extern JBool s_drawAutomap;
}  // namespace TFE_DarkForces