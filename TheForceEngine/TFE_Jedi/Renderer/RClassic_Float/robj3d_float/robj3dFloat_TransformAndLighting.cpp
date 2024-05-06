#include <TFE_System/profiler.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Settings/settings.h>
#include "robj3dFloat_TransformAndLighting.h"
#include "../rclassicFloatSharedState.h"
#include "../rlightingFloat.h"
#include "../../rcommon.h"

namespace TFE_Jedi
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
	std::vector<vec3_float> s_verticesVS;
	std::vector<vec3_float> s_vertexNormalsVS;
	// Vertex Lighting.
	std::vector<f32> s_vertexIntensity;

	/////////////////////////////////////////////
	// Polygon Processing
	/////////////////////////////////////////////
	// Polygon normals in viewspace (used for culling).
	std::vector<vec3_float> s_polygonNormalsVS;
			
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

	void robj3d_mulMatrix3x3(f32* mtx0, fixed16_16* mtx1, f32* mtxOut)
	{
		const f32 mtx1Flt[9]=
		{
			fixed16ToFloat(mtx1[0]), fixed16ToFloat(mtx1[1]), fixed16ToFloat(mtx1[2]),
			fixed16ToFloat(mtx1[3]), fixed16ToFloat(mtx1[4]), fixed16ToFloat(mtx1[5]),
			fixed16ToFloat(mtx1[6]), fixed16ToFloat(mtx1[7]), fixed16ToFloat(mtx1[8]),
		};

		mtxOut[0] = (mtx0[0] * mtx1Flt[0]) + (mtx0[3] * mtx1Flt[3]) + (mtx0[6] * mtx1Flt[6]);
		mtxOut[3] = (mtx0[0] * mtx1Flt[1]) + (mtx0[3] * mtx1Flt[4]) + (mtx0[6] * mtx1Flt[7]);
		mtxOut[6] = (mtx0[0] * mtx1Flt[2]) + (mtx0[3] * mtx1Flt[5]) + (mtx0[6] * mtx1Flt[8]);

		mtxOut[1] = (mtx0[1] * mtx1Flt[0]) + (mtx0[4] * mtx1Flt[3]) + (mtx0[7] * mtx1Flt[6]);
		mtxOut[4] = (mtx0[1] * mtx1Flt[1]) + (mtx0[4] * mtx1Flt[4]) + (mtx0[7] * mtx1Flt[7]);
		mtxOut[7] = (mtx0[1] * mtx1Flt[2]) + (mtx0[4] * mtx1Flt[5]) + (mtx0[7] * mtx1Flt[8]);

		mtxOut[2] = (mtx0[2] * mtx1Flt[0]) + (mtx0[5] * mtx1Flt[3]) + (mtx0[8] * mtx1Flt[6]);
		mtxOut[5] = (mtx0[2] * mtx1Flt[1]) + (mtx0[5] * mtx1Flt[4]) + (mtx0[8] * mtx1Flt[7]);
		mtxOut[8] = (mtx0[2] * mtx1Flt[2]) + (mtx0[5] * mtx1Flt[5]) + (mtx0[8] * mtx1Flt[8]);
	}

	f32 robj3d_dotProduct(const vec3_float* pos, const vec3_float* normal, const vec3_float* dir)
	{
		f32 nx = normal->x - pos->x;
		f32 ny = normal->y - pos->y;
		f32 nz = normal->z - pos->z;

		f32 dx = dir->x - pos->x;
		f32 dy = dir->y - pos->y;
		f32 dz = dir->z - pos->z;

		f32 ndx = nx * dx;
		f32 ndy = ny * dy;
		f32 ndz = nz * dz;

		return ndx + ndy + ndz;
	}
		
	void robj3d_shadeVertices(s32 vertexCount, f32* outShading, const vec3_float* vertices, const vec3_float* normals)
	{
		TFE_Settings_Graphics* graphicsSettings = TFE_Settings::getGraphicsSettings();
		bool overrideLighting = graphicsSettings->overrideLighting;
		s32 sectorAmbient = (overrideLighting ? 0 : s_sectorAmbient);
		f32 sectorAmbientFraction = (overrideLighting) ? 1 : fixed16ToFloat(s_sectorAmbientFraction);
		s32 worldAmbient = (overrideLighting ? 0 : s_worldAmbient);
		s32 scaledAmbient = (overrideLighting ? 0 : s_scaledAmbient);

		const vec3_float* normal = normals;
		const vec3_float* vertex = vertices;
		for (s32 i = 0; i < vertexCount; i++, normal++, vertex++, outShading++)
		{
			f32 intensity = 0.0f;
			if (sectorAmbient >= MAX_LIGHT_LEVEL || s_fullBright) // s_fullBright is for TFE cheat LABRIGHT.
			{
				intensity = VSHADE_MAX_INTENSITY_FLT;
			}
			else
			{
				f32 lightIntensity = intensity;

				// Lighting
				for (s32 i = 0; i < s_lightCount; i++)
				{
					const CameraLightFlt* light = &s_cameraLight[i];
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
						f32 sourceIntensity = VSHADE_MAX_INTENSITY_FLT * source;
						lightIntensity += (I * sourceIntensity);
					}
				}
				intensity += lightIntensity * sectorAmbientFraction;

				// Distance falloff
				const f32 z = max(0.0f, vertex->z);
				if (s_worldAmbient < 31 || s_cameraLightSource)
				{
					s32 depthScaled = min(s32(z * 4.0f), 127);
					s32 lightSource = MAX_LIGHT_LEVEL - (s_lightSourceRamp[depthScaled] + worldAmbient);
					if (lightSource > 0)
					{
						intensity += f32(lightSource);
					}
				}
				intensity = max(intensity, f32(sectorAmbient));

				const s32 falloff = s32(z / 16.0f) + s32(z / 32.0f);		// depth * 3/32
				intensity = max(intensity - f32(falloff), f32(scaledAmbient));
				intensity = clamp(intensity, 0.0f, VSHADE_MAX_INTENSITY_FLT);
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
		vec3_float offsetWS;
		offsetWS.x = fixed16ToFloat(obj->posWS.x) - s_rcfltState.cameraPos.x;
		offsetWS.y = fixed16ToFloat(obj->posWS.y) - s_rcfltState.eyeHeight;
		offsetWS.z = fixed16ToFloat(obj->posWS.z) - s_rcfltState.cameraPos.z;

		// Allocate buffers.
		robj3d_allocateBuffers(model);

		// Calculate the view space object camera offset.
		vec3_float offsetVS;
		rotateVectorM3x3(&offsetWS, &offsetVS, s_rcfltState.cameraMtx);

		// Concatenate the camera and object rotation matrices.
		f32 xform[9];
		robj3d_mulMatrix3x3(s_rcfltState.cameraMtx, obj->transform, xform);

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