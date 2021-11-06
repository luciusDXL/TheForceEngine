#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include "../rlimits.h"
#include "../rwallRender.h"

struct RSector;
struct EdgePairFixed;

namespace TFE_Jedi
{
	namespace RClassic_Fixed
	{
		struct RWallSegment
		{
			RWall* srcWall;		// Source wall
			s32 wallX0_raw;		// Projected wall X coordinates before adjoin/wall clipping.
			s32 wallX1_raw;
			fixed16_16 z0;		// Depth
			fixed16_16 z1;
			fixed16_16 uCoord0;	// Texture U coordinate offset based on clipping.
			fixed16_16 x0View;	// Viewspace vertex 0 X coordinate.
			s32 wallX0;			// Clipped projected X Coordinates.
			s32 wallX1;
			u8  orient;			// Wall orientation (WORIENT_DZ_DX or WORIENT_DX_DZ)
			u8  pad8[3];
			fixed16_16 slope;	// Wall slope
			fixed16_16 uScale;	// dUdX or dUdZ based on wall orientation.
		};

		void wall_process(RWall* wall);
		s32  wall_mergeSort(RWallSegment* segOutList, s32 availSpace, s32 start, s32 count);

		void wall_drawSolid(RWallSegment* wallSegment);
		void wall_drawTransparent(RWallSegment* wallSegment, EdgePairFixed* edge);
		void wall_drawMask(RWallSegment* wallSegment);
		void wall_drawBottom(RWallSegment* wallSegment);
		void wall_drawTop(RWallSegment* wallSegment);
		void wall_drawTopAndBottom(RWallSegment* wallSegment);

		void wall_drawSkyTop(RSector* sector);
		void wall_drawSkyTopNoWall(RSector* sector);
		void wall_drawSkyBottom(RSector* sector);
		void wall_drawSkyBottomNoWall(RSector* sector);

		void wall_addAdjoinSegment(s32 length, s32 x0, fixed16_16 top_dydx, fixed16_16 y1, fixed16_16 bot_dydx, fixed16_16 y0, RWallSegment* wallSegment);

		void wall_setupAdjoinDrawFlags(RWall* wall);
		void wall_computeTexelHeights(RWall* wall);

		// Sprite code for now because so much is shared.
		void sprite_drawFrame(u8* basePtr, WaxFrame* frame, SecObject* obj);
	}
}
