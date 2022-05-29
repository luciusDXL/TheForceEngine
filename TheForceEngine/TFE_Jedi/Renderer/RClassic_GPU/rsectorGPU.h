#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include "../rsectorRender.h"

namespace TFE_Jedi
{
	class TFE_Sectors_GPU : public TFE_Sectors
	{
	public:
		TFE_Sectors_GPU() {}

		// Sub-Renderer specific
		void reset() override;
		void prepare() override;
		void draw(RSector* sector) override;
		void subrendererChanged() override;

	private:

	public:
	};
}  // TFE_Jedi
