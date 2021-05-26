#include <TFE_System/profiler.h>
#include "robj3dFixed_TransformAndLighting.h"
#include "../rcommonFixed.h"
#include "../rlightingFixed.h"
#include "../../fixedPoint.h"
#include "../../rcommon.h"
#include "../../robject.h"

namespace TFE_JediRenderer
{

namespace RClassic_Fixed
{
	/////////////////////////////////////////////
	// Settings
	/////////////////////////////////////////////
	s32 s_enableFlatShading = 1;	// Set to 0 to disable flat shading.

	/////////////////////////////////////////////
	// Vertex Processing
	/////////////////////////////////////////////
	// Vertex attributes transformed to viewspace.
	vec3_fixed s_verticesVS[MAX_VERTEX_COUNT_3DO];
	vec3_fixed s_vertexNormalsVS[MAX_VERTEX_COUNT_3DO];
	// Vertex Lighting.
	fixed16_16 s_vertexIntensity[MAX_VERTEX_COUNT_3DO];

	/////////////////////////////////////////////
	// Polygon Processing
	/////////////////////////////////////////////
	// Polygon normals in viewspace (used for culling).
	vec3_fixed s_polygonNormalsVS[MAX_POLYGON_COUNT_3DO];

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

	void robj3d_transformVertices(s32 vertexCount, vec3_fixed* vtxIn, s32* xform, vec3_fixed* offset, vec3_fixed* vtxOut)
	{
		for (s32 v = 0; v < vertexCount; v++, vtxOut++, vtxIn++)
		{
			vtxOut->x = mul16(vtxIn->x, xform[0]) + mul16(vtxIn->y, xform[3]) + mul16(vtxIn->z, xform[6]) + offset->x;
			vtxOut->y = mul16(vtxIn->x, xform[1]) + mul16(vtxIn->y, xform[4]) + mul16(vtxIn->z, xform[7]) + offset->y;
			vtxOut->z = mul16(vtxIn->x, xform[2]) + mul16(vtxIn->y, xform[5]) + mul16(vtxIn->z, xform[8]) + offset->z;
		}
	}

	fixed16_16 robj3d_dotProduct(const vec3_fixed* pos, const vec3_fixed* normal, const vec3_fixed* dir)
	{
		fixed16_16 nx = normal->x - pos->x;
		fixed16_16 ny = normal->y - pos->y;
		fixed16_16 nz = normal->z - pos->z;

		fixed16_16 dx = dir->x - pos->x;
		fixed16_16 dy = dir->y - pos->y;
		fixed16_16 dz = dir->z - pos->z;

		fixed16_16 ndx = mul16(nx, dx);
		fixed16_16 ndy = mul16(ny, dy);
		fixed16_16 ndz = mul16(nz, dz);

		return ndx + ndy + ndz;
	}
		
	void robj3d_shadeVertices(s32 vertexCount, fixed16_16* outShading, const vec3_fixed* vertices, const vec3_fixed* normals)
	{
		const vec3_fixed* normal = normals;
		const vec3_fixed* vertex = vertices;
		for (s32 i = 0; i < vertexCount; i++, normal++, vertex++, outShading++)
		{
			fixed16_16 intensity = 0;
			if (s_sectorAmbient >= 31)
			{
				intensity = VSHADE_MAX_INTENSITY;
			}
			else
			{
				fixed16_16 lightIntensity = intensity;

				// Lighting
				for (s32 i = 0; i < s_lightCount; i++)
				{
					const CameraLight* light = &s_cameraLight[i];
					const vec3_fixed dir =
					{
						vertex->x + light->lightVS.x,
						vertex->y + light->lightVS.y,
						vertex->z + light->lightVS.z
					};

					const fixed16_16 I = robj3d_dotProduct(vertex, normal, &dir);
					if (I > 0)
					{
						fixed16_16 source = light->brightness;
						fixed16_16 sourceIntensity = mul16(VSHADE_MAX_INTENSITY, source);
						lightIntensity += mul16(I, sourceIntensity);
					}
				}
				intensity += mul16(lightIntensity, s_sectorAmbientFraction);

				// Distance falloff
				const fixed16_16 z = max(0, vertex->z);
				if (/*s_worldAtten < 31 || */s_cameraLightSource != 0)
				{
					const s32 depthScaled = min(s32(z >> 14), 127);
					const s32 cameraSource = MAX_LIGHT_LEVEL - (s_lightSourceRamp[depthScaled] + s_worldAmbient);
					if (cameraSource > 0)
					{
						intensity += intToFixed16(cameraSource);
					}
				}

				const s32 falloff = (z >> 15) + (z >> 14);	// z * 0.75
				intensity = max(intToFixed16(s_sectorAmbient), intensity) - falloff;
				intensity = clamp(intensity, s_scaledAmbient, VSHADE_MAX_INTENSITY);
			}
			*outShading = intensity;
		}
	}
		
	void robj3d_transformAndLight(SecObject* obj, JediModel* model)
	{
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
		robj3d_transformVertices(model->vertexCount, (vec3_fixed*)model->vertices, xform, &offsetVS, s_verticesVS);

		// No need for polygon normals or lighting if MFLAG_DRAW_VERTICES is set.
		if (model->flags & MFLAG_DRAW_VERTICES) { return; }

		// Polygon normals (used for backface culling)
		robj3d_transformVertices(model->polygonCount, (vec3_fixed*)model->polygonNormals, xform, &offsetVS, s_polygonNormalsVS);

		// Lighting
		if (model->flags & MFLAG_VERTEX_LIT)
		{
			robj3d_transformVertices(model->vertexCount, (vec3_fixed*)model->vertexNormals, xform, &offsetVS, s_vertexNormalsVS);
			robj3d_shadeVertices(model->vertexCount, s_vertexIntensity, s_verticesVS, s_vertexNormalsVS);
		}
	}

}}  // TFE_JediRenderer