#include <TFE_System/profiler.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include "robj3dFixed_Culling.h"
#include "robj3dFixed_TransformAndLighting.h"
#include "../rcommonFixed.h"
#include "../../rcommon.h"

namespace TFE_Jedi
{

namespace RClassic_Fixed
{
	enum
	{
		POLYGON_FRONT_FACING = 0,
		POLYGON_BACK_FACING,
	};

	// List of potentially visible polygons (after backface culling).
	Polygon* s_visPolygons[MAX_POLYGON_COUNT_3DO];

	s32 getPolygonFacing(const vec3_fixed* normal, const vec3_fixed* pos)
	{
		const vec3_fixed offset = { -pos->x, -pos->y, -pos->z };
		return dot(normal, &offset) < 0 ? POLYGON_BACK_FACING : POLYGON_FRONT_FACING;
	}

	s32 robj3d_backfaceCull(JediModel* model)
	{
		vec3_fixed* polygonNormal = s_polygonNormalsVS;
		s32 polygonCount = model->polygonCount;
		Polygon* polygon = model->polygons;
		for (s32 i = 0; i < polygonCount; i++, polygonNormal++, polygon++)
		{
			vec3_fixed* vertex = &s_verticesVS[polygon->indices[1]];
			polygonNormal->x -= vertex->x;
			polygonNormal->y -= vertex->y;
			polygonNormal->z -= vertex->z;
		}

		Polygon** visPolygon = s_visPolygons;
		s32 visPolygonCount = 0;

		polygon = model->polygons;
		polygonNormal = s_polygonNormalsVS;
		for (s32 i = 0; i < model->polygonCount; i++, polygon++, polygonNormal++)
		{
			vec3_fixed* pos = &s_verticesVS[polygon->indices[1]];
			s32 facing = getPolygonFacing(polygonNormal, pos);
			if (facing == POLYGON_BACK_FACING) { continue; }

			visPolygonCount++;
			s32 vertexCount = polygon->vertexCount;
			s32 zAve = 0;

			s32* indices = polygon->indices;
			for (s32 v = 0; v < vertexCount; v++)
			{
				zAve += s_verticesVS[indices[v]].z;
			}

			polygon->zAve = div16(zAve, intToFixed16(vertexCount));
			*visPolygon = polygon;
			visPolygon++;
		}

		return visPolygonCount;
	}

}}  // TFE_Jedi