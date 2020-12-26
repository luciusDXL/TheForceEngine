#include <TFE_System/profiler.h>

#include "robj3dFixed_PolygonSetup.h"
#include "robj3dFixed_TransformAndLighting.h"
#include "../rcommonFixed.h"
#include "../../fixedPoint.h"
#include "../../rmath.h"

namespace TFE_JediRenderer
{

namespace RClassic_Fixed
{
	vec3_fixed s_polygonVerticesVS[POLY_MAX_VTX_COUNT];
	vec3_fixed s_polygonVerticesProj[POLY_MAX_VTX_COUNT];
	vec2_fixed s_polygonUv[POLY_MAX_VTX_COUNT];
	fixed16_16 s_polygonIntensity[POLY_MAX_VTX_COUNT];

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
			for (s32 v = 0; v < polygon->vertexCount; v++)
			{
				s_polygonUv[v] = uv[v];
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