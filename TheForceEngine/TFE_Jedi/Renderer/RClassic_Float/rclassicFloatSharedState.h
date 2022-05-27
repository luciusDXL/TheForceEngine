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
	struct RClassicFloatState
	{
		// Resolution
		f32 halfWidth;
		f32 halfHeight;
		f32 projOffsetX;
		f32 projOffsetY;
		f32 projOffsetYBase;
		f32 oneOverHalfWidth;

		// Projection
		f32  focalLength;
		f32  focalLenAspect;
		f32  eyeHeight;
		f32* depth1d_all;
		f32* depth1d;

		// Camera
		vec3_float cameraPos;
		vec2_float cameraTrans;
		f32  cosYaw;
		f32  sinYaw;
		f32  negSinYaw;
		f32  cameraYaw;
		f32  cameraPitch;
		f32  yPlaneTop;
		f32  yPlaneBot;
		f32  skyYawOffset;
		f32  skyPitchOffset;
		f32* skyTable;
		f32  cameraMtx[9];
		f32  aspectScaleX;
		f32  aspectScaleY;
		f32  nearPlaneHalfLen;

		// Window
		f32 windowMinZ;
		f32 windowMinY;
		f32 windowMaxY;

		// Flats
		EdgePairFloat* flatEdge;
		EdgePairFloat  flatEdgeList[MAX_SEG];
		EdgePairFloat* adjoinEdge;
		EdgePairFloat  adjoinEdgeList[MAX_ADJOIN_SEG_EXT * MAX_ADJOIN_DEPTH_EXT];

		RWallSegmentFloat   wallSegListDst[MAX_SEG];
		RWallSegmentFloat   wallSegListSrc[MAX_SEG];
		RWallSegmentFloat** adjoinSegment;
	};
	extern RClassicFloatState s_rcfltState;
}  // TFE_Jedi