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
	static fixed16_16  s_clipParam;
	static fixed16_16  s_clipIntersectX;
	static vec3_fixed* s_clipPos0;
	static vec3_fixed* s_clipPos1;
	static vec3_fixed* s_clipPosSrc;
	static vec3_fixed* s_clipPosOut;
	static fixed16_16* s_clipIntensityOut;

	////////////////////////////////////////////////
	// Polygon Drawing
	////////////////////////////////////////////////
	// Polygon
	static u8  s_polyColorIndex;
	static s32 s_polyVertexCount;
	static s32 s_polyMaxXIndex;
	static fixed16_16* s_polyIntensity;
	static vec3_fixed* s_polyProjVtx;
	static const u8*   s_polyColorMap;

	// Column
	static s32 s_columnX;
	static s32 s_columnHeight;
	static s32 s_dither;
	static u8* s_pcolumnOut;
		
	static fixed16_16  s_col_I0;
	static fixed16_16  s_col_dIdY;

	// Polygon Edges
	static fixed16_16  s_ditherOffset;
	static fixed16_16  s_edgeBot_Z0;
	static fixed16_16  s_edgeBot_dZdX;
	static fixed16_16  s_edgeBot_dIdX;
	static fixed16_16  s_edgeBot_I0;
	static fixed16_16  s_edgeBot_dYdX;
	static fixed16_16  s_edgeBotY0;
	static fixed16_16  s_edgeTop_dIdX;
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

	s32 model_clipPolygonGouraud(vec3_fixed* pos, s32* intensity, s32 count);
	void model_projectVertices(vec3_fixed* pos, s32 count, vec3_fixed* out);
	void model_drawColorPolygon(vec3_fixed* projVertices, fixed16_16* intensity, s32 vertexCount, u8 color);
		
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
				for (s32 i = 0; i < s_lightCount; i++)
				{
					vec3_fixed dir;
					CameraLight* light = &s_cameraLight[i];
					dir.x = vertex->x + light->lightVS.x;
					dir.y = vertex->y + light->lightVS.y;
					dir.z = vertex->z + light->lightVS.z;
					fixed16_16 I = model_dotProduct(vertex, normal, &dir);
					if (I > 0)
					{
						s32 source = light->brightness;
						s32 sourceIntensity = mul16(VSHADE_MAX_INTENSITY, source);
						lightIntensity += mul16(I, sourceIntensity);
					}
				}
				intensity += mul16(lightIntensity, s_sectorAmbientFraction);
				s32 z = max(0, vertex->z);
				if (/*s_worldAtten < 31 || */s_cameraLightSource != 0)
				{
					// TODO
				}

				s32 falloff = (z >> 15) + (z >> 14);	// z * 0.75

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
			// DEBUG: Force everything to be gouraud for now.
			const s32 shading = PSHADE_GOURAUD;

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
				for (s32 v = 0; v < vertexCount; v++)
				{
					s_polygonUv[v] = ((vec2_fixed*)polygon->uv)[v];
				}
			}

			// Copy intensities if required.
			if (shading & PSHADE_GOURAUD)
			{
				for (s32 v = 0; v < vertexCount; v++)
				{
					s_polygonIntensity[v] = s_vertexIntensity[polygon->indices[v]];
				}
			}

			// Handle shading modes...
			if (shading == PSHADE_GOURAUD)
			{
				s32 vertexCount = model_clipPolygonGouraud(s_polygonVerticesVS, s_polygonIntensity, polygon->vertexCount);
				if (vertexCount < 3) { continue; }

				model_projectVertices(s_polygonVerticesVS, vertexCount, s_polygonVerticesProj);
				model_drawColorPolygon(s_polygonVerticesProj, s_polygonIntensity, vertexCount, polygon->color);
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

	s32 model_swapClipBuffers(s32 outVertexCount)
	{
		// Swap src position and output position.
		s_clipTempPos = s_clipPosSrc;
		s_clipPosSrc = s_clipPosOut;
		s_clipPosOut = s_clipTempPos;

		// Swap src intensity and output intensity.
		s_clipTempIntensity = s_clipIntensitySrc;
		s_clipIntensitySrc = s_clipIntensityOut;
		s_clipIntensityOut = s_clipIntensitySrc;

		s32 srcVertexCount = outVertexCount;
		s32 end = srcVertexCount - 1;

		// Left clip plane.
		s_clipPos0 = &s_clipPosSrc[end];
		s_clipPos1 = s_clipPosSrc;
		s_clipIntensity0 = &s_clipIntensitySrc[end];
		s_clipIntensity1 = s_clipIntensitySrc;

		return srcVertexCount;
	}

	s32 model_clipPolygonGouraud(vec3_fixed* pos, s32* intensity, s32 count)
	{
		s32 outVertexCount = 0;
		s32 end = count - 1;
		s_clipIntensitySrc = intensity;
		s_clipPosSrc = pos;

		// Clip against the near plane.
		s_clipPosOut = s_clipPosBuffer;
		s_clipPos0 = &pos[count - 1];
		s_clipPos1 = pos;

		s_clipIntensityOut = s_clipIntensityBuffer;
		s_clipIntensity0 = &intensity[count - 1];
		s_clipIntensity1 = intensity;

		///////////////////////////////////////////////////
		// Near Clip Plane
		///////////////////////////////////////////////////
		for (s32 i = 0; i < count; i++)
		{
			if (s_clipPos0->z < ONE_16 && s_clipPos1->z < ONE_16)
			{
				s_clipPos0 = s_clipPos1;
				s_clipIntensity0 = s_clipIntensity1;
				s_clipPos1++;
				s_clipIntensity1++;
				continue;
			}

			// Add vertex 0 if it is on or in front of the near plane.
			if (s_clipPos0->z >= ONE_16)
			{
				s_clipPosOut[outVertexCount] = *s_clipPos0;
				s_clipIntensityOut[outVertexCount] = *s_clipIntensity0;
				outVertexCount++;
			}

			// If either position is exactly on the near plane continue.
			if (s_clipPos0->z == ONE_16 || s_clipPos1->z == ONE_16)
			{
				s_clipPos0 = s_clipPos1;
				s_clipIntensity0 = s_clipIntensity1;
				s_clipPos1++;
				s_clipIntensity1++;
				continue;
			}

			// Finally clip the edge against the near plane, generating a new vertex.
			if (s_clipPos0->z < ONE_16 || s_clipPos1->z < ONE_16)
			{
				fixed16_16 z0 = s_clipPos0->z;
				fixed16_16 z1 = s_clipPos1->z;

				// Parametric clip coordinate.
				s_clipParam = div16(ONE_16 - z0, z1 - z0);
				// new vertex Z coordinate should be exactly 1.0
				s_clipPosOut[outVertexCount].z = ONE_16;
				s_clipPosOut[outVertexCount].y = s_clipPos0->y + mul16(s_clipParam, s_clipPos1->y - s_clipPos0->y);
				s_clipPosOut[outVertexCount].x = s_clipPos0->x + mul16(s_clipParam, s_clipPos1->x - s_clipPos0->x);

				fixed16_16 i0 = *s_clipIntensity0;
				fixed16_16 i1 = *s_clipIntensity1;
				s_clipIntensityOut[outVertexCount] = i0 + mul16(s_clipParam, i1 - i0);

				outVertexCount++;
			}

			s_clipPos0 = s_clipPos1;
			s_clipIntensity0 = s_clipIntensity1;
			s_clipPos1++;
			s_clipIntensity1++;
		}
		// Return if the polygon is clipping away.
		if (outVertexCount < 3)
		{
			return 0;
		}
		s32 srcVertexCount = model_swapClipBuffers(outVertexCount);
		outVertexCount = 0;

		///////////////////////////////////////////////////
		// Left Clip Plane
		///////////////////////////////////////////////////
		for (s32 i = 0; i < srcVertexCount; i++)
		{
			s_clipPlanePos0 = -s_clipPos0->z;
			s_clipPlanePos1 = -s_clipPos1->z;
			if (s_clipPos0->x < s_clipPlanePos0 && s_clipPos1->x < s_clipPlanePos1)
			{
				s_clipPos0 = s_clipPos1;
				s_clipIntensity0 = s_clipIntensity1;
				s_clipPos1++;
				s_clipIntensity1++;
				continue;
			}

			// Add vertex 0 if it is inside the plane.
			if (s_clipPos0->x >= s_clipPlanePos0)
			{
				s_clipPosOut[outVertexCount] = *s_clipPos0;
				s_clipIntensityOut[outVertexCount] = *s_clipIntensity0;
				assert(s_clipPosOut[outVertexCount].z >= ONE_16);
				outVertexCount++;
			}

			// Skip clipping if either vertex is touching the plane.
			if (s_clipPos0->x == s_clipPlanePos0 || s_clipPos1->x == s_clipPlanePos1)
			{
				s_clipPos0 = s_clipPos1;
				s_clipIntensity0 = s_clipIntensity1;
				s_clipPos1++;
				s_clipIntensity1++;
				continue;
			}

			// Clip the edge.
			if (s_clipPos0->x < s_clipPlanePos0 || s_clipPos1->x < s_clipPlanePos1)
			{
				fixed16_16 x0 = s_clipPos0->x;
				fixed16_16 x1 = s_clipPos1->x;
				fixed16_16 z0 = s_clipPos0->z;
				fixed16_16 z1 = s_clipPos1->z;

				fixed16_16 dx = s_clipPos1->x - s_clipPos0->x;
				fixed16_16 dz = s_clipPos1->z - s_clipPos0->z;

				s_clipParam0 = mul16(x0, z1) - mul16(x1, z0);
				s_clipParam1 = -dz - dx;

				s_clipIntersectZ = s_clipParam0;
				if (s_clipParam1 != 0)
				{
					s_clipIntersectZ = div16(s_clipParam0, s_clipParam1);
				}
				s_clipIntersectX = -s_clipIntersectZ;

				fixed16_16 p, p0, p1;
				if (abs(dz) > abs(dx))
				{
					p1 = s_clipPos1->z;
					p0 = s_clipPos0->z;
					p = s_clipIntersectZ;
				}
				else
				{
					p1 = s_clipPos1->x;
					p0 = s_clipPos0->x;
					p = s_clipIntersectX;
				}
				s_clipParam = div16(p - p0, p1 - p0);

				s_clipPosOut[outVertexCount].x = s_clipIntersectX;
				s_clipPosOut[outVertexCount].y = s_clipPos0->y + mul16(s_clipParam, s_clipPos1->y - s_clipPos0->y);
				s_clipPosOut[outVertexCount].z = s_clipIntersectZ;
				assert(s_clipPosOut[outVertexCount].z >= ONE_16);

				fixed16_16 i0 = *s_clipIntensity0;
				fixed16_16 i1 = *s_clipIntensity1;
				s_clipIntensityOut[outVertexCount] = i0 + mul16(s_clipParam, i1 - i0);

				outVertexCount++;
			}
			s_clipPos0 = s_clipPos1;
			s_clipIntensity0 = s_clipIntensity1;
			s_clipPos1++;
			s_clipIntensity1++;
		}
		if (outVertexCount < 3)
		{
			return 0;
		}
		srcVertexCount = model_swapClipBuffers(outVertexCount);
		outVertexCount = 0;

		///////////////////////////////////////////////////
		// Right Clip Plane
		///////////////////////////////////////////////////
		for (s32 i = 0; i < srcVertexCount; i++)
		{
			s_clipPlanePos0 = s_clipPos0->z;
			s_clipPlanePos1 = s_clipPos1->z;
			if (s_clipPos0->x > s_clipPlanePos0 && s_clipPos1->x > s_clipPlanePos1)
			{
				s_clipPos0 = s_clipPos1;
				s_clipIntensity0 = s_clipIntensity1;
				s_clipPos1++;
				s_clipIntensity1++;

				continue;
			}

			// Add vertex 0 if it is inside the plane.
			if (s_clipPos0->x <= s_clipPlanePos0)
			{
				s_clipPosOut[outVertexCount] = *s_clipPos0;
				s_clipIntensityOut[outVertexCount] = *s_clipIntensity0;
				assert(s_clipPosOut[outVertexCount].z >= ONE_16);
				outVertexCount++;
			}

			// Skip clipping if either vertex is touching the plane.
			if (s_clipPos0->x == s_clipPlanePos0 || s_clipPos1->x == s_clipPlanePos1)
			{
				s_clipPos0 = s_clipPos1;
				s_clipIntensity0 = s_clipIntensity1;
				s_clipPos1++;
				s_clipIntensity1++;

				continue;
			}

			// Clip the edge.
			if (s_clipPos0->x > s_clipPlanePos0 || s_clipPos1->x > s_clipPlanePos1)
			{
				fixed16_16 x0 = s_clipPos0->x;
				fixed16_16 x1 = s_clipPos1->x;
				fixed16_16 z0 = s_clipPos0->z;
				fixed16_16 z1 = s_clipPos1->z;

				fixed16_16 dx = s_clipPos1->x - s_clipPos0->x;
				fixed16_16 dz = s_clipPos1->z - s_clipPos0->z;

				s_clipParam0 = mul16(x0, z1) - mul16(x1, z0);
				s_clipParam1 = dz - dx;

				s_clipIntersectZ = s_clipParam0;
				if (s_clipParam1 != 0)
				{
					s_clipIntersectZ = div16(s_clipParam0, s_clipParam1);
				}
				s_clipIntersectX = s_clipIntersectZ;

				fixed16_16 p, p0, p1;
				if (abs(dz) > abs(dx))
				{
					p1 = s_clipPos1->z;
					p0 = s_clipPos0->z;
					p = s_clipIntersectZ;
				}
				else
				{
					p1 = s_clipPos1->x;
					p0 = s_clipPos0->x;
					p = s_clipIntersectX;
				}
				s_clipParam = div16(p - p0, p1 - p0);

				s_clipPosOut[outVertexCount].x = s_clipIntersectX;
				s_clipPosOut[outVertexCount].y = s_clipPos0->y + mul16(s_clipParam, s_clipPos1->y - s_clipPos0->y);	// edx
				s_clipPosOut[outVertexCount].z = s_clipIntersectZ;
				assert(s_clipPosOut[outVertexCount].z >= ONE_16);

				fixed16_16 i0 = *s_clipIntensity0;
				fixed16_16 i1 = *s_clipIntensity1;
				s_clipIntensityOut[outVertexCount] = i0 + mul16(s_clipParam, i1 - i0);

				outVertexCount++;
			}
			s_clipPos0 = s_clipPos1;
			s_clipIntensity0 = s_clipIntensity1;
			s_clipPos1++;
			s_clipIntensity1++;
		}
		if (outVertexCount < 3)
		{
			return 0;
		}
				
		srcVertexCount = model_swapClipBuffers(outVertexCount);
		outVertexCount = 0;

		///////////////////////////////////////////////////
		// Top Clip Plane
		///////////////////////////////////////////////////
		for (s32 i = 0; i < srcVertexCount; i++)
		{
			s_clipY0 = mul16(s_yPlaneTop, s_clipPos0->z);
			s_clipY1 = mul16(s_yPlaneTop, s_clipPos1->z);

			// If the edge is completely behind the plane, then continue.
			if (s_clipPos0->y < s_clipY0 && s_clipPos1->y < s_clipY1)
			{
				s_clipPos0 = s_clipPos1;
				s_clipIntensity0 = s_clipIntensity1;
				s_clipPos1++;
				s_clipIntensity1++;

				continue;
			}
			// Add vertex 0 if it is inside the plane.
			if (s_clipPos0->y > s_clipY0)
			{
				s_clipPosOut[outVertexCount] = *s_clipPos0;
				s_clipIntensityOut[outVertexCount] = *s_clipIntensity0;
				outVertexCount++;
			}

			// Skip clipping if either vertex is touching the plane.
			if (s_clipPos0->y == s_clipY0 || s_clipPos1->y == s_clipY1)
			{
				s_clipPos0 = s_clipPos1;
				s_clipIntensity0 = s_clipIntensity1;
				s_clipPos1++;
				s_clipIntensity1++;

				continue;
			}

			// Clip the edge.
			if (s_clipPos0->y < s_clipY0 || s_clipPos1->y < s_clipY1)
			{
				s_clipParam0 = mul16(s_clipPos0->y, s_clipPos1->z) - mul16(s_clipPos1->y, s_clipPos0->z);

				fixed16_16 dy = s_clipPos1->y - s_clipPos0->y;
				fixed16_16 dz = s_clipPos1->z - s_clipPos0->z;
				s_clipParam1 = mul16(s_yPlaneTop, dz) - dy;

				s_clipIntersectZ = s_clipParam0;
				if (s_clipParam1 != 0)
				{
					s_clipIntersectZ = div16(s_clipParam0, s_clipParam1);
				}
				s_clipIntersectY = mul16(s_yPlaneTop, s_clipIntersectZ);
				fixed16_16 aDz = abs(s_clipPos1->z - s_clipPos0->z);
				fixed16_16 aDy = abs(s_clipPos1->y - s_clipPos0->y);

				fixed16_16 p, p0, p1;
				if (aDz > aDy)
				{
					p1 = s_clipPos1->z;
					p0 = s_clipPos0->z;
					p = s_clipIntersectZ;
				}
				else
				{
					p1 = s_clipPos1->y;
					p0 = s_clipPos0->y;
					p = s_clipIntersectY;
				}
				s_clipParam = div16(p - p0, p1 - p0);

				s_clipPosOut[outVertexCount].x = s_clipPos0->x + mul16(s_clipParam, s_clipPos1->x - s_clipPos0->x);
				s_clipPosOut[outVertexCount].y = s_clipIntersectY;
				s_clipPosOut[outVertexCount].z = s_clipIntersectZ;

				fixed16_16 i0 = *s_clipIntensity0;
				fixed16_16 i1 = *s_clipIntensity1;
				s_clipIntensityOut[outVertexCount] = i0 + mul16(s_clipParam, i1 - i0);

				outVertexCount++;
			}
			s_clipPos0 = s_clipPos1;
			s_clipIntensity0 = s_clipIntensity1;
			s_clipPos1++;
			s_clipIntensity1++;
		}

		if (outVertexCount < 3)
		{
			return 0;
		}
		srcVertexCount = model_swapClipBuffers(outVertexCount);
		outVertexCount = 0;

		///////////////////////////////////////////////////
		// Bottom Clip Plane
		///////////////////////////////////////////////////
		for (s32 i = 0; i < srcVertexCount; i++)
		{
			s_clipY0 = mul16(s_yPlaneBot, s_clipPos0->z);
			s_clipY1 = mul16(s_yPlaneBot, s_clipPos1->z);

			// If the edge is completely behind the plane, then continue.
			if (s_clipPos0->y > s_clipY0 && s_clipPos1->y > s_clipY1)
			{
				s_clipPos0 = s_clipPos1;
				s_clipIntensity0 = s_clipIntensity1;
				s_clipPos1++;
				s_clipIntensity1++;
				continue;
			}

			// Add vertex 0 if it is inside the plane.
			if (s_clipPos0->y < s_clipY0)
			{
				s_clipPosOut[outVertexCount] = *s_clipPos0;
				s_clipIntensityOut[outVertexCount] = *s_clipIntensity0;
				outVertexCount++;
			}

			// Skip clipping if either vertex is touching the plane.
			if (s_clipPos0->y == s_clipY0 || s_clipPos1->y == s_clipY1)
			{
				s_clipPos0 = s_clipPos1;
				s_clipIntensity0 = s_clipIntensity1;
				s_clipPos1++;
				s_clipIntensity1++;
				continue;
			}

			// Clip the edge.
			if (s_clipPos0->y > s_clipY0 || s_clipPos1->y > s_clipY1)
			{
				s_clipParam0 = mul16(s_clipPos0->y, s_clipPos1->z) - mul16(s_clipPos1->y, s_clipPos0->z);

				fixed16_16 dy = s_clipPos1->y - s_clipPos0->y;
				fixed16_16 dz = s_clipPos1->z - s_clipPos0->z;
				s_clipParam1 = mul16(s_yPlaneBot, dz) - dy;

				s_clipIntersectZ = s_clipParam0;
				if (s_clipParam1 != 0)
				{
					s_clipIntersectZ = div16(s_clipParam0, s_clipParam1);
				}
				s_clipIntersectY = mul16(s_yPlaneBot, s_clipIntersectZ);
				fixed16_16 aDz = abs(s_clipPos1->z - s_clipPos0->z);
				fixed16_16 aDy = abs(s_clipPos1->y - s_clipPos0->y);

				fixed16_16 p, p0, p1;
				if (aDz > aDy)
				{
					p1 = s_clipPos1->z;
					p0 = s_clipPos0->z;
					p = s_clipIntersectZ;
				}
				else
				{
					p1 = s_clipPos1->y;
					p0 = s_clipPos0->y;
					p = s_clipIntersectY;
				}
				s_clipParam = div16(p - p0, p1 - p0);

				s_clipPosOut[outVertexCount].x = s_clipPos0->x + mul16(s_clipParam, s_clipPos1->x - s_clipPos0->x);
				s_clipPosOut[outVertexCount].y = s_clipIntersectY;
				s_clipPosOut[outVertexCount].z = s_clipIntersectZ;

				fixed16_16 i0 = *s_clipIntensity0;
				fixed16_16 i1 = *s_clipIntensity1;
				s_clipIntensityOut[outVertexCount] = i0 + mul16(s_clipParam, i1 - i0);

				outVertexCount++;
			}
			s_clipPos0 = s_clipPos1;
			s_clipIntensity0 = s_clipIntensity1;
			s_clipPos1++;
			s_clipIntensity1++;
		}
		if (outVertexCount < 3)
		{
			return 0;
		}

		if (pos != s_clipPosOut)
		{
			memcpy(pos, s_clipPosOut, outVertexCount * sizeof(vec3_fixed));
			memcpy(intensity, s_clipIntensityOut, outVertexCount * sizeof(fixed16_16));
		}
		return outVertexCount;
	}

	void model_drawColumnColor()
	{
		fixed16_16 intensity = s_col_I0;
		const u8* colorMap = s_polyColorMap;
		u8* colorOut = s_pcolumnOut;
		u8  colorIndex = s_polyColorIndex;
		s32 count = s_columnHeight;
		s32 dither = s_dither;

		s32 end = s_columnHeight - 1;
		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width)
		{
			s32 pixelIntensity = floor16(intensity);
			if (dither)
			{
				fixed16_16 iOffset = intensity - s_ditherOffset;
				if (iOffset >= 0)
				{
					pixelIntensity = floor16(iOffset);
				}
			}
			s_pcolumnOut[offset] = colorMap[pixelIntensity*256 + colorIndex];

			intensity += s_col_dIdY;
			dither = !dither;
		}
	}

	s32 model_findNextEdge(s32 xMinIndex, s32 xMin)
	{
		s32 prevScanlineLen = s_edgeTopLength;
		s32 curIndex = xMinIndex;

		// The min and max indices should not match, otherwise it is an error.
		if (xMinIndex == s_polyMaxXIndex)
		{
			s_edgeTopLength = prevScanlineLen;
			return -1;
		}

		while (1)
		{
			s32 nextIndex = curIndex + 1;
			if (nextIndex >= s_polyVertexCount) { nextIndex = 0; }
			else if (nextIndex < 0) { nextIndex = s_polyVertexCount - 1; }

			vec3_fixed* cur = &s_polyProjVtx[curIndex];
			vec3_fixed* next = &s_polyProjVtx[nextIndex];
			s32 dx = next->x - cur->x;
			if (next->x == s_maxScreenX)
			{
				dx++;
			}

			if (dx > 0)
			{
				s_edgeTopLength = dx;

				fixed16_16 step = div16(ONE_16, intToFixed16(dx));
				s_edgeTopY0_Pixel = cur->y;
				s_edgeTop_Y0 = intToFixed16(cur->y);

				fixed16_16 dy = intToFixed16(next->y - cur->y);
				s_edgeTop_dYdX = mul16(dy, step);
				
				fixed16_16 dz = next->z - cur->z;
				s_edgeTop_dZdX = mul16(dz, step);
				s_edgeTop_Z0 = cur->z;

				s_edgeTop_I0 = s_polyIntensity[curIndex];
				if (s_edgeTop_I0 <= 0) { s_edgeTop_I0 = 0; }
				if (s_edgeTop_I0 > VSHADE_MAX_INTENSITY) { s_edgeTop_I0 = VSHADE_MAX_INTENSITY; }

				fixed16_16 dI = s_polyIntensity[nextIndex] - s_edgeTop_I0;
				s_edgeTop_dIdX = mul16(dI, step);
				s_edgeTopIndex = nextIndex;
				return 0;
			}
			else if (nextIndex == s_polyMaxXIndex)
			{
				s_edgeTopLength = prevScanlineLen;
				return -1;
			}
			curIndex = nextIndex;
		}

		// This shouldn't be reached, but just in case.
		s_edgeTopLength = prevScanlineLen;
		return -1;
	}

	s32 model_findPrevEdge(s32 minXIndex)
	{
		s32 len = s_edgeBotLength;
		s32 curIndex = minXIndex;
		if (minXIndex == s_polyMaxXIndex)
		{
			s_edgeBotLength = len;
			return -1;
		}

		while (1)
		{
			s32 prevIndex = curIndex - 1;
			if (prevIndex >= s_polyVertexCount) { prevIndex = 0; }
			else if (prevIndex < 0) { prevIndex = s_polyVertexCount - 1; }

			vec3_fixed* cur  = &s_polyProjVtx[curIndex];
			vec3_fixed* prev = &s_polyProjVtx[prevIndex];
			s32 dx = prev->x - cur->x;
			if (s_maxScreenX == prev->x)
			{
				dx++;
			}

			if (dx > 0)
			{
				s_edgeBotLength = dx;

				fixed16_16 step = div16(ONE_16, intToFixed16(dx));
				s_edgeBotY0_Pixel = cur->y;
				s_edgeBotY0 = intToFixed16(cur->y);

				s32 dy = prev->y - cur->y;
				s_edgeBot_dYdX = mul16(intToFixed16(dy), step);
				
				fixed16_16 dz = prev->z - cur->z;
				s_edgeBot_dZdX = div16(dz, intToFixed16(dx));
				s_edgeBot_Z0 = cur->z;

				s_edgeBot_I0 = s_polyIntensity[curIndex];
				if (s_edgeBot_I0 < 0) { s_edgeBot_I0 = 0; }
				if (s_edgeBot_I0 > VSHADE_MAX_INTENSITY) { s_edgeBot_I0 = VSHADE_MAX_INTENSITY; }

				fixed16_16 dI = s_polyIntensity[prevIndex] - s_edgeBot_I0;
				s_edgeBot_dIdX = mul16(dI, step);
				s_edgeBotIndex = prevIndex;

				return 0;
			}
			else
			{
				curIndex = prevIndex;
				if (prevIndex == s_polyMaxXIndex)
				{
					s_edgeBotLength = len;
					return -1;
				}
			}
		}
		s_edgeBotLength = len;
		return -1;
	}

	void model_drawColorPolygon(vec3_fixed* projVertices, fixed16_16* intensity, s32 vertexCount, u8 color)
	{
		vec3_fixed* projVertex = projVertices;
		s32 xMax = INT_MIN;
		s32 xMin = INT_MAX;
		s_polyProjVtx     = projVertices;
		s_polyIntensity   = intensity;
		s_polyVertexCount = vertexCount;

		s32 yMax = xMax;
		s32 yMin = xMin;
		if (vertexCount <= 0)
		{
			return;
		}

		// Compute the 2D bounding box of the polygon.
		// Track the extreme vertex indices in X.
		s32 minXIndex;
		for (s32 i = 0; i < s_polyVertexCount; i++, projVertex++)
		{
			s32 x = projVertex->x;
			if (x < xMin)
			{
				xMin = x;
				minXIndex = i;
			}
			if (x > xMax)
			{
				xMax = x;
				s_polyMaxXIndex = i;
			}
			s32 y = projVertex->y;
			if (y < yMin)
			{
				yMin = y;
			}
			if (y > yMax)
			{
				yMax = y;
			}
		}

		// If the polygon is too small or off screen, skip it.
		if (xMin >= xMax || yMin > s_windowMaxY || yMax < s_windowMinY)
		{
			return;
		}

		s_polyColorMap = s_colorMap;
		s_polyColorIndex = color;
		s_ditherOffset = HALF_16;
		s_columnX = xMin;
		if (model_findNextEdge(minXIndex, xMin) != 0 || model_findPrevEdge(minXIndex) != 0)
		{
			return;
		}

		for (s32 foundEdge = 0; !foundEdge && s_columnX >= s_minScreenX && s_columnX <= s_maxScreenX; s_columnX++)
		{
			//fixed16_16 edgeAveZ0 = (s_edgeBot_Z0 + s_edgeTop_Z0) >> 1;
			// This produces more accurate results, look into the difference (it might be a difference between gouraud and textured).
			fixed16_16 edgeAveZ0 = min(s_edgeBot_Z0, s_edgeTop_Z0);
			fixed16_16 z = s_depth1d_Fixed[s_columnX];

			// Is ave edge Z occluded by walls? Is edge vertex 0 outside of the vertical area?
			if (edgeAveZ0 < z && s_edgeTopY0_Pixel <= s_windowMaxY && s_edgeBotY0_Pixel >= s_windowMinY)
			{
				s32 winTop = s_objWindowTop[s_columnX];
				s32 y0_Top = s_edgeTopY0_Pixel;
				s32 y0_Bot = s_edgeBotY0_Pixel;
				if (y0_Top < winTop)
				{
					y0_Top = winTop;
				}

				s32 winBot = s_objWindowBot[s_columnX];
				fixed16_16 yOffset = 0;
				if (y0_Bot > winBot)
				{
					yOffset = intToFixed16(y0_Bot - winBot);
					y0_Bot = winBot;
				}
				s_columnHeight = y0_Bot - y0_Top + 1;
				if (s_columnHeight > 0)
				{
					s_pcolumnOut = &s_display[y0_Top*s_width + s_columnX];
					fixed16_16 height = intToFixed16(s_edgeBotY0_Pixel - s_edgeTopY0_Pixel + 1);
					s_col_dIdY = div16(s_edgeTop_I0 - s_edgeBot_I0, height);
					s_col_I0 = s_edgeBot_I0;
					if (yOffset)
					{
						s_col_I0 = mul16(yOffset, s_col_dIdY) + s_edgeBot_I0;
					}
					s_dither = ((s_columnX & 1) ^ (y0_Bot & 1)) - 1;
					model_drawColumnColor();
				}
			}

			s_edgeTopLength--;
			if (s_edgeTopLength <= 0)
			{
				foundEdge = model_findNextEdge(s_edgeTopIndex, s_columnX);
			}
			else
			{
				s_edgeTop_I0 += s_edgeTop_dIdX;
				if (s_edgeTop_I0 < 0) { s_edgeTop_I0 = 0; }
				if (s_edgeTop_I0 > VSHADE_MAX_INTENSITY) { s_edgeTop_I0 = VSHADE_MAX_INTENSITY; }

				s_edgeTop_Y0 += s_edgeTop_dYdX;
				s_edgeTopY0_Pixel = round16(s_edgeTop_Y0);
				s_edgeTop_Z0 += s_edgeTop_dZdX;
			}
			if (foundEdge == 0)
			{
				s_edgeBotLength--;
				if (s_edgeBotLength <= 0)
				{
					foundEdge = model_findPrevEdge(s_edgeBotIndex);
				}
				else
				{
					s_edgeBot_I0 += s_edgeBot_dIdX;
					if (s_edgeBot_I0 < 0) { s_edgeBot_I0 = 0; }
					if (s_edgeBot_I0 > VSHADE_MAX_INTENSITY) { s_edgeBot_I0 = VSHADE_MAX_INTENSITY; }

					s_edgeBotY0 += s_edgeBot_dYdX;
					s_edgeBotY0_Pixel = round16(s_edgeBotY0);
					s_edgeBot_Z0 += s_edgeBot_dZdX;
				}
			}
		}
	}

}}  // TFE_JediRenderer