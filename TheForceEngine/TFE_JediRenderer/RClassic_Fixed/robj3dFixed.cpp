#include <TFE_System/profiler.h>
// TODO: Fix or move.
#include <TFE_Game/level.h>

#include "robj3dFixed.h"
#include "rsectorFixed.h"
#include "rcommonFixed.h"
#include "../fixedPoint.h"
#include "../rmath.h"
#include "../rcommon.h"
#include "../robject.h"

namespace TFE_JediRenderer
{

namespace RClassic_Fixed
{
	static vec3_fixed s_verticesVS[MAX_VERTEX_COUNT_3DO];

	void rotateVectorM3x3(vec3_fixed* inVec, vec3_fixed* outVec, s32* mtx)
	{
		outVec->x = mul16(inVec->x, mtx[0]) + mul16(inVec->y, mtx[3]) + mul16(inVec->z, mtx[6]);
		outVec->y = mul16(inVec->x, mtx[1]) + mul16(inVec->y, mtx[4]) + mul16(inVec->z, mtx[7]);
		outVec->z = mul16(inVec->x, mtx[2]) + mul16(inVec->y, mtx[5]) + mul16(inVec->z, mtx[8]);
	}

	void mulMatrix3x3(s32* mtx0, s32* mtx1, s32* mtxOut)
	{
		mtxOut[0] = mul16(mtx0[0], mtx1[0]) + mul16(mtx0[3], mtx1[3]) + mul16(mtx0[6], mtx1[6]);
		mtxOut[3] = mul16(mtx0[0], mtx1[1]) + mul16(mtx0[3], mtx1[4]) + mul16(mtx0[6], mtx1[7]);
		mtxOut[6] = mul16(mtx0[0], mtx1[2]) + mul16(mtx0[3], mtx1[5]) + mul16(mtx0[6], mtx1[8]);

		mtxOut[1] = mul16(mtx0[1], mtx1[0]) + mul16(mtx0[4], mtx1[3]) + mul16(mtx0[7], mtx1[6]);
		mtxOut[4] = mul16(mtx0[1], mtx1[1]) + mul16(mtx0[4], mtx1[4]) + mul16(mtx0[7], mtx1[7]);
		mtxOut[7] = mul16(mtx0[1], mtx1[2]) + mul16(mtx0[4], mtx1[5]) + mul16(mtx0[7], mtx1[8]);

		mtxOut[2] = mul16(mtx0[2], mtx1[0]) + mul16(mtx0[5], mtx1[3]) + mul16(mtx0[8], mtx1[6]);
		mtxOut[5] = mul16(mtx0[2], mtx1[1]) + mul16(mtx0[5], mtx1[4]) + mul16(mtx0[8], mtx1[7]);
		mtxOut[8] = mul16(mtx0[2], mtx1[2]) + mul16(mtx0[5], mtx1[5]) + mul16(mtx0[8], mtx1[8]);
	}

	void model_transformVertices(s32 vertexCount, vec3_fixed* vtxIn, s32* xform, vec3_fixed* offset, vec3_fixed* vtxOut)
	{
		for (s32 v = 0; v < vertexCount; v++, vtxOut++, vtxIn++)
		{
			vtxOut->x = mul16(vtxIn->x, xform[0]) + mul16(vtxIn->y, xform[3]) + mul16(vtxIn->z, xform[6]) + offset->x;
			vtxOut->y = mul16(vtxIn->x, xform[1]) + mul16(vtxIn->y, xform[4]) + mul16(vtxIn->z, xform[7]) + offset->y;
			vtxOut->z = mul16(vtxIn->x, xform[2]) + mul16(vtxIn->y, xform[5]) + mul16(vtxIn->z, xform[8]) + offset->z;
		}
	}

	void model_drawVertices(s32 vertexCount, vec3_fixed* vertices, u8 color)
	{
		// cannot draw if the color is transparent.
		if (color == 0) { return; }

		// Loop through the vertices and draw them as pixels.
		vec3_fixed* vertex = vertices;
		for (s32 v = 0; v < vertexCount; v++, vertex++)
		{
			fixed16_16 z = vertex->z;
			if (z <= ONE_16) { continue; }
			s32 pixel_x = round16(div16(mul16(vertex->x, s_focalLength_Fixed), z) + s_halfWidth_Fixed);
			s32 pixel_y = round16(div16(mul16(vertex->y, s_focalLenAspect_Fixed), z) + s_halfHeight_Fixed);

			// If the X position is out of view, skip the vertex.
			if (pixel_x < s_minScreenX || pixel_x > s_maxScreenX)
			{
				continue;
			}
			// Check the 1d depth buffer and Y positon and skip if occluded.
			if (z >= s_depth1d_Fixed[pixel_x] || pixel_y > s_windowMaxY || pixel_y < s_windowMinY || pixel_y < s_windowTop[pixel_x] || pixel_y > s_windowBot[pixel_x])
			{
				continue;
			}

			s_display[pixel_y*s_width + pixel_x] = color;
		}
	}

	void model_draw(SecObject* obj, JediModel* model)
	{
		//s_cameraMtx_Fixed
		RSector* sector = obj->sector;

		vec3_fixed offsetWS;
		offsetWS.x = obj->posWS.x.f16_16 - s_cameraPosX_Fixed;
		offsetWS.y = obj->posWS.y.f16_16 - s_cameraPosY_Fixed;
		offsetWS.z = obj->posWS.z.f16_16 - s_cameraPosZ_Fixed;

		// Calculate the view space object camera offset.
		vec3_fixed offsetVS;
		rotateVectorM3x3(&offsetWS, &offsetVS, s_cameraMtx_Fixed);

		// Concatenate the camera and object rotation matrices.
		fixed16_16 xform[9];
		mulMatrix3x3(s_cameraMtx_Fixed, obj->transform, xform);

		// Transform model vertices into view space.
		model_transformVertices(model->vertexCount, (vec3_fixed*)model->vertices, xform, &offsetVS, s_verticesVS);

		// DEBUG: Draw all models as points.
		//if (model->flags & MFLAG_DRAW_VERTICES)
		{
			// If the MFLAG_DRAW_VERTICES flag is set, draw all vertices as points. 
			model_drawVertices(model->vertexCount, s_verticesVS, model->polygons[0].color);
			return;
		}

		// TODO.
	}

}}  // TFE_JediRenderer