#include <TFE_System/profiler.h>
#include <TFE_RenderBackend/renderBackend.h>
// TODO: Fix or move.
#include <TFE_Game/level.h>

#include "rwallFloat.h"
#include "rflatFloat.h"
#include "rlightingFloat.h"
#include "rsectorFloat.h"
#include "redgePairFloat.h"
#include "fixedPoint20.h"
#include "../rmath.h"
#include "../rcommon.h"
#include "../robject.h"

namespace TFE_JediRenderer
{

namespace RClassic_Float
{
	#define SKY_BASE_HEIGHT 200

	enum SegSide
	{
		FRONT = 0xffff,
		BACK = 0,
	};

	static f32 s_segmentCross;
	static s32 s_texHeightMask;
	static s32 s_yPixelCount;
	static fixed44_20 s_vCoordStep;
	static fixed44_20 s_vCoordFixed;
	static const u8* s_columnLight;
	static u8* s_texImage;
	static u8* s_columnOut;
	static u8  s_workBuffer[1024];

	s32 segmentCrossesLine(f32 ax0, f32 ay0, f32 ax1, f32 ay1, f32 bx0, f32 by0, f32 bx1, f32 by1);
	f32 solveForZ_Numerator(RWallSegment* wallSegment);
	f32 solveForZ(RWallSegment* wallSegment, s32 x, f32 numerator, f32* outViewDx=nullptr);
	void drawColumn_Fullbright();
	void drawColumn_Lit();
	void drawColumn_Fullbright_Trans();
	void drawColumn_Lit_Trans();

	// Column rendering functions that can be chosen at runtime.
	enum ColumnFuncId
	{
		COLFUNC_FULLBRIGHT = 0,
		COLFUNC_LIT,
		COLFUNC_FULLBRIGHT_TRANS,
		COLFUNC_LIT_TRANS,

		COLFUNC_COUNT
	};

	typedef void(*ColumnFunction)();
	ColumnFunction s_columnFunc[COLFUNC_COUNT] =
	{
		drawColumn_Fullbright,			// COLFUNC_FULLBRIGHT
		drawColumn_Lit,					// COLFUNC_LIT
		drawColumn_Fullbright_Trans,	// COLFUNC_FULLBRIGHT_TRANS
		drawColumn_Lit_Trans,			// COLFUNC_LIT_TRANS
	};

	// Computes the intersection of line segment (x0,z0),(x1,z1) with frustum line (fx0, fz0),(fx1, fz1)
	f32 frustumIntersectParam(f32 x0, f32 z0, f32 x1, f32 z1, f32 fx0, f32 fz0, f32 fx1, f32 fz1)
	{
		f32 ax = x1 - x0;		// Segment 'a' - this is the wall segment.
		f32 az = z1 - z0;
		f32 bx = fx1 - fx0;		// Segment 'b' - this defines the frustum line.
		f32 bz = fz1 - fz0;
		f32 ux = fx0 - x0;		// Segment 'u' = b0 - a0
		f32 uz = fz0 - z0;

		// s = BxU / BxA
		f32 param = bx*uz - bz*ux;
		f32 den   = bx*az - bz*ax;
		if (fabsf(den) >= FLT_EPSILON)
		{
			param /= den;
		}

		return param;
	}

	f32 frustumIntersect(f32 x0, f32 z0, f32 x1, f32 z1, f32 dx, f32 dz)
	{
		f32 xz;
		xz = (x0 * z1) - (z0 * x1);
		f32 dyx = dz - dx;
		if (dyx != 0.0f)
		{
			xz /= dyx;
		}
		return xz;
	}

	// Returns true if the wall is potentially visible.
	// Original DOS clipping code converted to floating point.
	bool wall_clipToFrustum(f32& x0, f32& z0, f32& x1, f32& z1, f32& dx, f32& dz, f32& curU, f32& texelLen, f32& texelLenRem, s32& clipX0_Near, s32& clipX1_Near, f32 left0, f32 right0, f32 left1, f32 right1)
	{
		//////////////////////////////////////////////
		// Clip the Wall Segment by the left and right
		// frustum lines.
		//////////////////////////////////////////////
		s32 clipLeft = 0;
		s32 clipRight = 0;

		// The wall segment extends past the left clip line.
		if (x0 < left0)
		{
			// Intersect the segment (x0, z0),(x1, z1) with the frustum line that passes through (-z0, z0) and (-z1, z1)
			const f32 xz = frustumIntersect(x0, z0, x1, z1, dx, -dz);

			// Compute the parametric intersection of the segment and the left frustum line
			// where s is in the range of [0.0, 1.0]
			f32 s = 0;
			if (dz != 0 && abs(dz) > abs(dx))
			{
				s = (xz - z0) / dz;
			}
			else if (dx != 0)
			{
				s = (-xz - x0) / dx;
			}

			// Update the x0,y0 coordinate of the segment.
			x0 = -xz;
			z0 = xz;

			if (s != 0)
			{
				// length of the clipped portion of the remaining texel length.
				f32 clipLen = texelLenRem * s;
				// adjust the U texel offset.
				curU += clipLen;
				// adjust the remaining texel length.
				texelLenRem = texelLen - curU;
			}

			// We have clipped the segment on the left side.
			clipLeft = -1;
			// Update dX and dZ
			dx = x1 - x0;
			dz = z1 - z0;
		}
		// The wall segment extends past the right clip line.
		if (x1 > right1)
		{
			// Compute the coordinate where x0 + s*dx = z0 + s*dz
			// Solve for s = (x0 - y0)/(dz - dx)
			// Substitute: x = x0 + ((x0 - z0)/(dz - dx))*dx = (x0*z1 - z0*x1) / (dz - dx)
			const f32 xz = frustumIntersect(x0, z0, x1, z1, dx, dz);

			// Compute the parametric intersection of the segment and the left frustum line
			// where s is in the range of [0.0, 1.0]
			// Note we are computing from the right side, i.e. distance from (x1,y1).
			f32 s = 0;
			if (dz != 0 && abs(dz) > abs(dx))
			{
				s = (xz - z1) / dz;
			}
			else if (dx != 0)
			{
				s = (xz - x1) / dx;
			}

			// Update the x1,y1 coordinate of the segment.
			x1 = xz;
			z1 = xz;
			if (s != 0)
			{
				f32 adjLen = texelLen + (texelLenRem * s);
				f32 adjLenMinU = adjLen - curU;

				texelLen = adjLen;
				texelLenRem = adjLenMinU;
			}

			// We have clipped the segment on the right side.
			clipRight = -1;
			// Update dX and dZ
			dx = x1 - x0;
			dz = z1 - z0;
		}

		//////////////////////////////////////////////////
		// Clip the Wall Segment by the near plane.
		//////////////////////////////////////////////////
		if ((z0 < 0 || z1 < 0) && segmentCrossesLine(0.0f, 0.0f, 0.0f, -s_halfHeight, x0, x0, x1, z1) != 0)
		{
			return false;
		}
		if (z0 < 1.0f && z1 < 1.0f)
		{
			if (clipLeft != 0)
			{
				clipX0_Near = -1;
				x0 = -1.0f;
			}
			else
			{
				x0 /= z0;
			}
			if (clipRight != 0)
			{
				x1 = 1.0f;
				clipX1_Near = -1;
			}
			else
			{
				x1 /= z1;
			}
			dx = x1 - x0;
			dz = 0;
			z0 = 1.0f;
			z1 = 1.0f;
		}
		else if (z0 < 1.0f)
		{
			if (clipLeft != 0)
			{
				if (dz != 0)
				{
					f32 left = z0 / dz;
					x0 += (dx * left);

					dx = x1 - x0;
					texelLenRem = texelLen - curU;
				}
				z0 = 1.0f;
				clipX0_Near = -1;
				dz = z1 - 1.0f;
			}
			else
			{
				// BUG: This is NOT correct but matches the original implementation.
				// I am leaving it as-is to match the original DOS renderer.
				// Fortunately hitting this case is VERY rare in practice.
				x0 /= z0;
				z0 = 1.0f;
				dz = z1 - 1.0f;
				dx -= x0;
			}
		}
		else if (z1 < 1.0f)
		{
			if (clipRight != 0)
			{
				if (dz != 0)
				{
					f32 s = (1.0f - x1) / dz;
					x1 += (dx * s);
					texelLen += (texelLenRem * s);
					texelLenRem = texelLen - curU;
					dx = x1 - x0;
				}
				z1 = 1.0f;
				dz = 1.0f - z0;
				clipX1_Near = -1;
			}
			else
			{
				// BUG: This is NOT correct but matches the original implementation.
				// I am leaving it as-is to match the original DOS renderer.
				// Fortunately hitting this case is VERY rare in practice.
				x1 = x1 / z1;
				z1 = 1.0f;
				dx = x1 - x0;
				dz = z1 - z0;
			}
		}
		return true;
	}

	// Returns true if the wall is potentially visible.
	// Widescreen version, which involves a more complex frustum intersection but allows wider FOV.
	// Original DOS clipping code converted to floating point and then adjusted for widescreen -
	// Note this changes the wall segment/frustum intersection algorithm since the "exactly 90 degree FOV" simplifications can no longer be made but the near plane
	// intersection fix-up code is still the same (which fills holes when walls are clipped by the nearplane).
	bool wall_clipToFrustum_Widescreen(f32& x0, f32& z0, f32& x1, f32& z1, f32& dx, f32& dz, f32& curU, f32& texelLen, f32& texelLenRem, s32& clipX0_Near, s32& clipX1_Near, f32 left0, f32 right0, f32 left1, f32 right1, f32 nearPlaneHalfWidth)
	{
		//////////////////////////////////////////////
		// Clip the Wall Segment by the left and right
		// frustum lines.
		//////////////////////////////////////////////
		s32 clipLeft = 0;
		s32 clipRight = 0;

		// The wall segment extends past the left clip line.
		if (x0 < left0)
		{
			// Intersect the segment (x0, z0),(x1, z1) with the left frustum line.
			const f32 s = frustumIntersectParam(x0, z0, x1, z1, left0, z0, left1, z1);
			x0 += s*(x1 - x0);
			z0 += s*(z1 - z0);

			if (s != 0)
			{
				// length of the clipped portion of the remaining texel length.
				f32 clipLen = texelLenRem * s;
				// adjust the U texel offset.
				curU += clipLen;
				// adjust the remaining texel length.
				texelLenRem = texelLen - curU;
			}

			// We have clipped the segment on the left side.
			clipLeft = -1;
			// Update dX and dZ
			dx = x1 - x0;
			dz = z1 - z0;
			// Update the right nearplane X values.
			right0 = z0 * nearPlaneHalfWidth;
		}
		// The wall segment extends past the right clip line.
		if (x1 > right1)
		{
			// Intersect the segment (x0, z0),(x1, z1) with the left frustum line.
			f32 s = frustumIntersectParam(x0, z0, x1, z1, right0, z0, right1, z1);
			x1 = x0 + s*(x1 - x0);
			z1 = z0 + s*(z1 - z0);

			s = 1.0f - s;
			if (s != 0)
			{
				f32 adjLen = texelLen - texelLenRem*s;
				f32 adjLenMinU = adjLen - curU;

				texelLen = adjLen;
				texelLenRem = adjLenMinU;
			}

			// We have clipped the segment on the right side.
			clipRight = -1;
			// Update dX and dZ
			dx = x1 - x0;
			dz = z1 - z0;
		}

		//////////////////////////////////////////////////
		// Clip the Wall Segment by the near plane.
		//////////////////////////////////////////////////
		if ((z0 < 0 || z1 < 0) && segmentCrossesLine(0.0f, 0.0f, 0.0f, -s_halfHeight, x0, x0, x1, z1) != 0)
		{
			return false;
		}
		if (z0 < 1.0f && z1 < 1.0f)
		{
			if (clipLeft != 0)
			{
				x0 = -nearPlaneHalfWidth;
				clipX0_Near = -1;
			}
			else
			{
				x0 /= z0;
			}
			if (clipRight != 0)
			{
				x1 = nearPlaneHalfWidth;
				clipX1_Near = -1;
			}
			else
			{
				x1 /= z1;
			}
			dx = x1 - x0;
			dz = 0;
			z0 = 1.0f;
			z1 = 1.0f;
		}
		else if (z0 < 1.0f)
		{
			if (clipLeft != 0)
			{
				if (dz != 0)
				{
					f32 left = z0 / dz;
					x0 += (dx * left);

					dx = x1 - x0;
					texelLenRem = texelLen - curU;
				}
				z0 = 1.0f;
				clipX0_Near = -1;
				dz = z1 - 1.0f;
			}
			else
			{
				// BUG: This is NOT correct but matches the original implementation.
				// I am leaving it as-is to match the original DOS renderer.
				// Fortunately hitting this case is VERY rare in practice.
				x0 /= z0;
				z0 = 1.0f;
				dz = z1 - 1.0f;
				dx -= x0;
			}
		}
		else if (z1 < 1.0f)
		{
			if (clipRight != 0)
			{
				if (dz != 0)
				{
					f32 s = (1.0f - x1) / dz;
					x1 += (dx * s);
					texelLen += (texelLenRem * s);
					texelLenRem = texelLen - curU;
					dx = x1 - x0;
				}
				z1 = 1.0f;
				dz = 1.0f - z0;
				clipX1_Near = -1;
			}
			else
			{
				// BUG: This is NOT correct but matches the original implementation.
				// I am leaving it as-is to match the original DOS renderer.
				// Fortunately hitting this case is VERY rare in practice.
				x1 = x1 / z1;
				z1 = 1.0f;
				dx = x1 - x0;
				dz = z1 - z0;
			}
		}
		return true;
	}

