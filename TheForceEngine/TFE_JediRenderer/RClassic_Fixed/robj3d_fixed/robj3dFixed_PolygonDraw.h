#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/modelAsset_Jedi.h>
#include <TFE_Level/robject.h>
#include <TFE_Level/fixedPoint.h>
#include <TFE_Level/core_math.h>
#include "../../rlimits.h"

namespace TFE_JediRenderer
{
	namespace RClassic_Fixed
	{
		void robj3d_drawPolygon(Polygon* polygon, s32 polyVertexCount, SecObject* obj, JediModel* model);
	}
}
