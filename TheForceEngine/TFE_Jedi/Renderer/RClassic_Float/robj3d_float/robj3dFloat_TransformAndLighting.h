#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
struct SecObject;

#define VSHADE_MAX_INTENSITY_FLT 31.0f

namespace TFE_Jedi
{
	namespace RClassic_Float
	{
		extern s32 s_enableFlatShading;
		// Vertex attributes transformed to viewspace.
		extern vec3_float s_verticesVS[MAX_VERTEX_COUNT_3DO];
		extern vec3_float s_vertexNormalsVS[MAX_VERTEX_COUNT_3DO];
		// Vertex Lighting.
		extern f32 s_vertexIntensity[MAX_VERTEX_COUNT_3DO];
		// Polygon normals in viewspace (used for culling).
		extern vec3_float s_polygonNormalsVS[MAX_POLYGON_COUNT_3DO];

		void robj3d_transformAndLight(SecObject* obj, JediModel* model);
	}
}
