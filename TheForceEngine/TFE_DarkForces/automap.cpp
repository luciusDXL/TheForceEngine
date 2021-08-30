#include "automap.h"
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	static fixed16_16 s_screenScale = 0xc000;	// 0.75
	static fixed16_16 s_scrLeftScaled;
	static fixed16_16 s_scrRightScaled;
	static fixed16_16 s_scrTopScaled;
	static fixed16_16 s_scrBotScaled;

	static fixed16_16 s_mapXCenterInPixels = 159;
	static fixed16_16 s_mapZCenterInPixels = 99;
	static fixed16_16 s_mapX0;
	static fixed16_16 s_mapX1;
	static fixed16_16 s_mapZ0;
	static fixed16_16 s_mapZ1;

	JBool s_pdaActive = JFALSE;
	JBool s_drawAutomap = JFALSE;

	// _computeScreenBounds() and computeScaledScreenBounds() in the original source:
	// computeScaledScreenBounds() calls _computeScreenBounds() - so merged here.
	void automap_computeScreenBounds()
	{
		if (s_pdaActive)
		{
			// STUB
		}
		else
		{
			s32 leftEdge = intToFixed16(s_screenRect.left - 159);
			s_scrLeftScaled = div16(leftEdge, s_screenScale);

			s32 rightEdge = intToFixed16(s_screenRect.right - 159);
			s_scrRightScaled = div16(rightEdge, s_screenScale);

			s32 topEdge = intToFixed16(s_screenRect.top - 99);
			s_scrTopScaled = -div16(topEdge, s_screenScale);

			s32 botEdge = intToFixed16(s_screenRect.bot - 99);
			s_scrBotScaled = -div16(botEdge, s_screenScale);
		}
	}

	void automap_updateMapData(MapUpdateID id)
	{
		// TODO(Core Game Loop Release)
	}

	void automap_draw()
	{
		s_pdaActive = JFALSE;
		s_mapXCenterInPixels = 159;
		s_mapZCenterInPixels = 99;
		// automap_drawSectors();
	}
}  // namespace TFE_DarkForces