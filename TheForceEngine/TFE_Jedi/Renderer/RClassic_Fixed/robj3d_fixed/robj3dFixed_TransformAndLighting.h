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

#define VSHADE_MAX_INTENSITY (31 * ONE_16)

namespace TFE_Jedi
{
	namespace RClassic_Fixed
	{
		extern s32 s_enableFlatShading;
		// Vertex attributes transformed to viewspace.
		extern std::vector<vec3_fixed> s_verticesVS;
		extern std::vector<vec3_fixed> s_vertexNormalsVS;
		// Vertex Lighting.
		extern std::vector<fixed16_16> s_vertexIntensity;
		// Polygon normals in viewspace (used for culling).
		extern std::vector<vec3_fixed> s_polygonNormalsVS;

		void robj3d_transformAndLight(SecObject* obj, JediModel* model);
	}
}