	// Process the wall and produce an RWallSegment for rendering if the wall is potentially visible.
	void wall_process(RWall* wall)
	{
		const vec2* p0 = wall->v0;
		const vec2* p1 = wall->v1;

		// Widescreen adjustments are obviously added to the original code.
		// TODO: Factor out and only handle when the resolution changes.
		const bool widescreen = TFE_RenderBackend::getWidescreen();
		const f32 aspectScale = (s_height == 200 || s_height == 400) ? (10.0f / 16.0f) : (3.0f / 4.0f);
		f32 nearPlaneHalfLen = widescreen ? aspectScale * (f32(s_width) / f32(s_height)) : 1.0f;
		// at low resolution, increase the nearPlaneHalfLen slightly to avoid cutting off the last column.
		if (widescreen && s_height == 200)
		{
			nearPlaneHalfLen += 0.001f;
		}

		// Viewspace wall coordinates.
		f32 x0 = p0->x.f32;
		f32 x1 = p1->x.f32;
		f32 z0 = p0->z.f32;
		f32 z1 = p1->z.f32;

		// x values of frustum lines that pass through (x0,z0) and (x1,z1)
		// Note that the 'nearPlaneHalfLen' is 1.0 in the original code and thus omitted (see rwallFixed.cpp).
		f32 left0 = -z0 * nearPlaneHalfLen;
		f32 left1 = -z1 * nearPlaneHalfLen;
		f32 right0 = z0 * nearPlaneHalfLen;
		f32 right1 = z1 * nearPlaneHalfLen;

		// Cull the wall if it is completely beyind the camera.
		if (z0 < 0 && z1 < 0)
		{
			wall->visible = 0;
			return;
		}
		// Cull the wall if it is completely outside the view
		if ((x0 < left0 && x1 < left1) || (x0 > right0 && x1 > right1))
		{
			wall->visible = 0;
			return;
		}

		f32 dx = x1 - x0;
		f32 dz = z1 - z0;
		// Cull the wall if it is back facing.
		// y0*dx - x0*dy
		const f32 side = (z0 * dx) - (x0 * dz);
		if (side < 0)
		{
			wall->visible = 0;
			return;
		}

		f32 curU = 0;
		s32 clipX0_Near = 0;
		s32 clipX1_Near = 0;

		f32 texelLen = wall->texelLength.f32;
		f32 texelLenRem = texelLen;

		//////////////////////////////////////////////
		// Clip the Wall Segment by the left and right
		// frustum lines.
		//////////////////////////////////////////////
		if (widescreen)
		{
			if (!wall_clipToFrustum_Widescreen(x0, z0, x1, z1, dx, dz, curU, texelLen, texelLenRem, clipX0_Near, clipX1_Near, left0, right0, left1, right1, nearPlaneHalfLen))
			{
				wall->visible = 0;
				return;
			}
		}
		else if (!wall_clipToFrustum(x0, z0, x1, z1, dx, dz, curU, texelLen, texelLenRem, clipX0_Near, clipX1_Near, left0, right0, left1, right1))
		{
			wall->visible = 0;
			return;
		}
		
		//////////////////////////////////////////////////
		// Project.
		//////////////////////////////////////////////////
		s32 x0pixel, x1pixel;
		f32 x0proj = (x0 * s_focalLength)/z0 + s_halfWidth;
		f32 x1proj = (x1 * s_focalLength)/z1 + s_halfWidth;
		x0pixel = roundFloat(x0proj);
		x1pixel = roundFloat(x1proj) - 1;
		
		// Handle near plane clipping by adjusting the walls to avoid holes.
		if (clipX0_Near != 0 && x0pixel > s_minScreenX)
		{
			x0 = -nearPlaneHalfLen;
			dx = x1 + nearPlaneHalfLen;
			x0pixel = s_minScreenX;
		}
		if (clipX1_Near != 0 && x1pixel < s_maxScreenX)
		{
			dx = nearPlaneHalfLen - x0;
			x1pixel = s_maxScreenX;
		}

		// The wall is backfacing if x0 > x1
		if (x0pixel > x1pixel)
		{
			wall->visible = 0;
			return;
		}
		// The wall is completely outside of the screen.
		if (x0pixel > s_maxScreenX || x1pixel < s_minScreenX)
		{
			wall->visible = 0;
			return;
		}
		if (s_nextWall == MAX_SEG)
		{
			TFE_System::logWrite(LOG_ERROR, "ClassicRenderer", "Wall_Process : Maximum processed walls exceeded!");
			wall->visible = 0;
			return;
		}
	
		RWallSegment* wallSeg = &s_wallSegListSrc[s_nextWall];
		s_nextWall++;

		if (x0pixel < s_minScreenX)
		{
			x0pixel = s_minScreenX;
		}
		if (x1pixel > s_maxScreenX)
		{
			x1pixel = s_maxScreenX;
		}

		wallSeg->srcWall = wall;
		wallSeg->wallX0_raw = x0pixel;
		wallSeg->wallX1_raw = x1pixel;
		wallSeg->z0.f32 = z0;
		wallSeg->z1.f32 = z1;
		wallSeg->uCoord0.f32 = curU;
		wallSeg->wallX0 = x0pixel;
		wallSeg->wallX1 = x1pixel;
		wallSeg->x0View.f32 = x0;

		f32 slope, den;
		s32 orient;
		if (abs(dx) > abs(dz))
		{
			slope = dz / dx;
			den = dx;
			orient = WORIENT_DZ_DX;
		}
		else
		{
			slope = dx / dz;
			den = dz;
			orient = WORIENT_DX_DZ;
		}

		wallSeg->slope.f32 = slope;
		wallSeg->uScale.f32 = texelLenRem / den;
		wallSeg->orient = orient;
		wall->visible = 1;
	}

