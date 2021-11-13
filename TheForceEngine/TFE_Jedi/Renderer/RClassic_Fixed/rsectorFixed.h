#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include "rwallFixed.h"
#include "../rsectorRender.h"

struct RWall;
struct SecObject;

namespace TFE_Jedi
{
	class TFE_Sectors_Fixed : public TFE_Sectors
	{
	public:
		// Sub-Renderer specific
		void reset() override;
		void prepare() override;
		void draw(RSector* sector) override;
		void subrendererChanged() override;

	private:
		void saveValues(s32 index);
		void restoreValues(s32 index);
		void adjoin_computeWindowBounds(EdgePairFixed* adjoinEdges);
		void adjoin_setupAdjoinWindow(s32* winBot, s32* winBotNext, s32* winTop, s32* winTopNext, EdgePairFixed* adjoinEdges, s32 adjoinCount);
	};
}  // TFE_Jedi
