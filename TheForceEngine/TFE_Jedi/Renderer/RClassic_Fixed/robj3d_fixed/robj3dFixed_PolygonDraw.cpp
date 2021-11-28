#include <climits>

#include <TFE_System/profiler.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include "robj3dFixed_TransformAndLighting.h"
#include "robj3dFixed_PolygonSetup.h"
#include "robj3dFixed_Clipping.h"
#include "../rsectorFixed.h"
#include "../rflatFixed.h"
#include "../rclassicFixedSharedState.h"
#include "../rlightingFixed.h"
#include "../../rcommon.h"

namespace TFE_Jedi
{

namespace RClassic_Fixed
{
	////////////////////////////////////////////////
	// Polygon Drawing
	////////////////////////////////////////////////
	// Polygon
	static u8  s_polyColorIndex;
	static s32 s_polyVertexCount;
	static s32 s_polyMaxIndex;
	static fixed16_16* s_polyIntensity;
	static vec2_fixed* s_polyUv;
	static vec3_fixed* s_polyProjVtx;
	static const u8*   s_polyColorMap;
	static TextureData* s_polyTexture;

	// Column
	static s32 s_columnX;
	static s32 s_rowY;
	static s32 s_columnHeight;
	static s32 s_dither;
	static u8* s_pcolumnOut;
		
	static fixed16_16  s_col_I0;
	static fixed16_16  s_col_dIdY;
	static vec2_fixed  s_col_Uv0;
	static vec2_fixed  s_col_dUVdY;

	// Polygon Edges
	static fixed16_16  s_ditherOffset;
	// Bottom Edge
	static fixed16_16  s_edgeBot_Z0;
	static fixed16_16  s_edgeBot_dZdX;
	static fixed16_16  s_edgeBot_dIdX;
	static fixed16_16  s_edgeBot_I0;
	static vec2_fixed  s_edgeBot_dUVdX;
	static vec2_fixed  s_edgeBot_Uv0;
	static fixed16_16  s_edgeBot_dYdX;
	static fixed16_16  s_edgeBot_Y0;
	// Top Edge
	static fixed16_16  s_edgeTop_dIdX;
	static vec2_fixed  s_edgeTop_dUVdX;
	static vec2_fixed  s_edgeTop_Uv0;
	static fixed16_16  s_edgeTop_dYdX;
	static fixed16_16  s_edgeTop_Z0;
	static fixed16_16  s_edgeTop_Y0;
	static fixed16_16  s_edgeTop_dZdX;
	static fixed16_16  s_edgeTop_I0;
	// Left Edge
	static fixed16_16  s_edgeLeft_X0;
	static fixed16_16  s_edgeLeft_Z0;
	static fixed16_16  s_edgeLeft_dXdY;
	static fixed16_16  s_edgeLeft_dZmdY;
	// Right Edge
	static fixed16_16  s_edgeRight_X0;
	static fixed16_16  s_edgeRight_Z0;
	static fixed16_16  s_edgeRight_dXdY;
	static fixed16_16  s_edgeRight_dZmdY;
	// Edge Pixels & Indices
	static s32 s_edgeBotY0_Pixel;
	static s32 s_edgeTopY0_Pixel;
	static s32 s_edgeLeft_X0_Pixel;
	static s32 s_edgeRight_X0_Pixel;
	static s32 s_edgeBotIndex;
	static s32 s_edgeTopIndex;
	static s32 s_edgeLeftIndex;
	static s32 s_edgeRightIndex;
	static s32 s_edgeTopLength;
	static s32 s_edgeBotLength;
	static s32 s_edgeLeftLength;
	static s32 s_edgeRightLength;

	u8 robj3d_computePolygonColor(vec3_fixed* normal, u8 color, fixed16_16 z)
	{
		if (s_sectorAmbient >= 31) { return color; }
		s_polyColorMap = s_colorMap;

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

		if (s_worldAmbient < 31 || s_cameraLightSource)
		{
			const s32 depthScaled = min(s32(z >> 14), 127);
			const s32 cameraSource = MAX_LIGHT_LEVEL - s_lightSourceRamp[depthScaled] + s_worldAmbient;
			if (cameraSource > 0)
			{
				lightLevel += cameraSource;
			}
		}

		z = max(z, 0);
		const s32 falloff = (z >> 15) + (z >> 14);	// z * 0.75
		lightLevel = max(lightLevel, s_sectorAmbient);
		lightLevel = max(lightLevel - falloff, s_scaledAmbient);

		if (lightLevel >= 31) { return color; }
		if (lightLevel <= 0) { return s_polyColorMap[color]; }

		return s_polyColorMap[lightLevel*256 + color];
	}

	u8 robj3d_computePolygonLightLevel(vec3_fixed* normal, fixed16_16 z)
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