	s32 wall_mergeSort(RWallSegment* segOutList, s32 availSpace, s32 start, s32 count)
	{
		TFE_ZONE("Wall Merge/Sort");

		count = min(count, s_maxWallCount);
		if (!count) { return 0; }

		s32 outIndex = 0;
		s32 srcIndex = 0;
		s32 splitWallCount = 0;
		s32 splitWallIndex = -count;

		RWallSegment* srcSeg = &s_wallSegListSrc[start];
		RWallSegment* curSegOut = segOutList;

		RWallSegment  tempSeg;
		RWallSegment* newSeg = &tempSeg;
		RWallSegment  splitWalls[MAX_SPLIT_WALLS];
				
		while (1)
		{
			RWall* srcWall = srcSeg->srcWall;
			// TODO : fix this 
			bool processed = (s_drawFrame == srcWall->drawFrame);
			//bool processed = false;
			bool insideWindow = ((srcSeg->z0.f32 >= s_windowMinZ || srcSeg->z1.f32 >= s_windowMinZ) && srcSeg->wallX0 <= s_windowMaxX && srcSeg->wallX1 >= s_windowMinX);
			if (!processed && insideWindow)
			{
				// Copy the source segment into "newSeg" so it can be modified.
				*newSeg = *srcSeg;

				// Clip the segment 'newSeg' to the current window.
				if (newSeg->wallX0 < s_windowMinX) { newSeg->wallX0 = s_windowMinX; }
				if (newSeg->wallX1 > s_windowMaxX) { newSeg->wallX1 = s_windowMaxX; }

				// Check 'newSeg' versus all of the segments already added for this sector.
				RWallSegment* sortedSeg = segOutList;
				s32 segHidden = 0;
				for (s32 n = 0; n < outIndex && segHidden == 0; n++, sortedSeg++)
				{
					// Trivially skip segments that do not overlap in screenspace.
					bool segOverlap = newSeg->wallX0 <= sortedSeg->wallX1 && sortedSeg->wallX0 <= newSeg->wallX1;
					if (!segOverlap) { continue; }

					RWall* outSrcWall = sortedSeg->srcWall;
					RWall* newSrcWall = newSeg->srcWall;

					vec2* outV0 = outSrcWall->v0;
					vec2* outV1 = outSrcWall->v1;
					vec2* newV0 = newSrcWall->v0;
					vec2* newV1 = newSrcWall->v1;

					f32 newMinZ = min(newV0->z.f32, newV1->z.f32);
					f32 newMaxZ = max(newV0->z.f32, newV1->z.f32);
					f32 outMinZ = min(outV0->z.f32, outV1->z.f32);
					f32 outMaxZ = max(outV0->z.f32, outV1->z.f32);
					s32 side;

					if (newSeg->wallX0 <= sortedSeg->wallX0 && newSeg->wallX1 >= sortedSeg->wallX1)
					{
						if (newMaxZ < outMinZ || newMinZ > outMaxZ)
						{
							// This is a clear case, the 'newSeg' is clearly in front of or behind the current 'sortedSeg'
							side = (newV0->z.f32 < outV0->z.f32) ? FRONT : BACK;
						}
						else if (newV0->z.f32 < outV0->z.f32)
						{
							side = FRONT;
							if ((segmentCrossesLine(outV0->x.f32, outV0->z.f32, 0.0f, 0.0f, newV0->x.f32, newV0->z.f32, newV1->x.f32, newV1->z.f32) != 0 ||		// (outV0, 0) does NOT cross (newV0, newV1)
								 segmentCrossesLine(outV1->x.f32, outV1->z.f32, 0.0f, 0.0f, newV0->x.f32, newV0->z.f32, newV1->x.f32, newV1->z.f32) != 0) &&	// (outV1, 0) does NOT cross (newV0, newV1)
								(segmentCrossesLine(newV0->x.f32, newV0->z.f32, 0.0f, 0.0f, outV0->x.f32, outV0->z.f32, outV1->x.f32, outV1->z.f32) == 0 ||		// (newV0, 0) crosses (outV0, outV1)
								 segmentCrossesLine(newV1->x.f32, newV1->z.f32, 0.0f, 0.0f, outV0->x.f32, outV0->z.f32, outV1->x.f32, outV1->z.f32) == 0))		// (newV1, 0) crosses (outV0, outV1)
							{
								side = BACK;
							}
						}
						else  // newV0->z >= outV0->z
						{
							side = BACK;
							if ((segmentCrossesLine(newV0->x.f32, newV0->z.f32, 0.0f, 0.0f, outV0->x.f32, outV0->z.f32, outV1->x.f32, outV1->z.f32) != 0 ||		// (newV0, 0) does NOT cross (outV0, outV1)
								 segmentCrossesLine(newV1->x.f32, newV1->z.f32, 0.0f, 0.0f, outV0->x.f32, outV0->z.f32, outV1->x.f32, outV1->z.f32) != 0) &&	// (newV1, 0) does NOT cross (outV0, outV1)
								(segmentCrossesLine(outV0->x.f32, outV0->z.f32, 0.0f, 0.0f, newV0->x.f32, newV0->z.f32, newV1->x.f32, newV1->z.f32) == 0 ||		// (outV0, 0) crosses (newV0, newV1)
								 segmentCrossesLine(outV1->x.f32, outV1->z.f32, 0.0f, 0.0f, newV0->x.f32, newV0->z.f32, newV1->x.f32, newV1->z.f32) == 0))		// (outV1, 0) crosses (newV0, newV1)
							{
								side = FRONT;
							}
						}

						// 'newSeg' is in front of 'sortedSeg' and it completely hides it.
						if (side == FRONT)
						{
							s32 copyCount = outIndex - 1 - n;
							RWallSegment* outCur = sortedSeg;
							RWallSegment* outNext = sortedSeg + 1;

							// We are deleting outCur since it is completely hidden by moving all segments from outNext onwards to outCur.
							memmove(outCur, outNext, copyCount * sizeof(RWallSegment));

							// We are deleting outCur since it is completely hidden by 'newSeg'
							// Back up 1 step in the loop, so that outNext is processed (it will hold the same location as outCur before deletion).
							curSegOut = curSegOut - 1;
							sortedSeg = sortedSeg - 1;
							outIndex--;
							n--;
						}
						// 'newSeg' is behind 'sortedSeg' and they overlap.
						else    // (side == BACK)
						{
							// If 'newSeg' is behind 'sortedSeg' but it sticks out on both sides, then
							// 'newSeg' must be split.
							// |NNN|OOOOOOO|SSS|  -> N = newSeg, O = sortedSeg, S = splitSeg from newSeg.
							if (sortedSeg->wallX0 > newSeg->wallX0 && sortedSeg->wallX1 < newSeg->wallX1)
							{
								if (splitWallCount == MAX_SPLIT_WALLS)
								{
									TFE_System::logWrite(LOG_ERROR, "RendererClassic", "Wall_MergeSort : Maximum split walls exceeded!");
									segHidden = 0xffff;
									newSeg->wallX1 = sortedSeg->wallX0 - 1;
									break;
								}
								else
								{
									RWallSegment* splitWall = &splitWalls[splitWallCount];
									*splitWall = *newSeg;
									splitWall->wallX0 = sortedSeg->wallX1 + 1;
									splitWallCount++;

									newSeg->wallX1 = sortedSeg->wallX0 - 1;
								}
							}
							// if 'newSeg' is to the left, adjust it to end where 'sortedSeg' starts.
							// |NNN|OOOOOO|  -> N = newSeg, O = sortedSeg
							else if (sortedSeg->wallX0 > newSeg->wallX0)
							{
								newSeg->wallX1 = sortedSeg->wallX0 - 1;
							}
							// if 'newSeg' is to the right, adjust it to start where 'sortedSeg' ends.
							// |OOOOO|NNN|  -> N = newSeg, O = sortedSeg
							else
							{
								newSeg->wallX0 = sortedSeg->wallX1 + 1;
							}
						}
					}
					else  // (newSeg->wallX0 > sortedSeg->wallX0 || newSeg->wallX1 < sortedSeg->wallX1)
					{
						if (newSeg->wallX0 >= sortedSeg->wallX0 && newSeg->wallX1 <= sortedSeg->wallX1)
						{
							if (newMaxZ < outMinZ || newMinZ > outMaxZ)
							{
								side = (newV0->z.f32 < outV0->z.f32) ? FRONT : BACK;
							}
							else if (newV0->z.f32 < outV0->z.f32)
							{
								side = FRONT;
								if ((segmentCrossesLine(outV0->x.f32, outV0->z.f32, 0.0f, 0.0f, newV0->x.f32, newV0->z.f32, newV1->x.f32, newV1->z.f32) != 0 ||
									 segmentCrossesLine(outV1->x.f32, outV1->z.f32, 0.0f, 0.0f, newV0->x.f32, newV0->z.f32, newV1->x.f32, newV1->z.f32) != 0) &&
									(segmentCrossesLine(newV0->x.f32, newV0->z.f32, 0.0f, 0.0f, outV0->x.f32, outV0->z.f32, outV1->x.f32, outV1->z.f32) == 0 ||
									 segmentCrossesLine(newV1->x.f32, newV1->z.f32, 0.0f, 0.0f, outV0->x.f32, outV0->z.f32, outV1->x.f32, outV1->z.f32) == 0))
								{
									side = BACK;
								}
							}
							else  // (newV0->z >= outV0->z)
							{
								side = BACK;
								if ((segmentCrossesLine(newV0->x.f32, newV0->z.f32, 0.0f, 0.0f, outV0->x.f32, outV0->z.f32, outV1->x.f32, outV1->z.f32) != 0 ||
									 segmentCrossesLine(newV1->x.f32, newV1->z.f32, 0.0f, 0.0f, outV0->x.f32, outV0->z.f32, outV1->x.f32, outV1->z.f32) != 0) &&
									(segmentCrossesLine(outV0->x.f32, outV0->z.f32, 0.0f, 0.0f, newV0->x.f32, newV0->z.f32, newV1->x.f32, newV1->z.f32) == 0 ||
									 segmentCrossesLine(outV1->x.f32, outV1->z.f32, 0.0f, 0.0f, newV0->x.f32, newV0->z.f32, newV1->x.f32, newV1->z.f32) == 0))
								{
									side = FRONT;
								}
							}

							if (side == BACK)
							{
								// The new segment is hidden behind the 'sortedSeg' and can be discarded.
								segHidden = 0xffff;
								break;
							}
							// side == FRONT
							else if (newSeg->wallX0 > sortedSeg->wallX0 && newSeg->wallX1 <= sortedSeg->wallX1)
							{
								if (splitWallCount == MAX_SPLIT_WALLS)
								{
									TFE_System::logWrite(LOG_ERROR, "RendererClassic", "Wall_MergeSort : Maximum split walls exceeded!");
									segHidden = 0xffff;
									break;
								}
								else
								{
									// Split sortedSeg into 2 and insert newSeg in between.
									// { sortedSeg | newSeg | splitWall (from sortedSeg) }
									RWallSegment* splitWall = &splitWalls[splitWallCount];
									splitWallCount++;

									*splitWall = *sortedSeg;
									splitWall->wallX0 = newSeg->wallX1 + 1;
									sortedSeg->wallX1 = newSeg->wallX0 - 1;
								}
							}
							else if (newSeg->wallX0 > sortedSeg->wallX0)
							{
								sortedSeg->wallX1 = newSeg->wallX0 - 1;
							}
							else  // (newSeg->wallX0 <= sortedSeg->wallX0)
							{
								sortedSeg->wallX0 = newSeg->wallX1 + 1;
							}
						}
						// (newSeg->wallX0 < sortedSeg->wallX0 || newSeg->wallX1 > sortedSeg->wallX1)
						else if (newSeg->wallX1 >= sortedSeg->wallX0 && newSeg->wallX1 <= sortedSeg->wallX1)
						{
							if (newMinZ > outMaxZ)
							{
								if (newV1->z.f32 >= outV0->z.f32)
								{
									newSeg->wallX1 = sortedSeg->wallX0 - 1;
								}
								else
								{
									sortedSeg->wallX0 = newSeg->wallX1 + 1;
								}
							}
							else if (segmentCrossesLine(newV1->x.f32, newV1->z.f32, 0.0f, 0.0f, outV0->x.f32, outV0->z.f32, outV1->x.f32, outV1->z.f32) == 0 ||
								     segmentCrossesLine(outV0->x.f32, outV0->z.f32, 0.0f, 0.0f, newV0->x.f32, newV0->z.f32, newV1->x.f32, newV1->z.f32) != 0)
							{
								newSeg->wallX1 = sortedSeg->wallX0 - 1;
							}
							// Is this real?
							else
							{
								sortedSeg->wallX0 = newSeg->wallX1 + 1;
							}
						}
						else if (newMaxZ < outMinZ || newMinZ > outMaxZ)
						{
							if (newV0->z.f32 >= outV1->z.f32)
							{
								newSeg->wallX0 = sortedSeg->wallX1 + 1;
							}
							else
							{
								sortedSeg->wallX1 = newSeg->wallX0 - 1;
							}
						}
						else if (segmentCrossesLine(newV0->x.f32, newV0->z.f32, 0.0f, 0.0f, outV0->x.f32, outV0->z.f32, outV1->x.f32, outV1->z.f32) == 0 ||
							     segmentCrossesLine(outV1->x.f32, outV1->z.f32, 0.0f, 0.0f, newV0->x.f32, newV0->z.f32, newV1->x.f32, newV1->z.f32) != 0)
						{
							newSeg->wallX0 = sortedSeg->wallX1 + 1;
						}
						else
						{
							sortedSeg->wallX1 = newSeg->wallX0 - 1;
						}
					}
				} // for (s32 n = 0; n < outIndex && segHidden == 0; n++, sortedSeg++)

				// If the new segment is still visible and not back facing.
				if (segHidden == 0 && newSeg->wallX0 <= newSeg->wallX1)
				{
					if (outIndex == availSpace)
					{
						TFE_System::logWrite(LOG_ERROR, "RendererClassic", "Wall_MergeSort : Maximum merged walls exceeded!");
					}
					else
					{
						// Copy the temporary segment to the current segment.
						*curSegOut = *newSeg;
						curSegOut++;
						outIndex++;
					}
				}
			}  // if (!processed && insideWindow)
			splitWallIndex++;
			srcIndex++;
			if (srcIndex < count)
			{
				srcSeg++;
			}
			else if (splitWallIndex < splitWallCount)
			{
				srcSeg = &splitWalls[splitWallIndex];
			}
			else
			{
				break;
			}
		}  // while (1)

		return outIndex;
	}

	TextureFrame* setupSignTexture(RWall* srcWall, f32* signU0, f32* signU1, ColumnFunction* signFullbright, ColumnFunction* signLit, bool hqMode)
	{
		TextureFrame* signTex = srcWall->signTex;
		*signU0 = 0; *signU1 = 0;
		*signFullbright = nullptr; *signLit = nullptr;
		if (signTex)
		{
			// Compute the U texel range, the overlay is only drawn within this range.
			*signU0 = srcWall->signUOffset.f32;
			*signU1 = *signU0 + f32(signTex->width);

			// Determine the column functions based on texture opacity and flags.
			// In the original DOS code, the sign column functions are different but only because they do not apply the texture height mask
			// per pixel. I decided to keep it simple, removing the extra binary AND per pixel is not worth adding 4 extra functions that are
			// mostly redundant.
			if (signTex->opacity == OPACITY_TRANS)
			{
				*signFullbright = s_columnFunc[COLFUNC_FULLBRIGHT_TRANS];
				*signLit = s_columnFunc[(srcWall->flags1 & WF1_ILLUM_SIGN) ? COLFUNC_FULLBRIGHT_TRANS : COLFUNC_LIT_TRANS];
			}
			else
			{
				*signFullbright = s_columnFunc[COLFUNC_FULLBRIGHT];
				*signLit = s_columnFunc[(srcWall->flags1 & WF1_ILLUM_SIGN) ? COLFUNC_FULLBRIGHT : COLFUNC_LIT];
			}
		}
		return signTex;
	}

	void wall_drawSolid(RWallSegment* wallSegment)
	{
		RWall* srcWall = wallSegment->srcWall;
		RSector* sector = srcWall->sector;
		TextureFrame* texture = srcWall->midTex;
		if (!texture) { return; }

		f32 ceilingHeight = sector->ceilingHeight.f32;
		f32 floorHeight = sector->floorHeight.f32;

		f32 ceilEyeRel  = ceilingHeight - s_eyeHeight;
		f32 floorEyeRel = floorHeight   - s_eyeHeight;

		f32 z0 = wallSegment->z0.f32;
		f32 z1 = wallSegment->z1.f32;

		f32 y0C, y0F, y1C, y1F;
		y0C = ((ceilEyeRel * s_focalLenAspect)/z0) + s_halfHeight;
		y1C = ((ceilEyeRel * s_focalLenAspect)/z1) + s_halfHeight;
		y0F = ((floorEyeRel* s_focalLenAspect)/z0) + s_halfHeight;
		y1F = ((floorEyeRel* s_focalLenAspect)/z1) + s_halfHeight;

		s32 y0C_pixel = roundFloat(y0C);
		s32 y1C_pixel = roundFloat(y1C);
		s32 y0F_pixel = roundFloat(y0F);
		s32 y1F_pixel = roundFloat(y1F);

		s32 x = wallSegment->wallX0;
		s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;
		f32 numerator = solveForZ_Numerator(wallSegment);

		// For some reason we only early-out if the ceiling is below the view.
		if (y0C_pixel > s_windowMaxY && y1C_pixel > s_windowMaxY)
		{
			f32 yMax = f32(s_windowMaxY + 1);
			flat_addEdges(length, x, 0, yMax, 0, yMax);

			for (s32 i = 0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, numerator);
				s_columnTop[x] = s_windowMaxY;
			}

			srcWall->visible = 0;
			return;
		}

		s_texHeightMask = texture ? texture->height - 1 : 0;

		f32 signU0 = 0, signU1 = 0;
		ColumnFunction signFullbright = nullptr, signLit = nullptr;
		TextureFrame* signTex = setupSignTexture(srcWall, &signU0, &signU1, &signFullbright, &signLit, false);

		f32 wallDeltaX = f32(wallSegment->wallX1_raw - wallSegment->wallX0_raw);
		f32 dYdXtop = 0, dYdXbot = 0;
		if (wallDeltaX != 0)
		{
			dYdXtop = (y1C - y0C) / wallDeltaX;
			dYdXbot = (y1F - y0F) / wallDeltaX;
		}

