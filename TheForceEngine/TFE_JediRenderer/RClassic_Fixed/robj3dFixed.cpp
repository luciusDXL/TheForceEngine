#include <TFE_System/profiler.h>
// TODO: Fix or move.
#include <TFE_Game/level.h>

#include "robj3dFixed.h"
#include "rsectorFixed.h"
#include "rcommonFixed.h"
#include "rlightingFixed.h"
#include "../fixedPoint.h"
#include "../rmath.h"
#include "../rcommon.h"
#include "../robject.h"

namespace TFE_JediRenderer
{

namespace RClassic_Fixed
{
	#define VSHADE_MAX_INTENSITY (31 * ONE_16)
	#define POLY_MAX_VTX_COUNT 32

	enum
	{
		POLYGON_FRONT_FACING = 0,
		POLYGON_BACK_FACING,
	};

	/////////////////////////////////////////////
	// Settings
	/////////////////////////////////////////////
	s32 s_enableFlatShading = 1;	// Set to 0 to disable flat shading.

	/////////////////////////////////////////////
	// Vertex Processing
	/////////////////////////////////////////////
	// Vertex attributes transformed to viewspace.
	static vec3_fixed s_verticesVS[MAX_VERTEX_COUNT_3DO];
	static vec3_fixed s_vertexNormalsVS[MAX_VERTEX_COUNT_3DO];
	// Vertex Lighting.
	static fixed16_16 s_vertexIntensity[MAX_VERTEX_COUNT_3DO];

	/////////////////////////////////////////////
	// Polygon Processing
	/////////////////////////////////////////////
	// Polygon normals in viewspace (used for culling).
	static vec3_fixed s_polygonNormalsVS[MAX_POLYGON_COUNT_3DO];
	// List of potentially visible polygons (after backface culling).
	static Polygon* s_visPolygons[MAX_POLYGON_COUNT_3DO];

	/////////////////////////////////////////////
	// Per-Polygon Setup
	/////////////////////////////////////////////
	static vec3_fixed s_polygonVerticesVS[POLY_MAX_VTX_COUNT];
	static vec3_fixed s_polygonVerticesProj[POLY_MAX_VTX_COUNT];
	static vec2_fixed s_polygonUv[POLY_MAX_VTX_COUNT];
	static fixed16_16 s_polygonIntensity[POLY_MAX_VTX_COUNT];

	/////////////////////////////////////////////
	// Clipping
	/////////////////////////////////////////////
	static fixed16_16 s_clipIntensityBuffer[POLY_MAX_VTX_COUNT];	// a buffer to hold clipped/final intensities
	static vec3_fixed s_clipPosBuffer[POLY_MAX_VTX_COUNT];			// a buffer to hold clipped/final positions
	static vec2_fixed s_clipUvBuffer[POLY_MAX_VTX_COUNT];			// a buffer to hold clipped/final texture coordinates

	static fixed16_16  s_clipY0;
	static fixed16_16  s_clipY1;
	static fixed16_16  s_clipParam0;
	static fixed16_16  s_clipParam1;
	static fixed16_16  s_clipIntersectY;
	static fixed16_16  s_clipIntersectZ;
	static vec3_fixed* s_clipTempPos;
	static fixed16_16  s_clipPlanePos0;
	static fixed16_16  s_clipPlanePos1;
	static fixed16_16* s_clipTempIntensity;
	static fixed16_16* s_clipIntensitySrc;
	static fixed16_16* s_clipIntensity0;
	static fixed16_16* s_clipIntensity1;
	static vec2_fixed* s_clipTempUv;
	static vec2_fixed* s_clipUvSrc;
	static vec2_fixed* s_clipUv0;
	static vec2_fixed* s_clipUv1;
	static fixed16_16  s_clipParam;
	static fixed16_16  s_clipIntersectX;
	static vec3_fixed* s_clipPos0;
	static vec3_fixed* s_clipPos1;
	static vec3_fixed* s_clipPosSrc;
	static vec3_fixed* s_clipPosOut;
	static fixed16_16* s_clipIntensityOut;
	static vec2_fixed* s_clipUvOut;

	////////////////////////////////////////////////
	// Polygon Drawing
	////////////////////////////////////////////////
	// Polygon
	static u8  s_polyColorIndex;
	static s32 s_polyVertexCount;
	static s32 s_polyMaxXIndex;
	static fixed16_16* s_polyIntensity;
	static vec2_fixed* s_polyUv;
	static vec3_fixed* s_polyProjVtx;
	static const u8*   s_polyColorMap;
	static Texture*    s_polyTexture;

	// Column
	static s32 s_columnX;
	static s32 s_columnHeight;
	static s32 s_dither;
	static u8* s_pcolumnOut;
		
	static fixed16_16  s_col_I0;
	static fixed16_16  s_col_dIdY;
	static vec2_fixed  s_col_Uv0;
	static vec2_fixed  s_col_dUVdY;

