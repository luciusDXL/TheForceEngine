#include <TFE_System/profiler.h>
#include <TFE_Settings/settings.h>

#include "robj3dFloat_PolygonSetup.h"
#include "robj3dFloat_TransformAndLighting.h"
#include "../../fixedPoint.h"
#include "../../rmath.h"

namespace TFE_JediRenderer
{

namespace RClassic_Float
{
	vec3_float s_polygonVerticesVS[POLY_MAX_VTX_COUNT];
	vec3_float s_polygonVerticesProj[POLY_MAX_VTX_COUNT];
	vec2_float s_polygonUv[POLY_MAX_VTX_COUNT];
	f32 s_polygonIntensity[POLY_MAX_VTX_COUNT];

	void robj3d_setupPolygon(Polygon* polygon)
	{
		// Copy polygon vertices.
		for (s32 v = 0; v < polygon->vertexCount; v++)
		{
			s_polygonVerticesVS[v] = s_verticesVS[polygon->indices[v]];
		}

		// Copy uvs if required.
		if (polygon->shading & PSHADE_TEXTURE)
		{
			const vec2_fixed* uv = (vec2_fixed*)polygon->uv;
			if (uv)
			{
				for (s32 v = 0; v < polygon->vertexCount; v++)
				{
					s_polygonUv[v].x = fixed16ToFloat(uv[v].x);
					s_polygonUv[v].z = fixed16ToFloat(uv[v].z);
				}
			}
			else
			{
				vec2_float zero = { 0 };
				for (s32 v = 0; v < polygon->vertexCount; v++)
				{
					s_polygonUv[v] = zero;
				}
			}
		}

		// Copy intensities if required.
		if (polygon->shading & PSHADE_GOURAUD)
		{
			const s32* indices = polygon->indices;
			for (s32 v = 0; v < polygon->vertexCount; v++)
			{
				s_polygonIntensity[v] = s_vertexIntensity[indices[v]];
			}
		}
	}

}}  // TFE_JediRenderer