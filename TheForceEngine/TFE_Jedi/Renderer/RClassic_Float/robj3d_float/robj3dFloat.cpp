#include <TFE_System/profiler.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>

#include "robj3dFloat.h"
#include "robj3dFloat_TransformAndLighting.h"
#include "robj3dFloat_PolygonSetup.h"
#include "robj3dFloat_Culling.h"
#include "robj3dFloat_Clipping.h"
#include "robj3dFloat_PolygonDraw.h"
#include "../rclassicFloatSharedState.h"
#include "../../rcommon.h"

namespace TFE_Jedi
{
extern s32 s_drawnObjCount;
extern SecObject* s_drawnObj[];

namespace RClassic_Float
{
	void robj3d_projectVertices(vec3_float* pos, s32 count, vec3_float* out);
	void robj3d_drawVertices(s32 vertexCount, const vec3_float* vertices, u8 color, s32 size);
	s32 polygonSort(const void* r0, const void* r1);

	void robj3d_draw(SecObject* obj, JediModel* model)
	{
		// Handle transforms and vertex lighting.
		robj3d_transformAndLight(obj, model);

		// Draw vertices and return if the flag is set.
		if (model->flags & MFLAG_DRAW_VERTICES)
		{
			// Scale the points based on the resolution ratio.
			u32 width, height;
			vfb_getResolution(&width, &height);
			const s32 scale = max(1, (s32)(height / 200));

			// If the MFLAG_DRAW_VERTICES flag is set, draw all vertices as points. 
			robj3d_drawVertices(model->vertexCount, s_verticesVS, model->polygons[0].color, scale);
			return;
		}

		// Cull backfacing polygons. The results are stored as "visPolygons"
		s32 visPolygonCount = robj3d_backfaceCull(model);
		// Nothing to render.
		if (visPolygonCount < 1) { return; }

		// Sort polygons from back to front.
		qsort(s_visPolygons, visPolygonCount, sizeof(JmPolygon*), polygonSort);

		// Draw polygons
		JmPolygon** visPolygon = s_visPolygons;
		JBool drawn = JFALSE;
		for (s32 i = 0; i < visPolygonCount; i++, visPolygon++)
		{
			JmPolygon* polygon = *visPolygon;
			if (polygon->vertexCount <= 0) { continue; }

			robj3d_setupPolygon(polygon);

			s32 polyVertexCount = clipPolygon(polygon);
			// Cull the polygon if not enough vertices survive clipping.
			if (polyVertexCount < 3) { continue; }
			drawn = JTRUE;

			// Project the resulting vertices.
			robj3d_projectVertices(s_polygonVerticesVS, polyVertexCount, s_polygonVerticesProj);

			// Draw polygon based on its shading mode.
			robj3d_drawPolygon(polygon, polyVertexCount, obj, model);
		}

		if (drawn && s_drawnObjCount < MAX_DRAWN_OBJ_STORE)
		{
			s_drawnObj[s_drawnObjCount++] = obj;
		}
	}
		
	void robj3d_drawVertices(s32 vertexCount, const vec3_float* vertices, u8 color, s32 size)
	{
		// cannot draw if the color is transparent.
		if (color == 0) { return; }
		const s32 halfSize = (size > 1) ? (size >> 1) : 0;
		const s32 area = size * size;

		// Loop through the vertices and draw them as pixels.
		const vec3_float* vertex = vertices;
		for (s32 v = 0; v < vertexCount; v++, vertex++)
		{
			const f32 z = vertex->z;
			if (z <= 1.0f) { continue; }

			const s32 pixel_x = roundFloat((vertex->x*s_rcfltState.focalLength)    / z + s_rcfltState.projOffsetX);
			const s32 pixel_y = roundFloat((vertex->y*s_rcfltState.focalLenAspect) / z + s_rcfltState.projOffsetY);

			// If the X position is out of view, skip the vertex.
			if (pixel_x < s_minScreenX_Pixels || pixel_x > s_maxScreenX_Pixels)
			{
				continue;
			}
			// Check the 1d depth buffer and Y positon and skip if occluded.
			if (z >= s_rcfltState.depth1d[pixel_x] || pixel_y > s_windowMaxY_Pixels || pixel_y < s_windowMinY_Pixels || pixel_y < s_windowTop[pixel_x] || pixel_y > s_windowBot[pixel_x])
			{
				continue;
			}

			for (s32 i = 0; i < area; i++)
			{
				const s32 x = clamp(pixel_x - halfSize + (i % size), s_minScreenX_Pixels, s_maxScreenX_Pixels);
				const s32 y = clamp(pixel_y - halfSize + (i / size), s_windowMinY_Pixels, s_windowMaxY_Pixels);
				s_display[y*s_width + x] = color;
			}
		}
	}

	void robj3d_projectVertices(vec3_float* pos, s32 count, vec3_float* out)
	{
		for (s32 i = 0; i < count; i++, pos++, out++)
		{
			const f32 rcpZ = 1.0f / pos->z;

			out->x = (f32)roundFloat((pos->x*s_rcfltState.focalLength)   *rcpZ + s_rcfltState.projOffsetX);
			out->y = (f32)roundFloat((pos->y*s_rcfltState.focalLenAspect)*rcpZ + s_rcfltState.projOffsetY);
			out->z = pos->z;
		}
	}

	s32 polygonSort(const void* r0, const void* r1)
	{
		JmPolygon* p0 = *((JmPolygon**)r0);
		JmPolygon* p1 = *((JmPolygon**)r1);
		return signZero(p1->zAvef - p0->zAvef);
	}

}}  // TFE_Jedi