	// Polygon Edges
	static fixed16_16  s_ditherOffset;
	static fixed16_16  s_edgeBot_Z0;
	static fixed16_16  s_edgeBot_dZdX;
	static fixed16_16  s_edgeBot_dIdX;
	static fixed16_16  s_edgeBot_I0;
	static vec2_fixed  s_edgeBot_dUVdX;
	static vec2_fixed  s_edgeBot_Uv0;
	static fixed16_16  s_edgeBot_dYdX;
	static fixed16_16  s_edgeBot_Y0;
	static fixed16_16  s_edgeTop_dIdX;
	static vec2_fixed  s_edgeTop_dUVdX;
	static vec2_fixed  s_edgeTop_Uv0;
	static fixed16_16  s_edgeTop_dYdX;
	static fixed16_16  s_edgeTop_Z0;
	static fixed16_16  s_edgeTop_Y0;
	static fixed16_16  s_edgeTop_dZdX;
	static fixed16_16  s_edgeTop_I0;

	static s32 s_edgeBotY0_Pixel;
	static s32 s_edgeTopY0_Pixel;
	static s32 s_edgeBotIndex;
	static s32 s_edgeTopIndex;
	static s32 s_edgeTopLength;
	static s32 s_edgeBotLength;
		
	void model_projectVertices(vec3_fixed* pos, s32 count, vec3_fixed* out);
	void model_drawShadedColorPolygon(vec3_fixed* projVertices, fixed16_16* intensity, s32 vertexCount, u8 color);
	u8 model_computePolygonColor(vec3_fixed* normal, u8 color, fixed16_16 z);
	u8 model_computePolygonLightLevel(vec3_fixed* normal, fixed16_16 z);

	s32 model_clipPolygonGouraud(vec3_fixed* pos, s32* intensity, s32 count);
	s32 model_clipPolygonUv(vec3_fixed* pos, vec2_fixed* uv, s32 count);
	s32 model_clipPolygonUvGouraud(vec3_fixed* pos, vec2_fixed* uv, s32* intensity, s32 count);
	s32 model_clipPolygon(vec3_fixed* pos, s32 count);

	void model_drawFlatColorPolygon(vec3_fixed* projVertices, s32 vertexCount, u8 color);
	void model_drawShadedColorPolygon(vec3_fixed* projVertices, fixed16_16* intensity, s32 vertexCount, u8 color);
	void model_drawFlatTexturePolygon(vec3_fixed* projVertices, vec2_fixed* uv, s32 vertexCount, Texture* texture, u8 color);
	void model_drawShadedTexturePolygon(vec3_fixed* projVertices, vec2_fixed* uv, fixed16_16* intensity, s32 vertexCount, Texture* texture);

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

	fixed16_16 model_dotProduct(const vec3_fixed* pos, const vec3_fixed* normal, const vec3_fixed* dir)
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

	fixed16_16 dot(const vec3_fixed* v0, const vec3_fixed* v1)
	{
		return mul16(v0->x, v1->x) + mul16(v0->y, v1->y) + mul16(v0->z, v1->z);
	}

	void model_shadeVertices(s32 vertexCount, fixed16_16* outShading, const vec3_fixed* vertices, const vec3_fixed* normals)
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

					const fixed16_16 I = model_dotProduct(vertex, normal, &dir);
					if (I > 0)
					{
						s32 source = light->brightness;
						s32 sourceIntensity = mul16(VSHADE_MAX_INTENSITY, source);
						lightIntensity += mul16(I, sourceIntensity);
					}
				}
				intensity += mul16(lightIntensity, s_sectorAmbientFraction);

				// Distance falloff
				const s32 z = max(0, vertex->z);
				if (/*s_worldAtten < 31 || */s_cameraLightSource != 0)
				{
					// TODO
				}

