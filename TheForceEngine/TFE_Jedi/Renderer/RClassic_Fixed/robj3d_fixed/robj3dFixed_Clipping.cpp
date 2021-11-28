#include <cstring>

#include <TFE_System/profiler.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include "robj3dFixed_Clipping.h"
#include "robj3dFixed_PolygonSetup.h"
#include "../rclassicFixedSharedState.h"
#include "../../rcommon.h"

namespace TFE_Jedi
{

namespace RClassic_Fixed
{
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

	s32 clipPolygon(Polygon* polygon)
	{
		s32 polyVertexCount = 0;
		if (polygon->shading == PSHADE_GOURAUD)
		{
			polyVertexCount = robj3d_clipPolygonGouraud(s_polygonVerticesVS, s_polygonIntensity, polygon->vertexCount);
		}
		else if (polygon->shading == PSHADE_TEXTURE)
		{
			polyVertexCount = robj3d_clipPolygonUv(s_polygonVerticesVS, s_polygonUv, polygon->vertexCount);
		}
		else if (polygon->shading == PSHADE_GOURAUD_TEXTURE)
		{
			polyVertexCount = robj3d_clipPolygonUvGouraud(s_polygonVerticesVS, s_polygonUv, s_polygonIntensity, polygon->vertexCount);
		}
		else
		{
			polyVertexCount = robj3d_clipPolygon(s_polygonVerticesVS, polygon->vertexCount);
		}

		return polyVertexCount;
	}

}}  // TFE_Jedi