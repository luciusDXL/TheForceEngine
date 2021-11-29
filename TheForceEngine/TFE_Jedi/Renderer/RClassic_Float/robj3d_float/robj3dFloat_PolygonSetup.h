#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#define POLY_MAX_VTX_COUNT 32

namespace TFE_Jedi
{
	namespace RClassic_Float
	{
		extern vec3_float s_polygonVerticesVS[POLY_MAX_VTX_COUNT];
		extern vec3_float s_polygonVerticesProj[POLY_MAX_VTX_COUNT];
		extern vec2_float s_polygonUv[POLY_MAX_VTX_COUNT];
		extern f32 s_polygonIntensity[POLY_MAX_VTX_COUNT];

		void robj3d_setupPolygon(Polygon* polygon);
	}
}
