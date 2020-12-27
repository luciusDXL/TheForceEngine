#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/modelAsset_Jedi.h>
#include "../../rmath.h"

// This is 32 in the original code but that causes overflow in Detention Center, which either crashes
// in the level or on exit when using the float-point sub-renderer.
#define POLY_MAX_VTX_COUNT 64

namespace TFE_JediRenderer
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
