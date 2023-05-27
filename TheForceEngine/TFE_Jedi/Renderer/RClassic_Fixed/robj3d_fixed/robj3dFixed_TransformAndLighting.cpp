#include <TFE_System/profiler.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Math/core_math.h>
#include "robj3dFixed_TransformAndLighting.h"
#include "../rclassicFixedSharedState.h"
#include "../rlightingFixed.h"
#include "../../rcommon.h"

namespace TFE_Jedi
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
	// TFE: Model limits have been lifted, requiring some changes.
	// Vertex attributes transformed to viewspace.
	std::vector<vec3_fixed> s_verticesVS;
	std::vector<vec3_fixed> s_vertexNormalsVS;
	// Vertex Lighting.
	std::vector<fixed16_16> s_vertexIntensity;

	/////////////////////////////////////////////
	// Polygon Processing
	/////////////////////////////////////////////
	// Polygon normals in viewspace (used for culling).
	std::vector<vec3_fixed> s_polygonNormalsVS;
			
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
				// Lighting
				fixed16_16 lightIntensity = 0;
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
				if (s_worldAmbient < 31 || s_cameraLightSource)
				{
					const s32 depthScaled = min(s32(z >> 14), 127);
					const s32 cameraSource = MAX_LIGHT_LEVEL - (s_lightSourceRamp[depthScaled] + s_worldAmbient);
					if (cameraSource > 0)
					{
						intensity += intToFixed16(cameraSource);
					}
				}
				intensity = max(intensity, intToFixed16(s_sectorAmbient));

				const s32 falloff = (z >> LIGHT_ATTEN0) + (z >> LIGHT_ATTEN1);
				intensity = max(intensity - falloff, s_scaledAmbient);
				intensity = clamp(intensity, 0, VSHADE_MAX_INTENSITY);
			}
			*outShading = intensity;
		}
	}

	void robj3d_allocateBuffers(JediModel* model)
	{
		if (model->vertexCount > s_verticesVS.size())
		{
			s_verticesVS.resize(model->vertexCount);
			s_vertexNormalsVS.resize(model->vertexCount);
			s_vertexIntensity.resize(model->vertexCount);
		}
		if (model->polygonCount > s_polygonNormalsVS.size())
		{
			s_polygonNormalsVS.resize(model->polygonCount);
		}
	}
		
	void robj3d_transformAndLight(SecObject* obj, JediModel* model)
	{
		vec3_fixed offsetWS;
		offsetWS.x = obj->posWS.x - s_rcfState.cameraPos.x;
		offsetWS.y = obj->posWS.y - s_rcfState.eyeHeight;
		offsetWS.z = obj->posWS.z - s_rcfState.cameraPos.z;

		// Allocate buffer space.
		robj3d_allocateBuffers(model);

		// Calculate the view space object camera offset.
		vec3_fixed offsetVS;
		rotateVectorM3x3(&offsetWS, &offsetVS, s_rcfState.cameraMtx);

		// Concatenate the camera and object rotation matrices.
		fixed16_16 xform[9];
		mulMatrix3x3(s_rcfState.cameraMtx, obj->transform, xform);

		// Transform model vertices into view space.
		robj3d_transformVertices(model->vertexCount, (vec3_fixed*)model->vertices, xform, &offsetVS, s_verticesVS.data());

		// No need for polygon normals or lighting if MFLAG_DRAW_VERTICES is set.
		if (model->flags & MFLAG_DRAW_VERTICES) { return; }

		// Polygon normals (used for backface culling)
		robj3d_transformVertices(model->polygonCount, (vec3_fixed*)model->polygonNormals, xform, &offsetVS, s_polygonNormalsVS.data());

		// Lighting
		if (model->flags & MFLAG_VERTEX_LIT)
		{
			robj3d_transformVertices(model->vertexCount, (vec3_fixed*)model->vertexNormals, xform, &offsetVS, s_vertexNormalsVS.data());
			robj3d_shadeVertices(model->vertexCount, s_vertexIntensity.data(), s_verticesVS.data(), s_vertexNormalsVS.data());
		}
	}

}}  // TFE_Jedi