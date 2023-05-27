#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/modelAsset_jedi.h>

namespace TFE_Jedi
{
	namespace RClassic_Fixed
	{
		extern std::vector<JmPolygon*> s_visPolygons;
		s32 robj3d_backfaceCull(JediModel* model);
	}
}
