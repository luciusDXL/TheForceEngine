#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/modelAsset_Jedi.h>
#include "../../rmath.h"
#include "../../rlimits.h"
#include "../../robject.h"

namespace TFE_JediRenderer
{
	namespace RClassic_Float
	{
		void robj3d_drawPolygon(Polygon* polygon, s32 polyVertexCount, SecObject* obj, JediModel* model);
	}
}
