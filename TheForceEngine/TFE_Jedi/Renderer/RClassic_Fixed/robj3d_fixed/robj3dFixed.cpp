#include <TFE_System/profiler.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include "robj3dFixed.h"
#include "robj3dFixed_TransformAndLighting.h"
#include "robj3dFixed_PolygonSetup.h"
#include "robj3dFixed_Culling.h"
#include "robj3dFixed_Clipping.h"
#include "robj3dFixed_PolygonDraw.h"
#include "../rclassicFixedSharedState.h"
#include "../../rcommon.h"

namespace TFE_Jedi
{

namespace RClassic_Fixed
{
	void robj3d_projectVertices(vec3_fixed* pos, s32 count, vec3_fixed* out);
	void robj3d_drawVertices(s32 vertexCount, const vec3_fixed* vertices, u8 color);
	s32 polygonSort(const void* r0, const void* r1);

	void robj3d_draw(SecObject* obj, JediModel* model)
	{
		// Handle transforms and vertex lighting.
		robj3d_transformAndLight(obj, model);

		// Draw vertices and return if the flag is set.
		if (model->flags & MFLAG_DRAW_VERTICES)
		{
			// If the MFLAG_DRAW_VERTICES flag is set, draw all vertices as points. 
			robj3d_drawVertices(model->vertexCount, s_verticesVS, model->polygons[0].color);
			return;
		}

		// Cull backfacing polygons. The results are stored as "visPolygons"
		s32 visPolygonCount = robj3d_backfaceCull(model);
		// Nothing to render.
		if (visPolygonCount < 1) { return; }

		// Sort polygons from back to front.
		qsort(s_visPolygons, visPolygonCount, sizeof(Polygon*), polygonSort);

		// Draw polygons
		Polygon** visPolygon = s_visPolygons;
		for (s32 i = 0; i < visPolygonCount; i++, visPolygon++)
		{
			Polygon* polygon = *visPolygon;
			if (polygon->vertexCount <= 0) { continue; }

			robj3d_setupPolygon(polygon);

			s32 polyVertexCount = clipPolygon(polygon);
			// Cull the polygon if not enough vertices survive clipping.
			if (polyVertexCount < 3) { continue; }

			// Project the resulting vertices.
			robj3d_projectVertices(s_polygonVerticesVS, polyVertexCount, s_polygonVerticesProj);

			// Draw polygon based on its shading mode.
			robj3d_drawPolygon(polygon, polyVertexCount, obj, model);
		}
	}
		
	void robj3d_drawVertices(s32 vertexCount, const vec3_fixed* vertices, u8 color)
	{
		// cannot draw if the color is transparent.
		if (color == 0) { return; }

		// Loop through the vertices and draw them as pixels.
		const vec3_fixed* vertex = vertices;
		for (s32 v = 0; v < vertexCount; v++, vertex++)
		{
			const fixed16_16 z = vertex->z;
			if (z <= ONE_16) { continue; }

			const s32 pixel_x = round16(div16(mul16(vertex->x, s_rcfState.focalLength),    z) + s_rcfState.projOffsetX);
			const s32 pixel_y = round16(div16(mul16(vertex->y, s_rcfState.focalLenAspect), z) + s_rcfState.projOffsetY);

			// If the X position is out of view, skip the vertex.
			if (pixel_x < s_minScreenX || pixel_x > s_maxScreenX)
			{
				continue;
			}
			// Check the 1d depth buffer and Y positon and skip if occluded.
			if (z >= s_rcfState.depth1d[pixel_x] || pixel_y > s_windowMaxY_Pixels || pixel_y < s_windowMinY_Pixels || pixel_y < s_windowTop[pixel_x] || pixel_y > s_windowBot[pixel_x])
			{
				continue;
			}

			s_display[pixel_y*s_width + pixel_x] = color;
		}
	}

	void robj3d_projectVertices(vec3_fixed* pos, s32 count, vec3_fixed* out)
	{
		for (s32 i = 0; i < count; i++, pos++, out++)
		{
			out->x = round16(div16(mul16(pos->x, s_rcfState.focalLength),    pos->z) + s_rcfState.projOffsetX);
			out->y = round16(div16(mul16(pos->y, s_rcfState.focalLenAspect), pos->z) + s_rcfState.projOffsetY);
			out->z = pos->z;
		}
	}

	s32 polygonSort(const void* r0, const void* r1)
	{
		Polygon* p0 = *((Polygon**)r0);
		Polygon* p1 = *((Polygon**)r1);
		return p1->zAve - p0->zAve;
	}

}}  // TFE_Jedi