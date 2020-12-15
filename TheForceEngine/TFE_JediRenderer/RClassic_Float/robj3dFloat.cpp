#include <TFE_System/profiler.h>
#include <TFE_RenderBackend/renderBackend.h>
// TODO: Fix or move.
#include <TFE_Game/level.h>

#include "robj3dFloat.h"
#include "rsectorFloat.h"
#include "../fixedPoint.h"
#include "../rmath.h"
#include "../rcommon.h"
#include "../robject.h"

namespace TFE_JediRenderer
{

namespace RClassic_Float
{
	static vec3_float s_verticesVS[MAX_VERTEX_COUNT_3DO];

	void rotateVectorM3x3(vec3_float* inVec, vec3_float* outVec, f32* mtx)
	{
		outVec->x = inVec->x*mtx[0] + inVec->y*mtx[3] + inVec->z*mtx[6];
		outVec->y = inVec->x*mtx[1] + inVec->y*mtx[4] + inVec->z*mtx[7];
		outVec->z = inVec->x*mtx[2] + inVec->y*mtx[5] + inVec->z*mtx[8];
	}

	void mulMatrix3x3(f32* mtx0, f32* mtx1, f32* mtxOut)
	{
		mtxOut[0] = mtx0[0]*mtx1[0] + mtx0[3]*mtx1[3] + mtx0[6]*mtx1[6];
		mtxOut[3] = mtx0[0]*mtx1[1] + mtx0[3]*mtx1[4] + mtx0[6]*mtx1[7];
		mtxOut[6] = mtx0[0]*mtx1[2] + mtx0[3]*mtx1[5] + mtx0[6]*mtx1[8];

		mtxOut[1] = mtx0[1]*mtx1[0] + mtx0[4]*mtx1[3] + mtx0[7]*mtx1[6];
		mtxOut[4] = mtx0[1]*mtx1[1] + mtx0[4]*mtx1[4] + mtx0[7]*mtx1[7];
		mtxOut[7] = mtx0[1]*mtx1[2] + mtx0[4]*mtx1[5] + mtx0[7]*mtx1[8];

		mtxOut[2] = mtx0[2]*mtx1[0] + mtx0[5]*mtx1[3] + mtx0[8]*mtx1[6];
		mtxOut[5] = mtx0[2]*mtx1[1] + mtx0[5]*mtx1[4] + mtx0[8]*mtx1[7];
		mtxOut[8] = mtx0[2]*mtx1[2] + mtx0[5]*mtx1[5] + mtx0[8]*mtx1[8];
	}

	void model_transformVertices(s32 vertexCount, vec3_fixed* vtxIn, f32* xform, vec3_float* offset, vec3_float* vtxOut)
	{
		for (s32 v = 0; v < vertexCount; v++, vtxOut++, vtxIn++)
		{
			// TODO: Cache floating point vertices.
			vec3_float vtxFlt = { fixed16ToFloat(vtxIn->x), fixed16ToFloat(vtxIn->y), fixed16ToFloat(vtxIn->z) };

			vtxOut->x = vtxFlt.x*xform[0] + vtxFlt.y*xform[3] + vtxFlt.z*xform[6] + offset->x;
			vtxOut->y = vtxFlt.x*xform[1] + vtxFlt.y*xform[4] + vtxFlt.z*xform[7] + offset->y;
			vtxOut->z = vtxFlt.x*xform[2] + vtxFlt.y*xform[5] + vtxFlt.z*xform[8] + offset->z;
		}
	}

	void model_drawVertices(s32 vertexCount, vec3_float* vertices, u8 color, s32 size)
	{
		// cannot draw if the color is transparent.
		if (color == 0) { return; }
		const s32 halfSize = (size > 1) ? (size >> 1) : 0;
		const s32 area = size * size;

		// Loop through the vertices and draw them as pixels.
		vec3_float* vertex = vertices;
		for (s32 v = 0; v < vertexCount; v++, vertex++)
		{
			f32 z = vertex->z;
			if (z <= 1.0f) { continue; }
			s32 pixel_x = roundFloat(vertex->x*s_focalLength/z + s_halfWidth);
			s32 pixel_y = roundFloat(vertex->y*s_focalLenAspect/z + s_halfHeight);

			// If the X position is out of view, skip the vertex.
			if (pixel_x < s_minScreenX || pixel_x > s_maxScreenX)
			{
				continue;
			}
			// Check the 1d depth buffer and Y positon and skip if occluded.
			if (z >= s_depth1d[pixel_x] || pixel_y > s_windowMaxY || pixel_y < s_windowMinY || pixel_y < s_windowTop[pixel_x] || pixel_y > s_windowBot[pixel_x])
			{
				continue;
			}

			for (s32 i = 0; i < area; i++)
			{
				const s32 x = clamp(pixel_x - halfSize + (i % size), s_minScreenX, s_maxScreenX);
				const s32 y = clamp(pixel_y - halfSize + (i / size), s_windowMinY, s_windowMaxY);
				s_display[y*s_width + x] = color;
			}
		}
	}

	void model_draw(SecObject* obj, JediModel* model)
	{
		//s_cameraMtx_Fixed
		RSector* sector = obj->sector;

		vec3_float offsetWS;
		offsetWS.x = obj->posWS.x.f32 - s_cameraPosX;
		offsetWS.y = obj->posWS.y.f32 - s_cameraPosY;
		offsetWS.z = obj->posWS.z.f32 - s_cameraPosZ;

		// Calculate the view space object camera offset.
		vec3_float offsetVS;
		rotateVectorM3x3(&offsetWS, &offsetVS, s_cameraMtx);

		// Concatenate the camera and object rotation matrices.
		f32 xform[9];
		mulMatrix3x3(s_cameraMtx, obj->transformFlt, xform);

		// Transform model vertices into view space.
		model_transformVertices(model->vertexCount, (vec3_fixed*)model->vertices, xform, &offsetVS, s_verticesVS);

		// DEBUG: Draw all models as points.
		//if (model->flags & MFLAG_DRAW_VERTICES)
		{
			// Scale the points based on the resolution ratio.
			u32 height = TFE_RenderBackend::getVirtualDisplayHeight();
			s32 scale = (s32)max(1, height / 200);

			// If the MFLAG_DRAW_VERTICES flag is set, draw all vertices as points. 
			model_drawVertices(model->vertexCount, s_verticesVS, model->polygons[0].color, scale);
			return;
		}

		// TODO.
	}

}}  // TFE_JediRenderer