		if (s_worldAmbient < 31 || s_cameraLightSource)
		{
			const s32 depthScaled = min(s32(z >> 14), 127);
			const s32 cameraSource = MAX_LIGHT_LEVEL - s_lightSourceRamp[depthScaled] + s_worldAmbient;
			if (cameraSource > 0)
			{
				lightLevel += cameraSource;
			}
		}

		z = max(z, 0);
		const s32 falloff = (z >> 15) + (z >> 14);	// z * 0.75
		lightLevel = max(lightLevel, s_sectorAmbient);
		lightLevel = max(lightLevel - falloff, s_scaledAmbient);

		return clamp(lightLevel, 0, 31);
	}
		
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

	////////////////////////////////////////////
	// Polygon Draw Routine for Shading = PLANE
	// and support functions.
	////////////////////////////////////////////
	s32 robj3d_findRightEdge(s32 minIndex)
	{
		s32 len = s_edgeRightLength;
		if (minIndex == s_polyMaxIndex)
		{
			s_edgeRightLength = len;
			return -1;
		}

		s32 curIndex = minIndex;
		while (1)
		{
			s32 nextIndex = curIndex + 1;
			if (nextIndex >= s_polyVertexCount) { nextIndex = 0; }
			else if (nextIndex < 0) { nextIndex = s_polyVertexCount - 1; }

			const vec3_fixed* cur = &s_polyProjVtx[curIndex];
			const vec3_fixed* next = &s_polyProjVtx[nextIndex];
			const s32 y0 = cur->y;
			const s32 y1 = next->y;

			s32 dy = y1 - y0;
			if (y1 == s_maxScreenY) { dy++; }

			if (dy > 0)
			{
				const s32 x0 = cur->x;
				const s32 x1 = next->x;
				const fixed16_16 dX = intToFixed16(x1 - x0);
				const fixed16_16 dY = intToFixed16(dy);
				const fixed16_16 dXdY = div16(dX, dY);

				s_edgeRight_X0_Pixel = x0;
				s_edgeRight_X0 = intToFixed16(x0);
				s_edgeRightLength = dy;

				s_edgeRight_dXdY = dXdY;
				s_edgeRight_Z0 = cur->z;

				s_edgeRight_dZmdY = mul16(next->z - cur->z, dY);
				s_edgeRightIndex = nextIndex;
				return 0;
			}
			else
			{
				curIndex = nextIndex;
				if (nextIndex == s_polyMaxIndex)
				{
					break;
				}
			}
		}

		s_edgeRightLength = len;
		return -1;
	}

	s32 robj3d_findLeftEdge(s32 minIndex)
	{
		s32 len = s_edgeLeftLength;
		if (minIndex == s_polyMaxIndex)
		{
			s_edgeLeftLength = len;
			return -1;
		}

		s32 curIndex = minIndex;
		while (1)
		{
			s32 prevIndex = curIndex - 1;
			if (prevIndex >= s_polyVertexCount) { prevIndex = 0; }
			else if (prevIndex < 0) { prevIndex = s_polyVertexCount - 1; }

			const vec3_fixed* cur = &s_polyProjVtx[curIndex];
			const vec3_fixed* prev = &s_polyProjVtx[prevIndex];
			const s32 y0 = cur->y;
			const s32 y1 = prev->y;

			s32 dy = y1 - y0;
			if (y1 == s_maxScreenY) { dy++; }

			if (dy > 0)
			{
				const s32 x0 = cur->x;
				const s32 x1 = prev->x;
				const fixed16_16 dX = intToFixed16(x1 - x0);
				const fixed16_16 dY = intToFixed16(dy);
				const fixed16_16 dXdY = div16(dX, dY);

				s_edgeLeft_X0_Pixel = x0;
				s_edgeLeft_X0 = intToFixed16(x0);
				s_edgeLeftLength = dy;

				s_edgeLeft_dXdY = dXdY;
				s_edgeLeft_Z0 = cur->z;

				s_edgeLeft_dZmdY = mul16(prev->z - cur->z, dY);
				s_edgeLeftIndex = prevIndex;
				return 0;
			}
			else
			{
				curIndex = prevIndex;
				if (prevIndex == s_polyMaxIndex)
				{
					break;
				}
			}
		}

		s_edgeLeftLength = len;
		return -1;
	}

