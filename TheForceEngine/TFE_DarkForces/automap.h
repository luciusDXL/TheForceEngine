#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Automap functionality
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_FileSystem/stream.h>

namespace TFE_DarkForces
{
	enum MapUpdateID
	{
		MAP_CENTER_PLAYER      = 1,
		MAP_MOVE1_UP           = 2,
		MAP_MOVE1_DN           = 3,
		MAP_MOVE1_LEFT         = 4,
		MAP_MOVE1_RIGHT        = 5,
		MAP_ZOOM_IN            = 6,
		MAP_ZOOM_OUT           = 7,
		MAP_LAYER_UP           = 8,
		MAP_LAYER_DOWN         = 9,
		MAP_ENABLE_AUTOCENTER  = 10,
		MAP_DISABLE_AUTOCENTER = 11,
		MAP_TOGGLE_ALL_LAYERS  = 12,
		MAP_INCR_SECTOR_MODE   = 13,
		MAP_TELEPORT           = 14,
		MAP_MAX                = 14,
	};

	void automap_serialize(Stream* stream);

	void automap_computeScreenBounds();
	void automap_updateMapData(MapUpdateID id);
	void automap_resetScale();
	void automap_draw(u8* framebuffer);
	void automap_setPdaActive(JBool enable);
	void automap_updateDeltaCoords(s32 x, s32 z);

	s32  automap_getLayer();
	void automap_setLayer(s32 layer);
	void automap_disableLock();
	void automap_enableLock();

	extern JBool s_pdaActive;
	extern JBool s_drawAutomap;
	extern JBool s_automapLocked;
	extern TFE_Jedi::fixed16_16 s_mapX0;
	extern TFE_Jedi::fixed16_16 s_mapX1;
	extern TFE_Jedi::fixed16_16 s_mapZ0;
	extern TFE_Jedi::fixed16_16 s_mapZ1;
	extern s32 s_mapLayer;
}  // namespace TFE_DarkForces