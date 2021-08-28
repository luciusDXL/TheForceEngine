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
}  // namespace TFE_DarkForces