		f32 clippedXDelta = f32(wallSegment->wallX0 - wallSegment->wallX0_raw);
		if (clippedXDelta != 0)
		{
			y0C += (dYdXtop * clippedXDelta);
			y0F += (dYdXbot * clippedXDelta);
		}
		flat_addEdges(length, wallSegment->wallX0, dYdXbot, y0F, dYdXtop, y0C);

		const s32 texWidth = texture ? texture->width : 0;
		const bool flipHorz = (srcWall->flags1 & WF1_FLIP_HORIZ) != 0;
				
		for (s32 i = 0; i < length; i++, x++)
		{
			s32 top = roundFloat(y0C);
			s32 bot = roundFloat(y0F);
			s_columnBot[x] = bot + 1;
			s_columnTop[x] = top - 1;

			top = max(top, s_windowTop[x]);
			bot = min(bot, s_windowBot[x]);
			s_yPixelCount = bot - top + 1;

			f32 dxView = 0;
			f32 z = solveForZ(wallSegment, x, numerator, &dxView);
			s_depth1d[x] = z;

			f32 uScale = wallSegment->uScale.f32;
			f32 uCoord0 = wallSegment->uCoord0.f32 + srcWall->midUOffset.f32;
			f32 uCoord = uCoord0 + ((wallSegment->orient == WORIENT_DZ_DX) ? dxView*uScale : (z - z0)*uScale);

			if (s_yPixelCount > 0)
			{
				// texture wrapping, assumes texWidth is a power of 2.
				s32 texelU = floorFloat(uCoord) & (texWidth - 1);
				// flip texture coordinate if flag set.
				if (flipHorz) { texelU = texWidth - texelU - 1; }

				// Calculate the vertical texture coordinate start and increment.
				f32 wallHeightPixels = y0F - y0C + 1.0f;
				f32 wallHeightTexels = srcWall->midTexelHeight.f32;

				// s_vCoordStep = tex coord "v" step per y pixel step -> dVdY;
				f32 vCoordStep = wallHeightTexels / wallHeightPixels;
				s_vCoordStep = floatToFixed20(vCoordStep);

				// texel offset from the actual fixed point y position and the truncated y position.
				f32 vPixelOffset = y0F - f32(bot) + 0.5f;

				// scale the texel offset based on the v coord step.
				// the result is the sub-texel offset
				f32 v0 = vCoordStep * vPixelOffset;
				s_vCoordFixed = floatToFixed20(v0 + srcWall->midVOffset.f32);

				// Texture image data = imageStart + u * texHeight
				s_texImage = texture->image + (texelU << texture->logSizeY);
				s_columnLight = computeLighting(z, srcWall->wallLight);
				// column write output.
				s_columnOut = &s_display[top * s_width + x];

				// draw the column
				if (s_columnLight)
				{
					drawColumn_Lit();
				}
				else
				{
					drawColumn_Fullbright();
				}

				// Handle the "sign texture" - a wall overlay.
				if (signTex && uCoord >= signU0 && uCoord <= signU1)
				{
					f32 signYBase = y0F + (srcWall->signVOffset.f32 / vCoordStep);
					s32 y0 = max(floorFloat(signYBase - (f32(signTex->height) / vCoordStep) + 1.5f), top);
					s32 y1 = min(floorFloat(signYBase + 0.5f), bot);
					s_yPixelCount = y1 - y0 + 1;

					if (s_yPixelCount > 0)
					{
						s_vCoordFixed = floatToFixed20((signYBase - f32(y1) + 0.5f) * vCoordStep);
						s_columnOut = &s_display[y0*s_width + x];
						texelU = floorFloat(uCoord - signU0);
						s_texImage = &signTex->image[texelU << signTex->logSizeY];
						if (s_columnLight)
						{
							signLit();
						}
						else
						{
							signFullbright();
						}
					}
				}
			}

			y0C += dYdXtop;
			y0F += dYdXbot;
		}
	}

	void wall_drawTransparent(RWallSegment* wallSegment, EdgePair* edge)
	{
		RWall* srcWall = wallSegment->srcWall;
		RSector* sector = srcWall->sector;
		TextureFrame* texture = srcWall->midTex;

		f32 z0 = wallSegment->z0.f32;
		f32 yC0 = edge->yCeil0.f32;
		f32 yF0 = edge->yFloor0.f32;
		f32 uScale = wallSegment->uScale.f32;
		f32 uCoord0 = wallSegment->uCoord0.f32 + srcWall->midUOffset.f32;
		s32 lengthInPixels = edge->lengthInPixels;

		s_texHeightMask = texture->height - 1;
		s32 flipHorz = (srcWall->flags1 & WF1_FLIP_HORIZ) ? -1 : 0;

		f32 ceil_dYdX = edge->dyCeil_dx.f32;
		f32 floor_dYdX = edge->dyFloor_dx.f32;
		f32 num = solveForZ_Numerator(wallSegment);

		for (s32 i = 0, x = edge->x0; i < lengthInPixels; i++, x++)
		{
			s32 top = s_windowTop[x];
			s32 bot = s_windowBot[x];

			s32 yC_pixel = max(roundFloat(yC0), top);
			s32 yF_pixel = min(roundFloat(yF0), bot);

			s_yPixelCount = yF_pixel - yC_pixel + 1;
			if (s_yPixelCount > 0)
			{
				f32 dxView;
				f32 z = solveForZ(wallSegment, x, num, &dxView);
				f32 uCoord = uCoord0 + ((wallSegment->orient == WORIENT_DZ_DX) ? dxView*uScale : (z - z0)*uScale);

				s32 widthMask = texture->width - 1;
				s32 texelU = floorFloat(uCoord) & widthMask;
				if (flipHorz)
				{
					texelU = widthMask - texelU;
				}

				s_texImage = &texture->image[texelU << texture->logSizeY];
				f32 vCoordStep = srcWall->midTexelHeight.f32 / (yF0 - yC0 + 1.0f);
				s_vCoordStep   = floatToFixed20(vCoordStep);
				s_vCoordFixed  = floatToFixed20((yF0 - f32(yF_pixel) + 0.5f)*vCoordStep + srcWall->midVOffset.f32);

				s_columnOut = &s_display[yC_pixel*s_width + x];
				s_depth1d[x] = z;
				s_columnLight = computeLighting(z, srcWall->wallLight);

				if (s_columnLight)
				{
					drawColumn_Lit_Trans();
				}
				else
				{
					drawColumn_Fullbright_Trans();
				}
			}

			yC0 += ceil_dYdX;
			yF0 += floor_dYdX;
		}
	}

	void wall_drawMask(RWallSegment* wallSegment)
	{
		RWall* srcWall = wallSegment->srcWall;
		RSector* sector = srcWall->sector;
		RSector* nextSector = srcWall->nextSector;

		f32 z0 = wallSegment->z0.f32;
		f32 z1 = wallSegment->z1.f32;
		u32 flags1 = sector->flags1;
		u32 nextFlags1 = nextSector->flags1;

		f32 cProj0, cProj1;
		if ((flags1 & SEC_FLAGS1_EXTERIOR) && (nextFlags1 & SEC_FLAGS1_EXT_ADJ))  // ceiling
		{
			cProj0 = cProj1 = f32(s_windowMinY);
		}
		else
		{
			f32 ceilRel = sector->ceilingHeight.f32 - s_eyeHeight;
			cProj0 = ((ceilRel * s_focalLenAspect)/z0) + s_halfHeight;
			cProj1 = ((ceilRel * s_focalLenAspect)/z1) + s_halfHeight;
		}

		s32 c0pixel = roundFloat(cProj0);
		s32 c1pixel = roundFloat(cProj1);
		if (c0pixel > s_windowMaxY && c1pixel > s_windowMaxY)
		{
			s32 x = wallSegment->wallX0;
			s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;
			flat_addEdges(length, x, 0, f32(s_windowMaxY + 1), 0, f32(s_windowMaxY + 1));
			const f32 numerator = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, numerator);
			}

