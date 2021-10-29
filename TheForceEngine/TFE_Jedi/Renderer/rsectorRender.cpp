#include "rsectorRender.h"
#include "redgePair.h"
#include <TFE_Jedi/Level/robject.h>
#include "rcommon.h"

namespace TFE_Jedi
{
	void TFE_Sectors::computeAdjoinWindowBounds(EdgePair* adjoinEdges)
	{
		s32 yC = adjoinEdges->yPixel_C0;
		if (yC > s_windowMinY_Pixels)
		{
			s_windowMinY_Pixels = yC;
		}
		s32 yF = adjoinEdges->yPixel_F0;
		if (yF < s_windowMaxY_Pixels)
		{
			s_windowMaxY_Pixels = yF;
		}
		yC = adjoinEdges->yPixel_C1;
		if (yC > s_windowMaxCeil)
		{
			s_windowMaxCeil = yC;
		}
		yF = adjoinEdges->yPixel_F1;
		if (yF < s_windowMinFloor)
		{
			s_windowMinFloor = yF;
		}
		s_wallMaxCeilY = s_windowMinY_Pixels - 1;
		s_wallMinFloorY = s_windowMaxY_Pixels + 1;
		s_windowMinX = adjoinEdges->x0;
		s_windowMaxX = adjoinEdges->x1;
		s_windowTopPrev = s_windowTop;
		s_windowBotPrev = s_windowBot;
		s_prevSector = s_curSector;
	}
}  // TFE_Jedi
