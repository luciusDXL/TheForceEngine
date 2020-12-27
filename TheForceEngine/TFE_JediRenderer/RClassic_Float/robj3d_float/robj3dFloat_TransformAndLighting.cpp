#include <TFE_System/profiler.h>
#include "robj3dFloat_TransformAndLighting.h"
#include "../rlightingFloat.h"
#include "../../fixedPoint.h"
#include "../../rcommon.h"
#include "../../robject.h"

namespace TFE_JediRenderer
{

namespace RClassic_Float
{
	/////////////////////////////////////////////
	// Settings
	/////////////////////////////////////////////
	s32 s_enableFlatShading = 1;	// Set to 0 to disable flat shading.

	/////////////////////////////////////////////
	// Vertex Processing
	/////////////////////////////////////////////
	// Vertex attributes transformed to viewspace.
	vec3_float s_verticesVS[MAX_VERTEX_COUNT_3DO];
	vec3_float s_vertexNormalsVS[MAX_VERTEX_COUNT_3DO];
	// Vertex Lighting.
	f32 s_vertexIntensity[MAX_VERTEX_COUNT_3DO];

	/////////////////////////////////////////////
	// Polygon Processing
	/////////////////////////////////////////////
	// Polygon normals in viewspace (used for culling).
	vec3_float s_polygonNormalsVS[MAX_POLYGON_COUNT_3DO];

	void rotateVectorM3x3(vec3_float* inVec, vec3_float* outVec, f32* mtx)
	{
		outVec->x = (inVec->x*mtx[0]) + (inVec->y*mtx[3]) + (inVec->z*mtx[6]);
		outVec->y = (inVec->x*mtx[1]) + (inVec->y*mtx[4]) + (inVec->z*mtx[7]);
		outVec->z = (inVec->x*mtx[2]) + (inVec->y*mtx[5]) + (inVec->z*mtx[8]);
	}

	void mulMatrix3x3(f32* mtx0, f32* mtx1, f32* mtxOut)
	{
		mtxOut[0] = (mtx0[0]*mtx1[0]) + (mtx0[3]*mtx1[3]) + (mtx0[6]*mtx1[6]);
		mtxOut[3] = (mtx0[0]*mtx1[1]) + (mtx0[3]*mtx1[4]) + (mtx0[6]*mtx1[7]);
		mtxOut[6] = (mtx0[0]*mtx1[2]) + (mtx0[3]*mtx1[5]) + (mtx0[6]*mtx1[8]);

		mtxOut[1] = (mtx0[1]*mtx1[0]) + (mtx0[4]*mtx1[3]) + (mtx0[7]*mtx1[6]);
		mtxOut[4] = (mtx0[1]*mtx1[1]) + (mtx0[4]*mtx1[4]) + (mtx0[7]*mtx1[7]);
		mtxOut[7] = (mtx0[1]*mtx1[2]) + (mtx0[4]*mtx1[5]) + (mtx0[7]*mtx1[8]);

		mtxOut[2] = (mtx0[2]*mtx1[0]) + (mtx0[5]*mtx1[3]) + (mtx0[8]*mtx1[6]);
		mtxOut[5] = (mtx0[2]*mtx1[1]) + (mtx0[5]*mtx1[4]) + (mtx0[8]*mtx1[7]);
		mtxOut[8] = (mtx0[2]*mtx1[2]) + (mtx0[5]*mtx1[5]) + (mtx0[8]*mtx1[8]);
	}

	void robj3d_transformVertices(s32 vertexCount, vec3_fixed* vtxIn, f32* xform, vec3_float* offset, vec3_float* vtxOut)
	{
		for (s32 v = 0; v < vertexCount; v++, vtxOut++, vtxIn++)
		{
			const vec3_float vtxFlt = { fixed16ToFloat(vtxIn->x), fixed16ToFloat(vtxIn->y), fixed16ToFloat(vtxIn->z) };

			vtxOut->x = (vtxFlt.x*xform[0]) + (vtxFlt.y*xform[3]) + (vtxFlt.z*xform[6]) + offset->x;
			vtxOut->y = (vtxFlt.x*xform[1]) + (vtxFlt.y*xform[4]) + (vtxFlt.z*xform[7]) + offset->y;
			vtxOut->z = (vtxFlt.x*xform[2]) + (vtxFlt.y*xform[5]) + (vtxFlt.z*xform[8]) + offset->z;
		}
	}

	f32 robj3d_dotProduct(const vec3_float* pos, const vec3_float* normal, const vec3_float* dir)
	{
		f32 nx = normal->x - pos->x;
		f32 ny = normal->y - pos->y;
		f32 nz = normal->z - pos->z;

		f32 dx = dir->x - pos->x;
		f32 dy = dir->y - pos->y;
		f32 dz = dir->z - pos->z;

		f32 ndx = nx*dx;
		f32 ndy = ny*dy;
		f32 ndz = nz*dz;

		return ndx + ndy + ndz;
	}
		
	void robj3d_shadeVertices(s32 vertexCount, f32* outShading, const vec3_float* vertices, const vec3_float* normals)
	{
		const vec3_float* normal = normals;
		const vec3_float* vertex = vertices;
		for (s32 i = 0; i < vertexCount; i++, normal++, vertex++, outShading++)
		{
			f32 intensity = 0.0f;
			if (s_sectorAmbient >= 31)
			{
				intensity = (f32)VSHADE_MAX_INTENSITY;
			}
			else
			{
				f32 lightIntensity = intensity;

				// Lighting
				for (s32 i = 0; i < s_lightCount; i++)
				{
					const CameraLight* light = &s_cameraLight[i];
					const vec3_float dir =
					{
						vertex->x + light->lightVS.x,
						vertex->y + light->lightVS.y,
						vertex->z + light->lightVS.z
					};

					const f32 I = robj3d_dotProduct(vertex, normal, &dir);
					if (I > 0.0f)
					{
						f32 source = light->brightness;
						f32 sourceIntensity = (f32)VSHADE_MAX_INTENSITY * source;
						lightIntensity += (I * sourceIntensity);
					}
				}
				intensity += (lightIntensity * fixed16ToFloat(s_sectorAmbientFraction));

				// Distance falloff
				const f32 z = max(0.0f, vertex->z);
				if (/*s_worldAtten < 31 || */s_cameraLightSource != 0)
				{
					const s32 depthScaled = (s32)min(z * 4.0f, 127.0f);
					const s32 cameraSource = MAX_LIGHT_LEVEL - (s_lightSourceRamp[depthScaled] + s_worldAmbient);
					if (cameraSource > 0)
					{
						intensity += f32(cameraSource);
					}
				}

				const f32 falloff = floorf(z * 6.0f);
				intensity = max(f32(s_sectorAmbient), intensity) - falloff;
				intensity = clamp(intensity, (f32)s_scaledAmbient, (f32)VSHADE_MAX_INTENSITY);
			}
			*outShading = intensity;
		}
	}

	void robj3d_transformAndLight(SecObject* obj, JediModel* model)
	{
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