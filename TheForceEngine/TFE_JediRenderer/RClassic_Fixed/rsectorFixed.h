#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Level/fixedPoint.h>
#include <TFE_Level/core_math.h>
#include "rwallFixed.h"
#include "../rsectorRender.h"

struct RWall;
struct SecObject;

namespace TFE_JediRenderer
{
	class TFE_Sectors_Fixed : public TFE_Sectors
	{
	public:
		// Sub-Renderer specific
		void draw(RSector* sector) override;
		void subrendererChanged() override;

	private:
		void saveValues(s32 index);
		void restoreValues(s32 index);
		void adjoin_computeWindowBounds(EdgePair* adjoinEdges);
		void adjoin_setupAdjoinWindow(s32* winBot, s32* winBotNext, s32* winTop, s32* winTopNext, EdgePair* adjoinEdges, s32 adjoinCount);
	};

	extern RClassic_Fixed::RWallSegment s_wallSegListDst[MAX_SEG];
	extern RClassic_Fixed::RWallSegment s_wallSegListSrc[MAX_SEG];
	extern RClassic_Fixed::RWallSegment** s_adjoinSegment;
}  // TFE_JediRenderer
