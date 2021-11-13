#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/rwall.h>

namespace TFE_Jedi
{
	struct WallCached;

	struct RWallSegmentFixed
	{
		RWall* srcWall;		// Source wall
		s32 wallX0_raw;		// Projected wall X coordinates before adjoin/wall clipping.
		s32 wallX1_raw;
		s32 wallX0;			// Clipped projected X Coordinates.
		s32 wallX1;
		u8  orient;			// Wall orientation (WORIENT_DZ_DX or WORIENT_DX_DZ)
		u8  pad8[3];

		fixed16_16 z0;		// Depth
		fixed16_16 z1;
		fixed16_16 uCoord0;	// Texture U coordinate offset based on clipping.
		fixed16_16 x0View;	// Viewspace vertex 0 X coordinate.

		fixed16_16 slope;	// Wall slope
		fixed16_16 uScale;	// dUdX or dUdZ based on wall orientation.
	};

	struct RWallSegmentFloat
	{
		WallCached* srcWall;// Source wall
		s32 wallX0_raw;		// Projected wall X coordinates before adjoin/wall clipping.
		s32 wallX1_raw;
		s32 wallX0;			// Clipped projected X Coordinates.
		s32 wallX1;
		u8  orient;			// Wall orientation (WORIENT_DZ_DX or WORIENT_DX_DZ)
		u8  pad8[3];

		f32 z0;				// Depth
		f32 z1;
		f32 uCoord0;		// Texture U coordinate offset based on clipping.
		f32 x0View;			// Viewspace vertex 0 X coordinate.

		f32 slope;			// Wall slope
		f32 uScale;			// dUdX or dUdZ based on wall orientation.
	};
}