	void robj3d_drawPlaneTexturePolygon(vec3_fixed* projVertices, s32 vertexCount, TextureData* texture, fixed16_16 planeY, fixed16_16 ceilOffsetX, fixed16_16 ceilOffsetZ, fixed16_16 floorOffsetX, fixed16_16 floorOffsetZ)
	{
		if (vertexCount <= 0) { return; }

		s32 yMin = INT_MAX;
		s32 yMax = INT_MIN;
		s32 minIndex;

		s_polyProjVtx = projVertices;
		s_polyVertexCount = vertexCount;
		
		vec3_fixed* vertex = projVertices;
		for (s32 i = 0; i < s_polyVertexCount; i++, vertex++)
		{
			if (vertex->y < yMin)
			{
				yMin = vertex->y;
				minIndex = i;
			}
			if (vertex->y > yMax)
			{
				yMax = vertex->y;
				s_polyMaxIndex = i;
			}
		}
		if (yMin >= yMax || yMin > s_windowMaxY_Pixels || yMax < s_windowMinY_Pixels)
		{
			return;
		}

		bool trans = (texture->flags & OPACITY_TRANS) != 0;
		s_rowY = yMin;

		if (robj3d_findLeftEdge(minIndex) != 0 || robj3d_findRightEdge(minIndex) != 0)
		{
			return;
		}

		fixed16_16 heightOffset = planeY - s_rcfState.eyeHeight;
		// TODO: Figure out why s_heightInPixels has the wrong sign here.
		if (yMax <= -s_screenYMidFix)
		{
			flat_preparePolygon(heightOffset, ceilOffsetX, ceilOffsetZ, texture);
		}
		else
		{
			flat_preparePolygon(heightOffset, floorOffsetX, floorOffsetZ, texture);
		}

		s32 edgeFound = 0;
		for (; edgeFound == 0 && s_rowY <= s_maxScreenY; s_rowY++)
		{
			if (s_rowY >= s_windowMinY_Pixels && s_windowMaxY_Pixels != 0 && s_edgeLeft_X0_Pixel <= s_windowMaxX_Pixels && s_edgeRight_X0_Pixel >= s_windowMinX_Pixels)
			{
				flat_drawPolygonScanline(s_edgeLeft_X0_Pixel, s_edgeRight_X0_Pixel, s_rowY, trans);
			}

			s_edgeLeftLength--;
			if (s_edgeLeftLength <= 0)
			{
				if (robj3d_findLeftEdge(s_edgeLeftIndex) != 0) { return; }
			}
			else
			{
				s_edgeLeft_X0 += s_edgeLeft_dXdY;
				s_edgeLeft_Z0 += s_edgeLeft_dZmdY;
				s_edgeLeft_X0_Pixel = round16(s_edgeLeft_X0);

				// Right Z0 increment in the wrong place again.
				// TODO: Figure out the consequences of this bug.
				//s_edgeRight_Z0 += s_edgeRight_dZmdY;
			}
			s_edgeRightLength--;
			if (s_edgeRightLength <= 0)
			{
				if (robj3d_findRightEdge(s_edgeRightIndex) != 0) { return; }
			}
			else
			{
				s_edgeRight_X0 += s_edgeRight_dXdY;
				s_edgeRight_X0_Pixel = round16(s_edgeRight_X0);

				// This is the proper place for this.
				s_edgeRight_Z0 += s_edgeRight_dZmdY;
			}
		}
	}

	void robj3d_drawPolygon(Polygon* polygon, s32 polyVertexCount, SecObject* obj, JediModel* model)
	{
		switch (polygon->shading)
		{
			case PSHADE_FLAT:
			{
				u8 color = polygon->color;
				if (s_enableFlatShading)
				{
					color = robj3d_computePolygonColor(&s_polygonNormalsVS[polygon->index], color, polygon->zAve);
				}
				robj3d_drawFlatColorPolygon(s_polygonVerticesProj, polyVertexCount, color);
			} break;
			case PSHADE_GOURAUD:
			{
				robj3d_drawShadedColorPolygon(s_polygonVerticesProj, s_polygonIntensity, polyVertexCount, polygon->color);
			} break;
			case PSHADE_TEXTURE:
			{
				u8 lightLevel = 0;
				if (s_enableFlatShading)
				{
					lightLevel = robj3d_computePolygonLightLevel(&s_polygonNormalsVS[polygon->index], polygon->zAve);
				}
				robj3d_drawFlatTexturePolygon(s_polygonVerticesProj, s_polygonUv, polyVertexCount, polygon->texture, lightLevel);
			} break;
			case PSHADE_GOURAUD_TEXTURE:
			{
				robj3d_drawShadedTexturePolygon(s_polygonVerticesProj, s_polygonUv, s_polygonIntensity, polyVertexCount, polygon->texture);
			} break;
			case PSHADE_PLANE:
			{
				const RSector* sector = obj->sector;
				const fixed16_16 planeY = model->vertices[polygon->indices[0]].y + obj->posWS.y;
				robj3d_drawPlaneTexturePolygon(s_polygonVerticesProj, polyVertexCount, polygon->texture, planeY, sector->ceilOffset.x, sector->ceilOffset.z, sector->floorOffset.x, sector->floorOffset.z);
			} break;
			default:
			{
				TFE_System::logWrite(LOG_ERROR, "Object3D Render Fixed", "Invalid shading mode: %d", polygon->shading);
				assert(0);
			}
		}
	}

}}  // TFE_Jedi