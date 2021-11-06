#pragma once
//////////////////////////////////////////////////////////////////////
// Fixed-point shared state used by the Jedi Renderer.
// The floating-point sub-renderer will have its own mirrored struct
// of shared state.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/redgePair.h>
#include <TFE_Jedi/Renderer/rlimits.h>
#include <TFE_Jedi/Renderer/rwallSegment.h>

namespace TFE_Jedi
{
	struct RClassicFixedState
	{
		// Resolution
		fixed16_16 halfWidth;
		fixed16_16 halfHeight;
		fixed16_16 projOffsetX;
		fixed16_16 projOffsetY;
		fixed16_16 projOffsetYBase;
		fixed16_16 oneOverHalfWidth;

		// Projection
		fixed16_16  focalLength;
		fixed16_16  focalLenAspect;
		fixed16_16  eyeHeight;
		fixed16_16* depth1d_all;
		fixed16_16* depth1d;

		// Camera
		vec3_fixed  cameraPos;
		vec2_fixed  cameraTrans;
		fixed16_16  cosYaw;
		fixed16_16  sinYaw;
		fixed16_16  negSinYaw;
		fixed16_16  cameraYaw;
		fixed16_16  cameraPitch;
		fixed16_16  yPlaneTop;
		fixed16_16  yPlaneBot;
		fixed16_16  skyYawOffset;
		fixed16_16  skyPitchOffset;
		fixed16_16* skyTable;
		fixed16_16  cameraMtx[9];

		// Window
		fixed16_16 windowMinZ;
		fixed16_16 windowMinY;
		fixed16_16 windowMaxY;
		
		// Column Heights
		fixed16_16* column_Z_Over_X;
		fixed16_16* column_X_Over_Z;

		// Flats
		fixed16_16* rcpY;
		EdgePairFixed* flatEdge;
		EdgePairFixed  flatEdgeList[MAX_SEG];
		EdgePairFixed* adjoinEdge;
		EdgePairFixed  adjoinEdgeList[MAX_ADJOIN_SEG * MAX_ADJOIN_DEPTH];

		RWallSegmentFixed   wallSegListDst[MAX_SEG];
		RWallSegmentFixed   wallSegListSrc[MAX_SEG];
		RWallSegmentFixed** adjoinSegment;
	};
	extern RClassicFixedState s_rcfState;
}  // TFE_Jedi