				const s32 falloff = (z >> 15) + (z >> 14);	// z * 0.75
				intensity = max(intToFixed16(s_sectorAmbient), intensity) - falloff;
				intensity = clamp(intensity, s_scaledAmbient, VSHADE_MAX_INTENSITY);
			}
			*outShading = intensity;
		}
	}
		
	s32 getPolygonFacing(const vec3_fixed* normal, const vec3_fixed* pos)
	{
		const vec3_fixed offset = { -pos->x, -pos->y, -pos->z };
		return dot(normal, &offset) < 0 ? POLYGON_BACK_FACING : POLYGON_FRONT_FACING;
	}
	
	s32 polygonSort(const void* r0, const void* r1)
	{
		Polygon* p0 = *((Polygon**)r0);
		Polygon* p1 = *((Polygon**)r1);
		return p1->zAve - p0->zAve;
	}

	void model_drawVertices(s32 vertexCount, const vec3_fixed* vertices, u8 color)
	{
		// cannot draw if the color is transparent.
		if (color == 0) { return; }

		// Loop through the vertices and draw them as pixels.
		const vec3_fixed* vertex = vertices;
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

		// Draw vertices and return if the flag is set.
		if (model->flags & MFLAG_DRAW_VERTICES)
		{
			// If the MFLAG_DRAW_VERTICES flag is set, draw all vertices as points. 
			model_drawVertices(model->vertexCount, s_verticesVS, model->polygons[0].color);
			return;
		}

		// Polygon normals (used for backface culling)
		model_transformVertices(model->polygonCount, (vec3_fixed*)model->polygonNormals, xform, &offsetVS, s_polygonNormalsVS);

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

		// Lighting
		if (model->flags & MFLAG_VERTEX_LIT)
		{
			model_transformVertices(model->vertexCount, (vec3_fixed*)model->vertexNormals, xform, &offsetVS, s_vertexNormalsVS);
			model_shadeVertices(model->vertexCount, s_vertexIntensity, s_verticesVS, s_vertexNormalsVS);
		}

		// Cull backfacing polygons. The results are stored as "visPolygons"
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
		// Nothing to render.
		if (visPolygonCount < 1) { return; }

		// Sort polygons from back to front.
		qsort(s_visPolygons, visPolygonCount, sizeof(Polygon*), polygonSort);

		// Draw polygons
		visPolygon = s_visPolygons;
		for (s32 i = 0; i < visPolygonCount; i++, visPolygon++)
		{
			polygon = *visPolygon;
			
			//const s32 shading = polygon->shading;
			// DEBUG: Force PSHADE_PLANE to PSHADE_FLAT until implemented.
			const s32 shading = (polygon->shading == PSHADE_PLANE) ? PSHADE_FLAT : polygon->shading;

			const s32 vertexCount = polygon->vertexCount;
			if (vertexCount <= 0)
			{
				continue;
			}

			// Copy polygon vertices.
			for (s32 v = 0; v < vertexCount; v++)
			{
				s_polygonVerticesVS[v] = s_verticesVS[polygon->indices[v]];
			}

			// Copy uvs if required.
			if (shading & PSHADE_TEXTURE)
			{
				const vec2_fixed* uv = (vec2_fixed*)polygon->uv;
				for (s32 v = 0; v < vertexCount; v++)
				{
					s_polygonUv[v] = uv[v];
				}
			}

			// Copy intensities if required.
			if (shading & PSHADE_GOURAUD)
			{
				const s32* indices = polygon->indices;
				for (s32 v = 0; v < vertexCount; v++)
				{
					s_polygonIntensity[v] = s_vertexIntensity[indices[v]];
				}
			}

			// Handle clipping
			s32 polyVertexCount = 0;
			if (shading == PSHADE_GOURAUD)
			{
				polyVertexCount = model_clipPolygonGouraud(s_polygonVerticesVS, s_polygonIntensity, polygon->vertexCount);
			}
			else if (shading == PSHADE_TEXTURE)
			{
				polyVertexCount = model_clipPolygonUv(s_polygonVerticesVS, s_polygonUv, polygon->vertexCount);
			}
			else if (shading == PSHADE_GOURAUD_TEXTURE)
			{
				polyVertexCount = model_clipPolygonUvGouraud(s_polygonVerticesVS, s_polygonUv, s_polygonIntensity, polygon->vertexCount);
			}
			else
			{
				polyVertexCount = model_clipPolygon(s_polygonVerticesVS, polygon->vertexCount);
			}
			// Cull the polygon if not enough vertices survive clipping.
			if (polyVertexCount < 3) { continue; }

			// Project the resulting vertices.
			model_projectVertices(s_polygonVerticesVS, polyVertexCount, s_polygonVerticesProj);

			// Handle shading modes...
			if (shading == PSHADE_GOURAUD)
			{
				model_drawShadedColorPolygon(s_polygonVerticesProj, s_polygonIntensity, polyVertexCount, polygon->color);
			}
			else if (shading == PSHADE_FLAT)
			{
				u8 color = polygon->color;
				if (s_enableFlatShading)
				{
					color = model_computePolygonColor(&s_polygonNormalsVS[polygon->index], color, polygon->zAve);
				}
				model_drawFlatColorPolygon(s_polygonVerticesProj, polyVertexCount, color);
			}
			else if (shading == PSHADE_GOURAUD_TEXTURE)
			{
				model_drawShadedTexturePolygon(s_polygonVerticesProj, s_polygonUv, s_polygonIntensity, polyVertexCount, polygon->texture);
			}
			else if (shading == PSHADE_TEXTURE)
			{
				u8 lightLevel = 0;
				if (s_enableFlatShading)
				{
					lightLevel = model_computePolygonLightLevel(&s_polygonNormalsVS[polygon->index], polygon->zAve);
				}
				model_drawFlatTexturePolygon(s_polygonVerticesProj, s_polygonUv, polyVertexCount, polygon->texture, lightLevel);
			}
		}
	}

	void model_projectVertices(vec3_fixed* pos, s32 count, vec3_fixed* out)
	{
		for (s32 i = 0; i < count; i++, pos++, out++)
		{
			out->x = round16(div16(mul16(pos->x, s_focalLength_Fixed), pos->z) + s_halfWidth_Fixed);
			out->y = round16(div16(mul16(pos->y, s_focalLenAspect_Fixed), pos->z) + s_halfHeight_Fixed);
			out->z = pos->z;
		}
	}

	u8 model_computePolygonColor(vec3_fixed* normal, u8 color, fixed16_16 z)
	{
		if (s_sectorAmbient >= 31) { return color; }
		s32 lightLevel = 0;
		
		fixed16_16 lighting = 0;
		for (s32 i = 0; i < s_lightCount; i++)
		{
			const CameraLight* light = &s_cameraLight[i];
			const s32 L = dot(normal, &light->lightVS);

			if (L > 0)
			{
				const fixed16_16 brightness = mul16(light->brightness, VSHADE_MAX_INTENSITY);
				lighting += mul16(L, brightness);
			}
		}
		lightLevel += floor16(mul16(lighting, s_sectorAmbientFraction));
		if (lightLevel >= 31) { return color; }

		if (/*s_worldAtten < 31 || */s_cameraLightSource != 0)
		{
			// TODO
		}

		z = max(z, 0);
		const s32 falloff = (z >> 15) + (z >> 14);	// z * 0.75
		lightLevel = max(lightLevel, s_sectorAmbient);
		lightLevel = max(lightLevel - falloff, s_scaledAmbient);

		if (lightLevel >= 31) { return color; }
		if (lightLevel <= 0) { return s_polyColorMap[color]; }

		return s_polyColorMap[lightLevel*256 + color];
	}

	u8 model_computePolygonLightLevel(vec3_fixed* normal, fixed16_16 z)
	{
		if (s_sectorAmbient >= 31) { return 31; }
		s32 lightLevel = 0;

		fixed16_16 lighting = 0;
		for (s32 i = 0; i < s_lightCount; i++)
		{
			const CameraLight* light = &s_cameraLight[i];
			const s32 L = dot(normal, &light->lightVS);

			if (L > 0)
			{
				const fixed16_16 brightness = mul16(light->brightness, VSHADE_MAX_INTENSITY);
				lighting += mul16(L, brightness);
			}
		}
		lightLevel += floor16(mul16(lighting, s_sectorAmbientFraction));
		if (lightLevel >= 31) { return 31; }

		if (/*s_worldAtten < 31 || */s_cameraLightSource != 0)
		{
			// TODO
		}

		z = max(z, 0);
		const s32 falloff = (z >> 15) + (z >> 14);	// z * 0.75
		lightLevel = max(lightLevel, s_sectorAmbient);
		lightLevel = max(lightLevel - falloff, s_scaledAmbient);

		return clamp(lightLevel, 0, 31);
	}

	////////////////////////////////////////////////
	// Instantiate Clip Routines.
	// This abuses C-Macros to build 4 versions of
	// the clipping functions based on the defines.
	//
	// This avoids duplicating a lot of the code 4
	// times and is probably pretty similar to what
	// the original developers did at the time.
	//
	// This is similar to modern shader variants.
	////////////////////////////////////////////////
	// Position only
	#include "robj3dFixed_ClipFunc.h"

	// Position, Intensity
	#define CLIP_INTENSITY
	#include "robj3dFixed_ClipFunc.h"

	// Position, UV
	#define CLIP_UV
	#undef CLIP_INTENSITY
	#include "robj3dFixed_ClipFunc.h"

	// Position, UV, Intensity
	#define CLIP_INTENSITY
	#include "robj3dFixed_ClipFunc.h"


	////////////////////////////////////////////////
	// Instantiate Polygon Draw Routines.
	// This abuses C-Macros to build 4 versions of
	// the drawing functions based on the defines.
	//
	// This avoids duplicating a lot of the code 4
	// times and is probably pretty similar to what
	// the original developers did at the time.
	//
	// This is similar to modern shader variants.
	////////////////////////////////////////////////
	// Flat color
	#include "robj3dFixed_PolyRenderFunc.h"

	// Shaded
	#define POLY_INTENSITY
	#include "robj3dFixed_PolyRenderFunc.h"

	// Flat Texture
	#define POLY_UV
	#undef POLY_INTENSITY
	#include "robj3dFixed_PolyRenderFunc.h"

	// Shaded Texture
	#define POLY_INTENSITY
	#include "robj3dFixed_PolyRenderFunc.h"

}}  // TFE_JediRenderer