			srcWall->visible = 0;
			return;
		}

		f32 fProj0, fProj1;
		if ((sector->flags1 & SEC_FLAGS1_PIT) && (nextFlags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))	// floor
		{
			fProj0 = fProj1 = f32(s_windowMaxY);
		}
		else
		{
			f32 floorRel = sector->floorHeight.f32 - s_eyeHeight;
			fProj0 = ((floorRel * s_focalLenAspect)/z0) + s_halfHeight;
			fProj1 = ((floorRel * s_focalLenAspect)/z1) + s_halfHeight;
		}

		s32 f0pixel = roundFloat(fProj0);
		s32 f1pixel = roundFloat(fProj1);
		if (f0pixel < s_windowMinY && f1pixel < s_windowMinY)
		{
			s32 x = wallSegment->wallX0;
			s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;
			flat_addEdges(length, x, 0, f32(s_windowMinY - 1), 0, f32(s_windowMinY - 1));

			const f32 numerator = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, numerator);
				s_columnBot[x] = s_windowMinY;
			}
			srcWall->visible = 0;
			return;
		}

		f32 xStartOffset = f32(wallSegment->wallX0 - wallSegment->wallX0_raw);
		s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;

		f32 numerator = solveForZ_Numerator(wallSegment);
		f32 lengthRaw = f32(wallSegment->wallX1_raw - wallSegment->wallX0_raw);
		f32 dydxCeil = 0;
		f32 dydxFloor = 0;
		if (lengthRaw != 0)
		{
			dydxCeil  = (cProj1 - cProj0) / lengthRaw;
			dydxFloor = (fProj1 - fProj0) / lengthRaw;
		}
		f32 y0 = cProj0;
		f32 y1 = fProj0;
		s32 x = wallSegment->wallX0;
		if (xStartOffset != 0)
		{
			y0 = dydxCeil*xStartOffset  + cProj0;
			y1 = dydxFloor*xStartOffset + fProj0;
		}

		flat_addEdges(length, x, dydxFloor, y1, dydxCeil, y0);
		f32 nextFloor = nextSector->floorHeight.f32;
		f32 nextCeil  = nextSector->ceilingHeight.f32;
		// There is an opening in this wall to the next sector.
		if (nextFloor > nextCeil)
		{
			wall_addAdjoinSegment(length, x, dydxFloor, y1, dydxCeil, y0, wallSegment);
		}
		if (length != 0)
		{
			for (s32 i = 0; i < length; i++, x++)
			{
				s32 y0_pixel = roundFloat(y0);
				s32 y1_pixel = roundFloat(y1);
				s_columnTop[x] = y0_pixel - 1;
				s_columnBot[x] = y1_pixel + 1;

				s_depth1d[x] = solveForZ(wallSegment, x, numerator);
				y0 += dydxCeil;
				y1 += dydxFloor;
			}
		}
	}

	void wall_drawBottom(RWallSegment* wallSegment)
	{
		RWall* wall = wallSegment->srcWall;
		RSector* sector = wall->sector;
		RSector* nextSector = wall->nextSector;
		TextureFrame* tex = wall->botTex;

		f32 z0 = wallSegment->z0.f32;
		f32 z1 = wallSegment->z1.f32;

		f32 cProj0, cProj1;
		if ((sector->flags1 & SEC_FLAGS1_EXTERIOR) && (nextSector->flags1 & SEC_FLAGS1_EXT_ADJ))
		{
			cProj1 = f32(s_windowMinY);
			cProj0 = cProj1;
		}
		else
		{
			f32 ceilRel = sector->ceilingHeight.f32 - s_eyeHeight;
			cProj0 = ((ceilRel * s_focalLenAspect)/z0) + s_halfHeight;
			cProj1 = ((ceilRel * s_focalLenAspect)/z1) + s_halfHeight;
		}

		s32 cy0 = roundFloat(cProj0);
		s32 cy1 = roundFloat(cProj1);
		if (cy0 > s_windowMaxY && cy1 >= s_windowMaxY)
		{
			wall->visible = 0;
			s32 x = wallSegment->wallX0;
			s32 length = wallSegment->wallX1 - x + 1;

			flat_addEdges(length, x, 0.0f, f32(s_windowMaxY + 1), 0.0f, f32(s_windowMaxY + 1));

			f32 num = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, num);
				s_columnTop[x] = s_windowMaxY;
			}
			return;
		}

		f32 floorRel = sector->floorHeight.f32 - s_eyeHeight;
		f32 fProj0, fProj1;
		fProj0 = ((floorRel * s_focalLenAspect)/z0) + s_halfHeight;
		fProj1 = ((floorRel * s_focalLenAspect)/z1) + s_halfHeight;

		s32 fy0 = roundFloat(fProj0);
		s32 fy1 = roundFloat(fProj1);
		if (fy0 < s_windowMinY && fy1 < s_windowMinY)
		{
			// Wall is above the top of the screen.
			wall->visible = 0;
			s32 x = wallSegment->wallX0;
			s32 length = wallSegment->wallX1 - x + 1;

			flat_addEdges(length, x, 0, f32(s_windowMinY - 1), 0, f32(s_windowMinY - 1));

			f32 num = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, num);
				s_columnBot[x] = s_windowMinY;
			}
			return;
		}

		f32 floorRelNext = nextSector->floorHeight.f32 - s_eyeHeight;
		f32 fNextProj0, fNextProj1;
		fNextProj0 = ((floorRelNext * s_focalLenAspect)/z0) + s_halfHeight;
		fNextProj1 = ((floorRelNext * s_focalLenAspect)/z1) + s_halfHeight;

		s32 xOffset = wallSegment->wallX0 - wallSegment->wallX0_raw;
		s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;
		s32 lengthRaw = wallSegment->wallX1_raw - wallSegment->wallX0_raw;
		f32 lengthRawFixed = f32(lengthRaw);
		f32 xOffsetFixed = f32(xOffset);

		f32 floorNext_dYdX = 0;
		f32 floor_dYdX = 0;
		f32 ceil_dYdX = 0;
		if (lengthRawFixed != 0)
		{
			floorNext_dYdX = (fNextProj1 - fNextProj0) / lengthRawFixed;
			floor_dYdX = (fProj1 - fProj0) / lengthRawFixed;
			ceil_dYdX = (cProj1 - cProj0)  / lengthRawFixed;
		}
		if (xOffsetFixed)
		{
			fNextProj0 += (floorNext_dYdX * xOffsetFixed);
			fProj0 += (floor_dYdX * xOffsetFixed);
			cProj0 += (ceil_dYdX  * xOffsetFixed);
		}
		
		f32 yTop = fNextProj0;
		f32 yC = cProj0;
		f32 yBot = fProj0;
		s32 x = wallSegment->wallX0;
		flat_addEdges(length, wallSegment->wallX0, floor_dYdX, fProj0, ceil_dYdX, cProj0);

		s32 yTop0 = roundFloat(fNextProj0);
		s32 yTop1 = roundFloat(fNextProj1);
		if ((yTop0 > s_windowMinY || yTop1 > s_windowMinY) && sector->ceilingHeight.f32 < nextSector->floorHeight.f32)
		{
			wall_addAdjoinSegment(length, wallSegment->wallX0, floorNext_dYdX, fNextProj0, ceil_dYdX, cProj0, wallSegment);
		}

		if (yTop0 > s_windowMaxY && yTop1 > s_windowMaxY)
		{
			s32 bot = s_windowMaxY + 1;
			f32 num = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++, yC += ceil_dYdX)
			{
				s32 yC_pixel = min(roundFloat(yC), s_windowBot[x]);
				s_columnTop[x] = yC_pixel - 1;
				s_columnBot[x] = bot;
				s_depth1d[x] = solveForZ(wallSegment, x, num);
			}
			return;
		}

		f32 u0 = wallSegment->uCoord0.f32;
		f32 num = solveForZ_Numerator(wallSegment);
		s_texHeightMask = tex->height - 1;
		s32 flipHorz  = (wall->flags1 & WF1_FLIP_HORIZ) ? -1 : 0;
		s32 illumSign = (wall->flags1 & WF1_ILLUM_SIGN) ? -1 : 0;

		f32 signU0 = 0, signU1 = 0;
		ColumnFunction signFullbright = nullptr, signLit = nullptr;
		TextureFrame* signTex = setupSignTexture(wall, &signU0, &signU1, &signFullbright, &signLit, false);

		if (length > 0)
		{
			for (s32 i = 0; i < length; i++, x++)
			{
				s32 yTop_pixel = roundFloat(yTop);
				s32 yC_pixel   = roundFloat(yC);
				s32 yBot_pixel = roundFloat(yBot);

				s_columnTop[x] = yC_pixel - 1;
				s_columnBot[x] = yBot_pixel + 1;

				s32 bot = s_windowBot[x];
				s32 top = s_windowTop[x];
				if (yTop_pixel < s_windowTop[x])
				{
					yTop_pixel = s_windowTop[x];
				}
				if (yBot_pixel > s_windowBot[x])
				{
					yBot_pixel = s_windowBot[x];
				}
				s_yPixelCount = yBot_pixel - yTop_pixel + 1;

				// Calculate perspective correct Z and U (texture coordinate).
				f32 dxView;
				f32 z = solveForZ(wallSegment, x, num, &dxView);
				f32 uCoord;
				if (wallSegment->orient == WORIENT_DZ_DX)
				{
					uCoord = u0 + (dxView * wallSegment->uScale.f32) + wall->botUOffset.f32;
				}
				else
				{
					f32 dz = z - z0;
					uCoord = u0 + (dz * wallSegment->uScale.f32) + wall->botUOffset.f32;
				}
				s_depth1d[x] = z;
				if (s_yPixelCount > 0)
				{
					s32 widthMask = tex->width - 1;
					s32 texelU = floorFloat(uCoord) & widthMask;
					if (flipHorz != 0)
					{
						texelU = widthMask - texelU;
					}

					f32 vCoordStep = wall->botTexelHeight.f32 / (yBot - yTop + 1.0f);
					f32 v0 = (yBot - f32(yBot_pixel) + 0.5f) * vCoordStep;
					s_vCoordFixed = floatToFixed20(v0 + wall->botVOffset.f32);
					s_vCoordStep  = floatToFixed20(vCoordStep);

					s_texImage = &tex->image[texelU << tex->logSizeY];
					s_columnOut = &s_display[yTop_pixel * s_width + x];
					s_columnLight = computeLighting(z, wall->wallLight);
					if (s_columnLight)
					{
						drawColumn_Lit();
					}
					else
					{
						drawColumn_Fullbright();
					}

					// Handle the "sign texture" - a wall overlay.
					if (signTex && uCoord >= signU0 && uCoord <= signU1)
					{
						f32 signYBase = yBot + (wall->signVOffset.f32/vCoordStep);
						s32 y0 = max(floorFloat(signYBase - (f32(signTex->height)/vCoordStep) + 1.5f), yTop_pixel);
						s32 y1 = min(floorFloat(signYBase + 0.5f), yBot_pixel);
						s_yPixelCount = y1 - y0 + 1;

						if (s_yPixelCount > 0)
						{
							s_vCoordFixed = floatToFixed20((signYBase - f32(y1) + 0.5f)*vCoordStep);
							s_columnOut = &s_display[y0*s_width + x];
							texelU = floorFloat(uCoord - signU0);
							s_texImage = &signTex->image[texelU << signTex->logSizeY];
							if (s_columnLight)
							{
								signLit();
							}
							else
							{
								signFullbright();
							}
						}
					}
				}
				yTop += floorNext_dYdX;
				yBot += floor_dYdX;
				yC += ceil_dYdX;
			}
		}
	}

	void wall_drawTop(RWallSegment* wallSegment)
	{
		RWall* srcWall = wallSegment->srcWall;
		RSector* sector = srcWall->sector;
		RSector* next = srcWall->nextSector;
		TextureFrame* texture = srcWall->topTex;
		f32 z0 = wallSegment->z0.f32;
		f32 z1 = wallSegment->z1.f32;
		f32 num = solveForZ_Numerator(wallSegment);
				
		s32 x0 = wallSegment->wallX0;
		s32 lengthInPixels = wallSegment->wallX1 - wallSegment->wallX0 + 1;

		f32 ceilRel = sector->ceilingHeight.f32 - s_eyeHeight;
		f32 yC0, yC1;
		yC0 = ((ceilRel * s_focalLenAspect)/z0) + s_halfHeight;
		yC1 = ((ceilRel * s_focalLenAspect)/z1) + s_halfHeight;

		s32 yC0_pixel = roundFloat(yC0);
		s32 yC1_pixel = roundFloat(yC1);

		s_texHeightMask = texture->height - 1;

		if (yC0_pixel > s_windowMaxY && yC1_pixel > s_windowMaxY)
		{
			srcWall->visible = 0;
			for (s32 i = 0; i < lengthInPixels; i++) { s_columnTop[x0 + i] = s_windowMaxY; }
			flat_addEdges(lengthInPixels, x0, 0, f32(s_windowMaxY + 1), 0, f32(s_windowMaxY + 1));
			for (s32 i = 0, x = x0; i < lengthInPixels; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, num);
			}
			return;
		}

		f32 yF0, yF1;
		if ((sector->flags1 & SEC_FLAGS1_PIT) && (next->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
		{
			yF0 = yF1 = f32(s_windowMaxY);
		}
		else
		{
			f32 floorRel = sector->floorHeight.f32 - s_eyeHeight;
			yF0 = ((floorRel * s_focalLenAspect)/z0) + s_halfHeight;
			yF1 = ((floorRel * s_focalLenAspect)/z1) + s_halfHeight;
		}
		s32 yF0_pixel = roundFloat(yF0);
		s32 yF1_pixel = roundFloat(yF1);
		if (yF0_pixel < s_windowMinY && yF1_pixel < s_windowMinY)
		{
			srcWall->visible = 0;
			for (s32 i = 0; i < lengthInPixels; i++) { s_columnBot[x0 + i] = s_windowMinY; }
			flat_addEdges(lengthInPixels, x0, 0, f32(s_windowMinY - 1), 0, f32(s_windowMinY - 1));
			for (s32 i = 0, x = x0; i < lengthInPixels; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, num);
			}
			return;
		}

		f32 next_ceilRel = next->ceilingHeight.f32 - s_eyeHeight;
		f32 next_yC0, next_yC1;
		next_yC0 = ((next_ceilRel * s_focalLenAspect)/z0) + s_halfHeight;
		next_yC1 = ((next_ceilRel * s_focalLenAspect)/z1) + s_halfHeight;

		f32 xOffset = f32(wallSegment->wallX0 - wallSegment->wallX0_raw);
		f32 length = f32(wallSegment->wallX1_raw - wallSegment->wallX0_raw);
		f32 ceil_dYdX = 0;
		f32 next_ceil_dYdX = 0;
		f32 floor_dYdX = 0;
		if (length)
		{
			ceil_dYdX = (yC1 - yC0) / length;
			next_ceil_dYdX = (next_yC1 - next_yC0) / length;
			floor_dYdX = (yF1 - yF0) / length;
		}
		if (xOffset)
		{
			yC0 += (ceil_dYdX * xOffset);
			next_yC0 += (next_ceil_dYdX * xOffset);
			yF0 += (floor_dYdX * xOffset);
		}

		flat_addEdges(lengthInPixels, x0, floor_dYdX, yF0, ceil_dYdX, yC0);
		s32 next_yC0_pixel = roundFloat(next_yC0);
		s32 next_yC1_pixel = roundFloat(next_yC1);
		if ((next_yC0_pixel < s_windowMaxY || next_yC1_pixel < s_windowMaxY) && (sector->floorHeight.f32 > next->ceilingHeight.f32))
		{
			wall_addAdjoinSegment(lengthInPixels, x0, floor_dYdX, yF0, next_ceil_dYdX, next_yC0, wallSegment);
		}

		if (next_yC0_pixel < s_windowMinY && next_yC1_pixel < s_windowMinY)
		{
			for (s32 i = 0; i < lengthInPixels; i++) { s_columnTop[x0 + i] = s_windowMinY - 1; }
			for (s32 i = 0, x = x0; i < lengthInPixels; i++, x++)
			{
				yF0_pixel = roundFloat(yF0);
				if (yF0_pixel > s_windowBot[x])
				{
					yF0_pixel = s_windowBot[x];
				}

				s_columnBot[x] = yF0_pixel + 1;
				s_depth1d[x] = solveForZ(wallSegment, x, num);
				yF0 += floor_dYdX;
			}
			return;
		}

		f32 uCoord0 = wallSegment->uCoord0.f32;
		s32 flipHorz = (srcWall->flags1 & WF1_FLIP_HORIZ) ? -1 : 0;
		s32 illumSign = (srcWall->flags1 & WF1_ILLUM_SIGN) ? -1 : 0;

		f32 signU0 = 0, signU1 = 0;
		ColumnFunction signFullbright = nullptr, signLit = nullptr;
		TextureFrame* signTex = setupSignTexture(srcWall, &signU0, &signU1, &signFullbright, &signLit, false);

		for (s32 i = 0, x = x0; i < lengthInPixels; i++, x++)
		{
			yC0_pixel = roundFloat(yC0);
			next_yC0_pixel = roundFloat(next_yC0);
			yF0_pixel = roundFloat(yF0);

			s_columnTop[x] = yC0_pixel - 1;
			s_columnBot[x] = yF0_pixel + 1;
			s32 top = s_windowTop[x];
			if (yC0_pixel < top)
			{
				yC0_pixel = top;
			}
			s32 bot = s_windowBot[x];
			if (next_yC0_pixel > bot)
			{
				next_yC0_pixel = bot;
			}

			s_yPixelCount = next_yC0_pixel - yC0_pixel + 1;

			f32 dxView;
			f32 z = solveForZ(wallSegment, x, num, &dxView);

			f32 uScale = wallSegment->uScale.f32;
			f32 uCoord0 = wallSegment->uCoord0.f32 + srcWall->topUOffset.f32;
			f32 uCoord = uCoord0 + ((wallSegment->orient == WORIENT_DZ_DX) ? dxView*uScale : (z - z0)*uScale);

			s_depth1d[x] = z;
			if (s_yPixelCount > 0)
			{
				s32 widthMask = texture->width - 1;
				s32 texelU = floorFloat(uCoord) & widthMask;
				if (flipHorz)
				{
					texelU = widthMask - texelU;
				}
				assert(texelU >= 0 && texelU <= widthMask);

				f32 vCoordStep = (srcWall->topTexelHeight.f32) / (next_yC0 - yC0 + 1.0f);
				s_vCoordFixed = floatToFixed20(srcWall->topVOffset.f32 + (next_yC0 - f32(next_yC0_pixel) + 0.5f)*vCoordStep);
				s_vCoordStep = floatToFixed20(vCoordStep);
				s_texImage = &texture->image[texelU << texture->logSizeY];

				s_columnOut = &s_display[yC0_pixel * s_width + x];
				s_columnLight = computeLighting(z, srcWall->wallLight);
				if (s_columnLight)
				{
					drawColumn_Lit();
				}
				else
				{
					drawColumn_Fullbright();
				}

				// Handle the "sign texture" - a wall overlay.
				if (signTex && uCoord >= signU0 && uCoord <= signU1)
				{
					f32 signYBase = next_yC0 + (srcWall->signVOffset.f32/vCoordStep);
					s32 y0 = max(floorFloat(signYBase - (f32(signTex->height)/vCoordStep) + 1.5f), yC0_pixel);
					s32 y1 = min(floorFloat(signYBase + 0.5f), next_yC0_pixel);
					s_yPixelCount = y1 - y0 + 1;

					if (s_yPixelCount > 0)
					{
						s_vCoordFixed = floatToFixed20((signYBase - f32(y1) + 0.5f) * vCoordStep);
						s_columnOut = &s_display[y0*s_width + x];
						texelU = floorFloat(uCoord - signU0);
						s_texImage = &signTex->image[texelU << signTex->logSizeY];
						if (s_columnLight)
						{
							signLit();
						}
						else
						{
							signFullbright();
						}
					}
				}
			}

			yC0 += ceil_dYdX;
			next_yC0 += next_ceil_dYdX;
			yF0 += floor_dYdX;
		}
	}

	void wall_drawTopAndBottom(RWallSegment* wallSegment)
	{
		RWall* srcWall = wallSegment->srcWall;
		RSector* sector = srcWall->sector;
		TextureFrame* topTex = srcWall->topTex;
		f32 z0 = wallSegment->z0.f32;
		f32 z1 = wallSegment->z1.f32;
		s32 x0 = wallSegment->wallX0;
		f32 xOffset = f32(wallSegment->wallX0 - wallSegment->wallX0_raw);
		s32 length  = wallSegment->wallX1 - wallSegment->wallX0 + 1;
		f32 lengthRaw = f32(wallSegment->wallX1_raw - wallSegment->wallX0_raw);

		f32 ceilRel = sector->ceilingHeight.f32 - s_eyeHeight;
		f32 cProj0, cProj1;
		cProj0 = ((ceilRel * s_focalLenAspect)/z0) + s_halfHeight;
		cProj1 = ((ceilRel * s_focalLenAspect)/z1) + s_halfHeight;

		s32 c0_pixel = roundFloat(cProj0);
		s32 c1_pixel = roundFloat(cProj1);
				
		if (c0_pixel > s_windowMaxY && c1_pixel > s_windowMaxY)
		{
			srcWall->visible = 0;
			for (s32 i = 0; i < length; i++) { s_columnTop[x0 + i] = s_windowMaxY; }

			flat_addEdges(length, x0, 0, f32(s_windowMaxY + 1), 0, f32(s_windowMaxY + 1));
			f32 num = solveForZ_Numerator(wallSegment);
			for (s32 i = 0, x = x0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, num);
			}
			return;
		}

		f32 floorRel = sector->floorHeight.f32 - s_eyeHeight;
		f32 fProj0, fProj1;
		fProj0 = ((floorRel * s_focalLenAspect)/z0) + s_halfHeight;
		fProj1 = ((floorRel * s_focalLenAspect)/z1) + s_halfHeight;

		s32 f0_pixel = roundFloat(fProj0);
		s32 f1_pixel = roundFloat(fProj1);
		if (f0_pixel < s_windowMinY && f1_pixel < s_windowMinY)
		{
			srcWall->visible = 0;
			for (s32 i = 0; i < length; i++) { s_columnBot[x0 + i] = s_windowMinY; }

			flat_addEdges(length, x0, 0, f32(s_windowMinY - 1), 0, f32(s_windowMinY - 1));
			f32 num = solveForZ_Numerator(wallSegment);

			for (s32 i = 0, x = x0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, num);
			}
			return;
		}

		RSector* nextSector = srcWall->nextSector;
		f32 next_ceilRel = nextSector->ceilingHeight.f32 - s_eyeHeight;
		f32 next_cProj0, next_cProj1;
		next_cProj0 = ((next_ceilRel * s_focalLenAspect)/z0) + s_halfHeight;
		next_cProj1 = ((next_ceilRel * s_focalLenAspect)/z1) + s_halfHeight;

		f32 ceil_dYdX = 0;
		f32 next_ceil_dYdX = 0;
		if (lengthRaw != 0)
		{
			ceil_dYdX = (cProj1 - cProj0) / lengthRaw;
			next_ceil_dYdX = (next_cProj1 - next_cProj0) / lengthRaw;
		}
		if (xOffset)
		{
			cProj0 += (ceil_dYdX * xOffset);
			next_cProj0 += (next_ceil_dYdX * xOffset);
		}

		f32 yC0 = cProj0;
		f32 yC1 = next_cProj0;
		
		s32 cn0_pixel = roundFloat(next_cProj0);
		s32 cn1_pixel = roundFloat(next_cProj1);
		if (cn0_pixel >= s_windowMinY || cn1_pixel >= s_windowMinY)
		{
			f32 u0 = wallSegment->uCoord0.f32;
			f32 num = solveForZ_Numerator(wallSegment);
			s_texHeightMask = topTex->height - 1;
			s32 flipHorz = (srcWall->flags1 & WF1_FLIP_HORIZ) ? -1 : 0;

			for (s32 i = 0, x = x0; i < length; i++, x++)
			{
				s32 yC1_pixel = roundFloat(yC1);
				s32 yC0_pixel = roundFloat(yC0);
				s_columnTop[x] = yC0_pixel - 1;
				s32 top = s_windowTop[x];
				if (yC0_pixel < top)
				{
					yC0_pixel = top;
				}
				s32 bot = s_windowBot[x];
				if (yC1_pixel > bot)
				{
					yC1_pixel = bot;
				}
				s_yPixelCount = yC1_pixel - yC0_pixel + 1;

				// Calculate perspective correct Z and U (texture coordinate).
				f32 dxView;
				f32 z = solveForZ(wallSegment, x, num, &dxView);
				f32 u;
				if (wallSegment->orient == WORIENT_DZ_DX)
				{
					u = u0 + (dxView*wallSegment->uScale.f32) + srcWall->topUOffset.f32;
				}
				else
				{
					f32 dz = z - z0;
					u = u0 + (dz*wallSegment->uScale.f32) + srcWall->topUOffset.f32;
				}
				s_depth1d[x] = z;
				if (s_yPixelCount > 0)
				{
					s32 widthMask = topTex->width - 1;
					s32 texelU = floorFloat(u) & widthMask;
					if (flipHorz)
					{
						texelU = widthMask - texelU;
					}
					f32 vCoordStep = (srcWall->topTexelHeight.f32) / (yC1 - yC0 + 1.0f);
					f32 yOffset = yC1 - f32(yC1_pixel) + 0.5f;
					s_vCoordFixed = floatToFixed20((yOffset * vCoordStep) + srcWall->topVOffset.f32);
					s_vCoordStep = floatToFixed20(vCoordStep);
					s_texImage = &topTex->image[texelU << topTex->logSizeY];
					s_columnOut = &s_display[yC0_pixel * s_width + x];
					s_columnLight = computeLighting(z, srcWall->wallLight);

					if (s_columnLight)
					{
						drawColumn_Lit();
					}
					else
					{
						drawColumn_Fullbright();
					}
				}
				yC0 += ceil_dYdX;
				yC1 += next_ceil_dYdX;
			}
		}
		else
		{
			for (s32 i = 0; i < length; i++) { s_columnTop[x0 + i] = s_windowMinY - 1; }
		}

		f32 next_floorRel = nextSector->floorHeight.f32 - s_eyeHeight;
		f32 next_fProj0, next_fProj1;
		next_fProj0 = ((next_floorRel * s_focalLenAspect)/z0) + s_halfHeight;
		next_fProj1 = ((next_floorRel * s_focalLenAspect)/z1) + s_halfHeight;

		f32 next_floor_dYdX = 0;
		f32 floor_dYdX = 0;
		if (lengthRaw > 0)
		{
			next_floor_dYdX = (next_fProj1 - next_fProj0) / lengthRaw;
			floor_dYdX = (fProj1 - fProj0) / lengthRaw;
		}
		if (xOffset)
		{
			next_fProj0 += (next_floor_dYdX * xOffset);
			fProj0 += (floor_dYdX * xOffset);
		}

		f32 yF0 = next_fProj0;
		f32 yF1 = fProj0;
		TextureFrame* botTex = srcWall->botTex;
		f0_pixel = roundFloat(next_fProj0);
		f1_pixel = roundFloat(next_fProj1);

		if (f0_pixel <= s_windowMaxY || f1_pixel <= s_windowMaxY)
		{
			f32 u0 = wallSegment->uCoord0.f32;
			f32 num = solveForZ_Numerator(wallSegment);

			s_texHeightMask = botTex->height - 1;
			s32 flipHorz = (srcWall->flags1 & WF1_FLIP_HORIZ) ? -1 : 0;
			s32 illumSign = (srcWall->flags1 & WF1_ILLUM_SIGN) ? -1 : 0;

			f32 signU0 = 0, signU1 = 0;
			ColumnFunction signFullbright = nullptr, signLit = nullptr;
			TextureFrame* signTex = setupSignTexture(srcWall, &signU0, &signU1, &signFullbright, &signLit, false);

			if (length > 0)
			{
				for (s32 i = 0, x = x0; i < length; i++, x++)
				{
					s32 yF0_pixel = roundFloat(yF0);
					s32 yF1_pixel = roundFloat(yF1);
					s32 top = s_windowTop[x];
					s_columnBot[x] = yF1_pixel + 1;
					if (yF0_pixel < top)
					{
						yF0_pixel = top;
					}
					s32 bot = s_windowBot[x];
					if (yF1_pixel > bot)
					{
						yF1_pixel = bot;
					}
					s_yPixelCount = yF1_pixel - yF0_pixel + 1;

					// Calculate perspective correct Z and U (texture coordinate).
					f32 dxView;
					f32 z = solveForZ(wallSegment, x, num, &dxView);
					f32 uCoord;
					if (wallSegment->orient == WORIENT_DZ_DX)
					{
						uCoord = u0 + (dxView*wallSegment->uScale.f32) + srcWall->botUOffset.f32;
					}
					else
					{
						f32 dz = z - z0;
						uCoord = u0 + (dz*wallSegment->uScale.f32) + srcWall->botUOffset.f32;
					}
					s_depth1d[x] = z;
					if (s_yPixelCount > 0)
					{
						s32 widthMask = botTex->width - 1;
						s32 texelU = floorFloat(uCoord) & widthMask;
						if (flipHorz)
						{
							texelU = widthMask - texelU;
						}
						f32 vCoordStep = (srcWall->botTexelHeight.f32) / (yF1 - yF0 + 1.0f);
						s_vCoordFixed = floatToFixed20(srcWall->botVOffset.f32 + (yF1 - f32(yF1_pixel) + 0.5f)*vCoordStep);
						s_vCoordStep = floatToFixed20(vCoordStep);
						s_texImage = &botTex->image[texelU << botTex->logSizeY];
						s_columnOut = &s_display[yF0_pixel * s_width + x];
						s_columnLight = computeLighting(z, srcWall->wallLight);

						if (s_columnLight)
						{
							drawColumn_Lit();
						}
						else
						{
							drawColumn_Fullbright();
						}

						// Handle the "sign texture" - a wall overlay.
						if (signTex && uCoord >= signU0 && uCoord <= signU1)
						{
							f32 signYBase = yF1 + (srcWall->signVOffset.f32/vCoordStep);
							s32 y0 = max(floorFloat(signYBase - (f32(signTex->height)/vCoordStep) + 1.5f), yF0_pixel);
							s32 y1 = min(floorFloat(signYBase + 0.5f), yF1_pixel);
							s_yPixelCount = y1 - y0 + 1;

							if (s_yPixelCount > 0)
							{
								s_vCoordFixed = floatToFixed20((signYBase - f32(y1) + 0.5f)*vCoordStep);
								s_columnOut = &s_display[y0*s_width + x];
								texelU = floorFloat(uCoord - signU0);
								s_texImage = &signTex->image[texelU << signTex->logSizeY];
								if (s_columnLight)
								{
									signLit();
								}
								else
								{
									signFullbright();
								}
							}
						}
					}
					yF1 += floor_dYdX;
					yF0 += next_floor_dYdX;
				}
			}
		}
		else
		{
			for (s32 i = 0; i < length; i++) { s_columnBot[x0 + i] = s_windowMaxY + 1; }
		}
		flat_addEdges(length, x0, floor_dYdX, fProj0, ceil_dYdX, cProj0);

		s32 next_f0_pixel = roundFloat(next_fProj0);
		s32 next_f1_pixel = roundFloat(next_fProj1);
		s32 next_c0_pixel = roundFloat(next_cProj0);
		s32 next_c1_pixel = roundFloat(next_cProj1);
		if ((next_f0_pixel <= s_windowMinY && next_f1_pixel <= s_windowMinY) || (next_c0_pixel >= s_windowMaxY && next_c1_pixel >= s_windowMaxY) || (nextSector->floorHeight.f32 <= nextSector->ceilingHeight.f32))
		{
			return;
		}

		wall_addAdjoinSegment(length, x0, next_floor_dYdX, next_fProj0 - 1.0f, next_ceil_dYdX, next_cProj0 + 1.0f, wallSegment);
	}

	// Parts of the code inside 's_height == SKY_BASE_HEIGHT' are based on the original DOS exe.
	// Other parts of those same conditionals are modified to handle higher resolutions.
	void wall_drawSkyTop(RSector* sector)
	{
		if (s_wallMaxCeilY < s_windowMinY) { return; }
		TFE_ZONE("Draw Sky");

		TextureFrame* texture = sector->ceilTex;
		f32 heightScale;
		// In the original code (at the original 200p resolution) the sky is setup to step exactly one texel per vertical pixel
		// However with higher resolutions this must be scaled to look the same.
		f32 vCoordStep;
		if (s_height == SKY_BASE_HEIGHT)
		{
			vCoordStep  = 1.0f;
			heightScale = 1.0f;
		}
		else
		{
			vCoordStep = f32(SKY_BASE_HEIGHT) / f32(s_height);
			heightScale = vCoordStep;
		}
		s_vCoordStep = floatToFixed20(vCoordStep);

		s_texHeightMask = texture->height - 1;
		const s32 texWidthMask = texture->width - 1;

		for (s32 x = s_windowMinX; x <= s_windowMaxX; x++)
		{
			const s32 y0 = s_windowTop[x];
			const s32 y1 = min(s_columnTop[x], s_windowBot[x]);

			s_yPixelCount = y1 - y0 + 1;
			if (s_yPixelCount > 0)
			{
				if (s_height == SKY_BASE_HEIGHT)
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask - y1) - s_skyPitchOffset - sector->ceilOffsetZ.f32);
				}
				else
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask) - (f32(y1) * heightScale) - s_skyPitchOffset - sector->ceilOffsetZ.f32);
				}

				s32 texelU = ( floorFloat(sector->ceilOffsetX.f32 - s_skyYawOffset + s_skyTable[x]) ) & texWidthMask;
				s_texImage = &texture->image[texelU << texture->logSizeY];
				s_columnOut = &s_display[y0*s_width + x];
				drawColumn_Fullbright();
			}
		}
	}

	void wall_drawSkyTopNoWall(RSector* sector)
	{
		TFE_ZONE("Draw Sky");
		const TextureFrame* texture = sector->ceilTex;

		f32 heightScale;
		f32 vCoordStep;
		// In the original code (at the original 200p resolution) the sky is setup to step exactly one texel per vertical pixel
		// However with higher resolutions this must be scaled to look the same.
		if (s_height == SKY_BASE_HEIGHT)
		{
			vCoordStep = 1.0f;
			heightScale = 1.0f;
		}
		else
		{
			vCoordStep = f32(SKY_BASE_HEIGHT) / f32(s_height);
			heightScale = vCoordStep;
		}
		s_vCoordStep = floatToFixed20(vCoordStep);

		s_texHeightMask = texture->height - 1;
		for (s32 x = s_windowMinX; x <= s_windowMaxX; x++)
		{
			const s32 y0 = s_windowTop[x];
			const s32 y1 = min(s_heightInPixels - 1, s_windowBot[x]);

			s_yPixelCount = y1 - y0 + 1;
			if (s_yPixelCount > 0)
			{
				if (s_height == SKY_BASE_HEIGHT)
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask - y1) - s_skyPitchOffset - sector->ceilOffsetZ.f32);
				}
				else
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask) - (f32(y1) * heightScale) - s_skyPitchOffset - sector->ceilOffsetZ.f32);
				}

				s32 widthMask = texture->width - 1;
				s32 texelU = floorFloat(sector->ceilOffsetX.f32 - s_skyYawOffset + s_skyTable[x]) & widthMask;
				s_texImage = &texture->image[texelU << texture->logSizeY];
				s_columnOut = &s_display[y0*s_width + x];

				drawColumn_Fullbright();
			}
		}
	}

	// Parts of the code inside 's_height == SKY_BASE_HEIGHT' are based on the original DOS exe.
	// Other parts of those same conditionals are modified to handle higher resolutions.
	void wall_drawSkyBottom(RSector* sector)
	{
		if (s_wallMinFloorY > s_windowMaxY) { return; }
		TFE_ZONE("Draw Sky");

		TextureFrame* texture = sector->floorTex;
		f32 heightScale;
		f32 vCoordStep;
		// In the original code (at the original 200p resolution) the sky is setup to step exactly one texel per vertical pixel
		// However with higher resolutions this must be scaled to look the same.
		if (s_height == SKY_BASE_HEIGHT)
		{
			vCoordStep = 1.0f;
			heightScale = 1.0f;
		}
		else
		{
			vCoordStep = f32(SKY_BASE_HEIGHT) / f32(s_height);
			heightScale = vCoordStep;
		}
		s_vCoordStep = floatToFixed20(vCoordStep);

		s_texHeightMask = texture->height - 1;
		const s32 texWidthMask = texture->width - 1;

		for (s32 x = s_windowMinX; x <= s_windowMaxX; x++)
		{
			const s32 y0 = max(s_columnBot[x], s_windowTop[x]);
			const s32 y1 = s_windowBot[x];

			s_yPixelCount = y1 - y0 + 1;
			if (s_yPixelCount > 0)
			{
				if (s_height == SKY_BASE_HEIGHT)
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask - y1) - s_skyPitchOffset - sector->floorOffsetZ.f32);
				}
				else
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask) - (f32(y1) * heightScale) - s_skyPitchOffset - sector->floorOffsetZ.f32);
				}

				s32 texelU = floorFloat(sector->floorOffsetX.f32 - s_skyYawOffset + s_skyTable[x]) & texWidthMask;
				s_texImage = &texture->image[texelU << texture->logSizeY];
				s_columnOut = &s_display[y0*s_width + x];
				drawColumn_Fullbright();
			}
		}
	}

	void wall_drawSkyBottomNoWall(RSector* sector)
	{
		TFE_ZONE("Draw Sky");
		const TextureFrame* texture = sector->floorTex;

		f32 heightScale;
		f32 vCoordStep;
		// In the original code (at the original 200p resolution) the sky is setup to step exactly one texel per vertical pixel
		// However with higher resolutions this must be scaled to look the same.
		if (s_height == SKY_BASE_HEIGHT)
		{
			vCoordStep = 1.0f;
			heightScale = 1.0f;
		}
		else
		{
			vCoordStep = f32(SKY_BASE_HEIGHT) / f32(s_height);
			heightScale = vCoordStep;
		}
		s_vCoordStep = floatToFixed20(vCoordStep);

		s_texHeightMask = texture->height - 1;
		for (s32 x = s_windowMinX; x <= s_windowMaxX; x++)
		{
			const s32 y0 = min(s_heightInPixels, s_windowTop[x]);
			const s32 y1 = s_windowBot[x];

			s_yPixelCount = y1 - y0 + 1;
			if (s_yPixelCount > 0)
			{
				if (s_height == SKY_BASE_HEIGHT)
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask - y1) - s_skyPitchOffset - sector->floorOffsetZ.f32);
				}
				else
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask) - (f32(y1) * heightScale) - s_skyPitchOffset - sector->floorOffsetZ.f32);
				}

				s32 widthMask = texture->width - 1;
				s32 texelU = floorFloat(sector->floorOffsetX.f32 - s_skyYawOffset + s_skyTable[x]) & widthMask;
				s_texImage = &texture->image[texelU << texture->logSizeY];
				s_columnOut = &s_display[y0*s_width + x];

				drawColumn_Fullbright();
			}
		}
	}
	
	// Determines if segment A is disjoint from the line formed by B - i.e. they do not intersect.
	// Returns 1 if segment A does NOT cross line B or 0 if it does.
	s32 segmentCrossesLine(f32 ax0, f32 ay0, f32 ax1, f32 ay1, f32 bx0, f32 by0, f32 bx1, f32 by1)
	{
		// [ (a1-b0)x(b1-b0) ].[ (a0-b0)x(b1 - b0) ]
		// In 2D x = "perp product"
		s_segmentCross = ((ax1 - bx0)*(by1 - by0) - (ay1 - by0)*(bx1 - bx0)) *
			             ((ax0 - bx0)*(by1 - by0) - (ay0 - by0)*(bx1 - bx0));

		return s_segmentCross > 0.0f ? 1 : 0;
	}
		
	// When solving for Z, part of the computation can be done once per wall.
	f32 solveForZ_Numerator(RWallSegment* wallSegment)
	{
		const f32 z0 = wallSegment->z0.f32;

		f32 numerator;
		if (wallSegment->orient == WORIENT_DZ_DX)
		{
			numerator = z0 - (wallSegment->slope.f32 * wallSegment->x0View.f32);
		}
		else  // WORIENT_DX_DZ
		{
			numerator = wallSegment->x0View.f32 - (wallSegment->slope.f32 * z0);
		}

		return numerator;
	}
	
	// Solve for perspective correct Z at the current x pixel coordinate.
	f32 solveForZ(RWallSegment* wallSegment, s32 x, f32 numerator, f32* outViewDx/*=nullptr*/)
	{
		f32 z;	// perspective correct z coordinate at the current x pixel coordinate.
		if (wallSegment->orient == WORIENT_DZ_DX)
		{
			// Solve for viewspace X at the current pixel x coordinate in order to get dx in viewspace.
			const f32 fx = f32(x);
			// Scale halfWidthOverX by focal length to account for widescreen.
			// Note in the original code s_focalLength == s_halfWidth, so in that case the code is functionally equivalent.
			const f32 halfWidthOverX = ((fx != s_halfWidth) ? s_focalLength / (fx - s_halfWidth) : s_focalLength);
			f32 den = halfWidthOverX - wallSegment->slope.f32;
			// Avoid divide by zero.
			if (den == 0) { den = 1; }

			f32 xView = numerator/den;
			// Use the saved x0View to get dx in viewspace.
			f32 dxView = xView - wallSegment->x0View.f32;
			// avoid recalculating for u coordinate computation.
			if (outViewDx) { *outViewDx = dxView; }
			// Once we have dx in viewspace, we multiply by the slope (dZ/dX) in order to get dz.
			f32 dz = dxView * wallSegment->slope.f32;
			// Then add z0 to get z(x) that is perspective correct.
			z = wallSegment->z0.f32 + dz;
		}
		else  // WORIENT_DX_DZ
		{
			// Directly solve for Z at the current pixel x coordinate.
			// Scale xOverHalfWidth by focal length to account for widescreen.
			// Note in the original code s_focalLength == s_halfWidth, so in that case the code is functionally equivalent.
			const f32 xOverHalfWidth = (f32(x) - s_halfWidth) / s_focalLength;
			f32 den = xOverHalfWidth - wallSegment->slope.f32;
			// Avoid divide by 0.
			if (den == 0) { den = 1; }

			z = numerator / den;
		}
		return z;
	}
	
	// Column Draw functions all used much higher precision and range fixed point values then the original code.
	// See rwallFixed.cpp for DOS precision functions.
	void drawColumn_Fullbright()
	{
		const s32 end = s_yPixelCount - 1;
		const u8* tex = s_texImage;

		fixed44_20 vCoordFixed = s_vCoordFixed;
		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width, vCoordFixed += s_vCoordStep)
		{
			const s32 v = floor20(vCoordFixed) & s_texHeightMask;
			s_columnOut[offset] = tex[v];
		}
	}

	void drawColumn_Lit()
	{
		const s32 end = s_yPixelCount - 1;
		const u8* tex = s_texImage;

		fixed44_20 vCoordFixed = s_vCoordFixed;
		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width, vCoordFixed += s_vCoordStep)
		{
			const s32 v = floor20(vCoordFixed) & s_texHeightMask;
			s_columnOut[offset] = s_columnLight[tex[v]];
		}
	}

	void drawColumn_Fullbright_Trans()
	{
		const s32 end = s_yPixelCount - 1;
		const u8* tex = s_texImage;

		fixed44_20 vCoordFixed = s_vCoordFixed;
		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width, vCoordFixed += s_vCoordStep)
		{
			const s32 v = floor20(vCoordFixed) & s_texHeightMask;
			const u8 c = tex[v];
			if (c) { s_columnOut[offset] = c; }
		}
	}

	void drawColumn_Lit_Trans()
	{
		const s32 end = s_yPixelCount - 1;
		const u8* tex = s_texImage;

		fixed44_20 vCoordFixed = s_vCoordFixed;
		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width, vCoordFixed += s_vCoordStep)
		{
			const s32 v = floor20(vCoordFixed) & s_texHeightMask;
			const u8 c = tex[v];
			if (c) { s_columnOut[offset] = s_columnLight[c]; }
		}
	}

	void wall_addAdjoinSegment(s32 length, s32 x0, f32 top_dydx, f32 y1, f32 bot_dydx, f32 y0, RWallSegment* wallSegment)
	{
		if (s_adjoinSegCount < MAX_ADJOIN_SEG)
		{
			f32 lengthFixed = f32(length - 1);
			f32 y0End = y0;
			if (bot_dydx != 0)
			{
				y0End += (bot_dydx * lengthFixed);
			}
			f32 y1End = y1;
			if (top_dydx != 0)
			{
				y1End += (top_dydx * lengthFixed);
			}
			edgePair_setup(length, x0, top_dydx, y1End, y1, bot_dydx, y0, y0End, s_adjoinEdge);

			s_adjoinEdge++;
			s_adjoinSegCount++;

			*s_adjoinSegment = wallSegment;
			s_adjoinSegment++;
		}
	}

	void wall_setupAdjoinDrawFlags(RWall* wall)
	{
		if (wall->nextSector)
		{
			RSector* sector = wall->sector;
			RWall* mirror = wall->mirrorWall;
			f32 wFloorHeight = sector->floorHeight.f32;
			f32 wCeilHeight = sector->ceilingHeight.f32;
			RSector* nextSector = mirror->sector;
			f32 mFloorHeight = nextSector->floorHeight.f32;
			f32 mCeilHeight = nextSector->ceilingHeight.f32;
			wall->drawFlags = 0;
			mirror->drawFlags = 0;

			if (wCeilHeight < mCeilHeight)
			{
				wall->drawFlags |= WDF_TOP;
			}
			if (wFloorHeight > mFloorHeight)
			{
				wall->drawFlags |= WDF_BOT;
			}
			if (mCeilHeight < wCeilHeight)
			{
				mirror->drawFlags |= WDF_TOP;
			}
			if (mFloorHeight > wFloorHeight)
			{
				mirror->drawFlags |= WDF_BOT;
			}
		}
	}

	void wall_computeTexelHeights(RWall* wall)
	{
		if (wall->nextSector)
		{
			if (wall->drawFlags & 1)
			{
				RSector* next = wall->nextSector;
				RSector* cur = wall->sector;
				wall->topTexelHeight.f32 = (next->ceilingHeight.f32 - cur->ceilingHeight.f32) * 8.0f;
			}
			if (wall->drawFlags & 2)
			{
				RSector* cur = wall->sector;
				RSector* next = wall->nextSector;
				wall->botTexelHeight.f32 = (cur->floorHeight.f32 - next->floorHeight.f32) * 8.0f;
			}
			if (wall->midTex)
			{
				if (!(wall->drawFlags & 2) && !(wall->drawFlags & 1))
				{
					RSector* midSector = wall->sector;
					f32 midFloorHeight = wall->sector->floorHeight.f32;
					wall->midTexelHeight.f32 = (midFloorHeight - midSector->ceilingHeight.f32) * 8.0f;
				}
				else if (!(wall->drawFlags & 2))
				{
					RSector* midSector = wall->nextSector;
					f32 midFloorHeight = wall->sector->floorHeight.f32;
					wall->midTexelHeight.f32 = (midFloorHeight - midSector->ceilingHeight.f32) * 8.0f;
				}
				else if (!(wall->drawFlags & 1))
				{
					RSector* midSector = wall->sector;
					f32 midFloorHeight = wall->nextSector->floorHeight.f32;
					wall->midTexelHeight.f32 = (midFloorHeight - midSector->ceilingHeight.f32) * 8.0f;
				}
				else
				{
					RSector* midSector = wall->nextSector;
					f32 midFloorHeight = wall->nextSector->floorHeight.f32;
					wall->midTexelHeight.f32 = (midFloorHeight - midSector->ceilingHeight.f32) * 8.0f;
				}
			}
		}
		else
		{
			RSector* midSector = wall->sector;
			f32 midFloorHeight = midSector->floorHeight.f32;
			wall->midTexelHeight.f32 = (midFloorHeight - midSector->ceilingHeight.f32) * 8;
		}
	}

	// This is the same as the fixed point version
	// TODO: Refactor.
	void sprite_decompressColumn(const u8* colData, u8* outBuffer, s32 height)
	{
		for (s32 y = 0, i = 0; y < height; )
		{
			u8 count = *colData;
			colData++;

			if (count & 0x80)
			{
				count &= 0x7f;
				for (s32 r = 0; r < count; r++, y++)
				{
					outBuffer[y] = 0;
				}
			}
			else
			{
				for (s32 r = 0; r < count; r++, y++, colData++)
				{
					outBuffer[y] = *colData;
				}
			}
		}
	}

	// Refactor this into a sprite specific file.
	void sprite_drawFrame(u8* basePtr, WaxFrame* frame, SecObject* obj)
	{
		if (!frame) { return; }

		const WaxCell* cell = WAX_CellPtr(basePtr, frame);
		const f32 z = obj->posVS.z.f32;
		const s32 flip = frame->flip;
		// Make sure the sprite isn't behind the near plane.
		if (z < 1.0f) { return; }

		const f32 widthWS  = fixed16ToFloat(frame->widthWS);
		const f32 heightWS = fixed16ToFloat(frame->heightWS);
		const f32 fOffsetX = fixed16ToFloat(frame->offsetX);
		const f32 fOffsetY = fixed16ToFloat(frame->offsetY);

		const f32 yOffset = heightWS - fOffsetY;
		const f32 x0 = obj->posVS.x.f32 - fOffsetX;
		const f32 y0 = obj->posVS.y.f32 - yOffset;

		const f32 rcpZ = 1.0f / z;
		const f32 projX0 = x0*s_focalLength*rcpZ + s_halfWidth;
		const f32 projY0 = y0*s_focalLenAspect*rcpZ + s_halfHeight;

		s32 x0_pixel = roundFloat(projX0);
		s32 y0_pixel = roundFloat(projY0);
		// Cull the sprite if it is completely outside of the view.
		if (x0_pixel > s_windowMaxX || y0_pixel > s_windowMaxY)
		{
			return;
		}

		const f32 x1 = x0 + widthWS;
		const f32 y1 = y0 + heightWS;
		const f32 projX1 = x1 * s_focalLength*rcpZ + s_halfWidth;
		const f32 projY1 = y1 * s_focalLenAspect*rcpZ + s_halfHeight;

		s32 x1_pixel = roundFloat(projX1);
		s32 y1_pixel = roundFloat(projY1);
		// Cull the sprite if it is completely outside of the view.
		if (x1_pixel < s_windowMinX || y1_pixel < s_windowMinY)
		{
			return;
		}

		const s32 length = x1_pixel - x0_pixel + 1;
		// Cull zero length sprites.
		if (length < 1)
		{
			return;
		}

		// Calculate the screen space size and texture coordinate step values.
		const f32 height = projY1 - projY0 + 1.0f;
		const f32 width  = projX1 - projX0 + 1.0f;
		const f32 uCoordStep = f32(cell->sizeX) / width;
		const f32 vCoordStep = f32(cell->sizeY) / height;
		s_vCoordStep = floatToFixed20(vCoordStep);

		// Clip the x positions to the window and adjust the left texture coordinate.
		f32 uCoord = 0.0f;
		if (x0_pixel < s_windowX0)
		{
			uCoord = uCoordStep * f32(s_windowX0 - x0_pixel);
			x0_pixel = s_windowX0;
		}
		if (x1_pixel > s_windowX1)
		{
			x1_pixel = s_windowX1;
		}

		// Cull if the result is backfacing (which shouldn't happen for sprites, but just in case).
		if (x0_pixel > x1_pixel)
		{
			return;
		}

		// Compute the lighting for the whole sprite.
		s_columnLight = computeLighting(z, 0);

		// Figure out the correct column draw function.
		ColumnFunction spriteColumnFunc;
		if (s_columnLight && !(obj->flags & OBJ_FLAG_FULLBRIGHT))
		{
			spriteColumnFunc = s_columnFunc[COLFUNC_LIT_TRANS];
		}
		else
		{
			spriteColumnFunc = s_columnFunc[COLFUNC_FULLBRIGHT_TRANS];
		}

		// Draw
		const s32 compressed = cell->compressed;
		const u8* imageData = (u8*)cell + sizeof(WaxCell);
		const u8* image = imageData + ((compressed == 1) ? cell->sizeX * sizeof(u32) : 0);

		// This should be set to handle all sizes, repeating is not required.
		s_texHeightMask = 0xffff;

		// Loop through each column, decompress if required and then draw the column using the selected column function.
		s32 lastColumn = INT_MIN;
		const u32* columnOffset = (u32*)(basePtr + cell->columnOffset);
		for (s32 x = x0_pixel; x <= x1_pixel; x++, uCoord += uCoordStep)
		{
			if (z < s_depth1d[x])
			{
				const s32 top = s_objWindowTop[x];
				const s32 bot = s_objWindowBot[x];

				const s32 y0 = y0_pixel < top ? top : y0_pixel;
				const s32 y1 = y1_pixel > bot ? bot : y1_pixel;

				s_yPixelCount = y1 - y0 + 1;
				if (s_yPixelCount > 0)
				{
					const f32 vOffset = f32(y1_pixel - y1);
					s_vCoordFixed = floatToFixed20(vOffset*vCoordStep);

					s32 texelU = floorFloat(uCoord);
					if (flip)
					{
						texelU = cell->sizeX - texelU - 1;
					}

					if (compressed)
					{
						const u8* colPtr = (u8*)cell + columnOffset[texelU];

						// Decompress the column into "work buffer."
						if (lastColumn != texelU)
						{
							// Unlike the original code, columns are only decompressed if they change texels in order to
							// perform better at higher resolutions.
							sprite_decompressColumn(colPtr, s_workBuffer, cell->sizeY);
						}
						s_texImage = (u8*)s_workBuffer;
					}
					else
					{
						s_texImage = (u8*)image + columnOffset[texelU];
					}
					lastColumn = texelU;
					// Output.
					s_columnOut = &s_display[y0 * s_width + x];
					// Draw the column.
					spriteColumnFunc();
				}
			}
		}
	}

}  // RClassic_Float

}  // TFE_JediRenderer