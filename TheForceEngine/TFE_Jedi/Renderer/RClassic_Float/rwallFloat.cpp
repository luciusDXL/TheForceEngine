#include <cstring>

#include <TFE_System/profiler.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/rtexture.h>

#include "fixedPoint20.h"
#include "rwallFloat.h"
#include "rflatFloat.h"
#include "rlightingFloat.h"
#include "rsectorFloat.h"
#include "redgePairFloat.h"
#include "rclassicFloatSharedState.h"
#include "../rcommon.h"
#include "../jediRenderer.h"

namespace TFE_Jedi
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
	f32 solveForZ_Numerator(RWallSegmentFloat* wallSegment);
	f32 solveForZ(RWallSegmentFloat* wallSegment, s32 x, f32 numerator, f32* outViewDx=nullptr);
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
		f32 param = bx * uz - bz * ux;
		f32 den = bx * az - bz * ax;
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
		f32 dyx = dz * s_rcfltState.nearPlaneHalfLen - dx;
		if (dyx != 0.0f)
		{
			xz /= dyx;
		}
		return xz;
	}

	// Returns true if the wall is potentially visible.
	// Original DOS clipping code converted to floating point with small additions to support widescreen.
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
				s = (-xz * s_rcfltState.nearPlaneHalfLen - x0) / dx;
			}

			// Update the x0,y0 coordinate of the segment.
			x0 = -xz * s_rcfltState.nearPlaneHalfLen;
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
				s = (xz*s_rcfltState.nearPlaneHalfLen - x1) / dx;
			}

			// Update the x1,y1 coordinate of the segment.
			x1 = xz * s_rcfltState.nearPlaneHalfLen;
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
		if ((z0 < 0 || z1 < 0) && segmentCrossesLine(0.0f, 0.0f, 0.0f, -s_rcfltState.halfHeight, x0, x0, x1, z1) != 0)
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

	// Process the wall and produce an RWallSegment for rendering if the wall is potentially visible.
	void wall_process(WallCached* wallCached)
	{
		const vec2_float* p0 = wallCached->v0;
		const vec2_float* p1 = wallCached->v1;
		RWall* wall = wallCached->wall;

		// viewspace wall coordinates.
		f32 x0 = p0->x;
		f32 x1 = p1->x;
		f32 z0 = p0->z;
		f32 z1 = p1->z;

		// x values of frustum lines that pass through (x0,z0) and (x1,z1)
		f32 left0 = -z0 * s_rcfltState.nearPlaneHalfLen;
		f32 left1 = -z1 * s_rcfltState.nearPlaneHalfLen;
		f32 right0 = z0 * s_rcfltState.nearPlaneHalfLen;
		f32 right1 = z1 * s_rcfltState.nearPlaneHalfLen;

		// Cull the wall if it is completely beyind the camera.
		if (z0 < 0.0f && z1 < 0.0f)
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
		if (side < 0.0f)
		{
			wall->visible = 0;
			return;
		}

		f32 curU = 0.0f;
		s32 clipLeft = 0;
		s32 clipRight = 0;
		s32 clipX1_Near = 0;
		s32 clipX0_Near = 0;

		f32 texelLen = wallCached->texelLength;
		f32 texelLenRem = texelLen;

		//////////////////////////////////////////////
		// Clip the Wall Segment by the left and right
		// frustum lines.
		//////////////////////////////////////////////
		if (!wall_clipToFrustum(x0, z0, x1, z1, dx, dz, curU, texelLen, texelLenRem, clipX0_Near, clipX1_Near, left0, right0, left1, right1))
		{
			wall->visible = 0;
			return;
		}
		
		//////////////////////////////////////////////////
		// Project.
		//////////////////////////////////////////////////
		f32 x0proj = (x0*s_rcfltState.focalLength)/z0 + s_rcfltState.projOffsetX;
		f32 x1proj = (x1*s_rcfltState.focalLength)/z1 + s_rcfltState.projOffsetX;
		s32 x0pixel = roundFloat(x0proj);
		s32 x1pixel = roundFloat(x1proj) - 1;
		
		// Handle near plane clipping by adjusting the walls to avoid holes.
		if (clipX0_Near != 0 && x0pixel > s_minScreenX_Pixels)
		{
			x0 = -s_rcfltState.nearPlaneHalfLen;
			dx = x1 + s_rcfltState.nearPlaneHalfLen;
			x0pixel = s_minScreenX_Pixels;
		}
		if (clipX1_Near != 0 && x1pixel < s_maxScreenX_Pixels)
		{
			dx = s_rcfltState.nearPlaneHalfLen - x0;
			x1pixel = s_maxScreenX_Pixels;
		}

		// The wall is backfacing if x0 > x1
		if (x0pixel > x1pixel)
		{
			wall->visible = 0;
			return;
		}
		// The wall is completely outside of the screen.
		if (x0pixel > s_maxScreenX_Pixels || x1pixel < s_minScreenX_Pixels)
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
	
		RWallSegmentFloat* wallSeg = &s_rcfltState.wallSegListSrc[s_nextWall];
		s_nextWall++;

		if (x0pixel < s_minScreenX_Pixels)
		{
			x0pixel = s_minScreenX_Pixels;
		}
		if (x1pixel > s_maxScreenX_Pixels)
		{
			x1pixel = s_maxScreenX_Pixels;
		}

		wallSeg->srcWall = wallCached;
		wallSeg->wallX0_raw = x0pixel;
		wallSeg->wallX1_raw = x1pixel;
		wallSeg->z0 = z0;
		wallSeg->z1 = z1;
		wallSeg->uCoord0 = curU;
		wallSeg->wallX0 = x0pixel;
		wallSeg->wallX1 = x1pixel;
		wallSeg->x0View = x0;

		f32 slope, den;
		s32 orient;
		if (TFE_Jedi::abs(dx) > TFE_Jedi::abs(dz))
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

		wallSeg->slope = slope;
		wallSeg->uScale = texelLenRem / den;
		wallSeg->orient = orient;
		wall->visible = 1;
	}

	s32 wall_mergeSort(RWallSegmentFloat* segOutList, s32 availSpace, s32 start, s32 count)
	{
		TFE_ZONE("Wall Merge/Sort");

		count = min(count, s_maxWallCount);
		if (!count) { return 0; }

		s32 outIndex = 0;
		s32 srcIndex = 0;
		s32 splitWallCount = 0;
		s32 splitWallIndex = -count;

		RWallSegmentFloat* srcSeg = &s_rcfltState.wallSegListSrc[start];
		RWallSegmentFloat* curSegOut = segOutList;

		RWallSegmentFloat  tempSeg;
		RWallSegmentFloat* newSeg = &tempSeg;
		RWallSegmentFloat  splitWalls[MAX_SPLIT_WALLS];
				
		while (1)
		{
			WallCached* srcWall = srcSeg->srcWall;
			JBool processed = (s_drawFrame == srcWall->wall->drawFrame) ? JTRUE : JFALSE;
			JBool insideWindow = ((srcSeg->z0 >= s_rcfltState.windowMinZ || srcSeg->z1 >= s_rcfltState.windowMinZ) && srcSeg->wallX0 <= s_windowMaxX_Pixels && srcSeg->wallX1 >= s_windowMinX_Pixels) ? JTRUE : JFALSE;
			if (!processed && insideWindow)
			{
				// Copy the source segment into "newSeg" so it can be modified.
				*newSeg = *srcSeg;

				// Clip the segment 'newSeg' to the current window.
				if (newSeg->wallX0 < s_windowMinX_Pixels) { newSeg->wallX0 = s_windowMinX_Pixels; }
				if (newSeg->wallX1 > s_windowMaxX_Pixels) { newSeg->wallX1 = s_windowMaxX_Pixels; }

				// Check 'newSeg' versus all of the segments already added for this sector.
				RWallSegmentFloat* sortedSeg = segOutList;
				s32 segHidden = 0;
				for (s32 n = 0; n < outIndex && segHidden == 0; n++, sortedSeg++)
				{
					// Trivially skip segments that do not overlap in screenspace.
					JBool segOverlap = (newSeg->wallX0 <= sortedSeg->wallX1 && sortedSeg->wallX0 <= newSeg->wallX1) ? JTRUE : JFALSE;
					if (!segOverlap) { continue; }

					WallCached* outSrcWall = sortedSeg->srcWall;
					WallCached* newSrcWall = newSeg->srcWall;

					vec2_float* outV0 = outSrcWall->v0;
					vec2_float* outV1 = outSrcWall->v1;
					vec2_float* newV0 = newSrcWall->v0;
					vec2_float* newV1 = newSrcWall->v1;

					f32 newMinZ = min(newV0->z, newV1->z);
					f32 newMaxZ = max(newV0->z, newV1->z);
					f32 outMinZ = min(outV0->z, outV1->z);
					f32 outMaxZ = max(outV0->z, outV1->z);
					s32 side;

					if (newSeg->wallX0 <= sortedSeg->wallX0 && newSeg->wallX1 >= sortedSeg->wallX1)
					{
						if (newMaxZ < outMinZ || newMinZ > outMaxZ)
						{
							// This is a clear case, the 'newSeg' is clearly in front of or behind the current 'sortedSeg'
							side = (newV0->z < outV0->z) ? FRONT : BACK;
						}
						else if (newV0->z < outV0->z)
						{
							side = FRONT;
							if ((segmentCrossesLine(outV0->x, outV0->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) != 0 ||		// (outV0, 0) does NOT cross (newV0, newV1)
								 segmentCrossesLine(outV1->x, outV1->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) != 0) &&	// (outV1, 0) does NOT cross (newV0, newV1)
								(segmentCrossesLine(newV0->x, newV0->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) == 0 ||		// (newV0, 0) crosses (outV0, outV1)
								 segmentCrossesLine(newV1->x, newV1->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) == 0))		// (newV1, 0) crosses (outV0, outV1)
							{
								side = BACK;
							}
						}
						else  // newV0->z >= outV0->z
						{
							side = BACK;
							if ((segmentCrossesLine(newV0->x, newV0->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) != 0 ||		// (newV0, 0) does NOT cross (outV0, outV1)
								 segmentCrossesLine(newV1->x, newV1->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) != 0) &&	// (newV1, 0) does NOT cross (outV0, outV1)
								(segmentCrossesLine(outV0->x, outV0->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) == 0 ||		// (outV0, 0) crosses (newV0, newV1)
								 segmentCrossesLine(outV1->x, outV1->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) == 0))		// (outV1, 0) crosses (newV0, newV1)
							{
								side = FRONT;
							}
						}

						// 'newSeg' is in front of 'sortedSeg' and it completely hides it.
						if (side == FRONT)
						{
							s32 copyCount = outIndex - 1 - n;
							RWallSegmentFloat* outCur = sortedSeg;
							RWallSegmentFloat* outNext = sortedSeg + 1;

							// We are deleting outCur since it is completely hidden by moving all segments from outNext onwards to outCur.
							memmove(outCur, outNext, copyCount * sizeof(RWallSegmentFloat));

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
									RWallSegmentFloat* splitWall = &splitWalls[splitWallCount];
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
								side = (newV0->z < outV0->z) ? FRONT : BACK;
							}
							else if (newV0->z < outV0->z)
							{
								side = FRONT;
								if ((segmentCrossesLine(outV0->x, outV0->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) != 0 ||
									 segmentCrossesLine(outV1->x, outV1->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) != 0) &&
									(segmentCrossesLine(newV0->x, newV0->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) == 0 ||
									 segmentCrossesLine(newV1->x, newV1->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) == 0))
								{
									side = BACK;
								}
							}
							else  // (newV0->z >= outV0->z)
							{
								side = BACK;
								if ((segmentCrossesLine(newV0->x, newV0->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) != 0 ||
									 segmentCrossesLine(newV1->x, newV1->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) != 0) &&
									(segmentCrossesLine(outV0->x, outV0->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) == 0 ||
									 segmentCrossesLine(outV1->x, outV1->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) == 0))
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
									RWallSegmentFloat* splitWall = &splitWalls[splitWallCount];
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
								if (newV1->z >= outV0->z)
								{
									newSeg->wallX1 = sortedSeg->wallX0 - 1;
								}
								else
								{
									sortedSeg->wallX0 = newSeg->wallX1 + 1;
								}
							}
							else if (segmentCrossesLine(newV1->x, newV1->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) == 0 ||
								     segmentCrossesLine(outV0->x, outV0->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) != 0)
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
							if (newV0->z >= outV1->z)
							{
								newSeg->wallX0 = sortedSeg->wallX1 + 1;
							}
							else
							{
								sortedSeg->wallX1 = newSeg->wallX0 - 1;
							}
						}
						else if (segmentCrossesLine(newV0->x, newV0->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) == 0 ||
							     segmentCrossesLine(outV1->x, outV1->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) != 0)
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

	TextureData* setupSignTexture(WallCached* srcWall, f32* signU0, f32* signU1, ColumnFunction* signFullbright, ColumnFunction* signLit)
	{
		if (!srcWall->wall->signTex) { return nullptr; }

		TextureData* signTex = *srcWall->wall->signTex;
		*signU0 = 0; *signU1 = 0;
		*signFullbright = nullptr; *signLit = nullptr;
		if (signTex)
		{
			// Compute the U texel range, the overlay is only drawn within this range.
			*signU0 = srcWall->signOffset.x;
			*signU1 = *signU0 + f32(signTex->width);

			// Determine the column functions based on texture opacity and flags.
			// In the original DOS code, the sign column functions are different but only because they do not apply the texture height mask
			// per pixel. I decided to keep it simple, removing the extra binary AND per pixel is not worth adding 4 extra functions that are
			// mostly redundant.
			if (signTex->flags & OPACITY_TRANS)
			{
				*signFullbright = s_columnFunc[COLFUNC_FULLBRIGHT_TRANS];
				*signLit = s_columnFunc[(srcWall->wall->flags1 & WF1_ILLUM_SIGN) ? COLFUNC_FULLBRIGHT_TRANS : COLFUNC_LIT_TRANS];
			}
			else
			{
				*signFullbright = s_columnFunc[COLFUNC_FULLBRIGHT];
				*signLit = s_columnFunc[(srcWall->wall->flags1 & WF1_ILLUM_SIGN) ? COLFUNC_FULLBRIGHT : COLFUNC_LIT];
			}
		}
		return signTex;
	}

	void wall_drawSolid(RWallSegmentFloat* wallSegment)
	{
		WallCached* cachedWall = wallSegment->srcWall;
		SectorCached* cachedSector = cachedWall->sector;

		RWall* srcWall = cachedWall->wall;
		RSector* sector = srcWall->sector;
		TextureData* texture = *srcWall->midTex;
		if (!texture) { return; }

		f32 ceilingHeight = cachedSector->ceilingHeight;
		f32 floorHeight = cachedSector->floorHeight;

		f32 ceilEyeRel  = ceilingHeight - s_rcfltState.eyeHeight;
		f32 floorEyeRel = floorHeight   - s_rcfltState.eyeHeight;

		f32 z0 = wallSegment->z0;
		f32 z1 = wallSegment->z1;

		f32 y0C = (ceilEyeRel  * s_rcfltState.focalLenAspect) / z0 + s_rcfltState.projOffsetY;
		f32 y1C = (ceilEyeRel  * s_rcfltState.focalLenAspect) / z1 + s_rcfltState.projOffsetY;
		f32 y0F = (floorEyeRel * s_rcfltState.focalLenAspect) / z0 + s_rcfltState.projOffsetY;
		f32 y1F = (floorEyeRel * s_rcfltState.focalLenAspect) / z1 + s_rcfltState.projOffsetY;

		s32 y0C_pixel = roundFloat(y0C);
		s32 y1C_pixel = roundFloat(y1C);
		s32 y0F_pixel = roundFloat(y0F);
		s32 y1F_pixel = roundFloat(y1F);

		s32 x = wallSegment->wallX0;
		s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;
		f32 numerator = solveForZ_Numerator(wallSegment);

		// For some reason we only early-out if the ceiling is below the view.
		if (y0C_pixel > s_windowMaxY_Pixels && y1C_pixel > s_windowMaxY_Pixels)
		{
			f32 yMax = f32(s_windowMaxY_Pixels + 1);
			flat_addEdges(length, x, 0, yMax, 0, yMax);

			for (s32 i = 0; i < length; i++, x++)
			{
				s_rcfltState.depth1d[x] = solveForZ(wallSegment, x, numerator);
				s_columnTop[x] = s_windowMaxY_Pixels;
			}

			srcWall->visible = 0;
			return;
		}

		s_texHeightMask = texture ? texture->height - 1 : 0;

		f32 signU0 = 0, signU1 = 0;
		ColumnFunction signFullbright = nullptr, signLit = nullptr;
		TextureData* signTex = setupSignTexture(cachedWall, &signU0, &signU1, &signFullbright, &signLit);

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
		const JBool flipHorz = ((srcWall->flags1 & WF1_FLIP_HORIZ)!=0) ? JTRUE : JFALSE;
				
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
			s_rcfltState.depth1d[x] = z;

			f32 uScale  = wallSegment->uScale;
			f32 uCoord0 = wallSegment->uCoord0 + cachedWall->midOffset.x;
			f32 uCoord = uCoord0 + ((wallSegment->orient == WORIENT_DZ_DX) ? dxView*uScale : (z - z0)*uScale);

			if (s_yPixelCount > 0)
			{
				// texture wrapping, assumes texWidth is a power of 2.
				s32 texelU = floorFloat(uCoord) & (texWidth - 1);
				// flip texture coordinate if flag set.
				if (flipHorz) { texelU = texWidth - texelU - 1; }

				// Calculate the vertical texture coordinate start and increment.
				f32 wallHeightPixels = y0F - y0C + 1.0f;
				f32 wallHeightTexels = cachedWall->midTexelHeight;

				// s_vCoordStep = tex coord "v" step per y pixel step -> dVdY;
				f32 vCoordStep = wallHeightTexels / wallHeightPixels;
				s_vCoordStep = floatToFixed20(vCoordStep);

				// texel offset from the actual fixed point y position and the truncated y position.
				f32 vPixelOffset = y0F - f32(bot) + 0.5f;

				// scale the texel offset based on the v coord step.
				// the result is the sub-texel offset
				f32 v0 = vCoordStep * vPixelOffset;
				s_vCoordFixed = floatToFixed20(v0 + cachedWall->midOffset.z);

				// Texture image data = imageStart + u * texHeight
				s_texImage = texture->image + (texelU << texture->logSizeY);
				s_columnLight = computeLighting(z, floor16(srcWall->wallLight));
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
					f32 signYBase = y0F + (cachedWall->signOffset.z /  vCoordStep);
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

		srcWall->seen = JTRUE;
	}

	void wall_drawTransparent(RWallSegmentFloat* wallSegment, EdgePairFloat* edge)
	{
		WallCached* cachedWall = wallSegment->srcWall;
		SectorCached* cachedSector = cachedWall->sector;

		RWall* srcWall = cachedWall->wall;
		RSector* sector = srcWall->sector;
		TextureData* texture = *srcWall->midTex;

		f32 z0 = wallSegment->z0;
		f32 yC0 = edge->yCeil0;
		f32 yF0 = edge->yFloor0;
		f32 uScale = wallSegment->uScale;
		f32 uCoord0 = wallSegment->uCoord0 + cachedWall->midOffset.x;
		s32 lengthInPixels = edge->lengthInPixels;

		s_texHeightMask = texture->height - 1;
		JBool flipHorz = ((srcWall->flags1 & WF1_FLIP_HORIZ)!=0) ? JTRUE : JFALSE;

		f32 ceil_dYdX  = edge->dyCeil_dx;
		f32 floor_dYdX = edge->dyFloor_dx;
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
				f32 vCoordStep = cachedWall->midTexelHeight / (yF0 - yC0 + 1.0f);
				s_vCoordStep  = floatToFixed20(vCoordStep);
				s_vCoordFixed = floatToFixed20((yF0 - f32(yF_pixel) + 0.5f)*vCoordStep + cachedWall->midOffset.z);

				s_columnOut = &s_display[yC_pixel*s_width + x];
				s_rcfltState.depth1d[x] = z;
				s_columnLight = computeLighting(z, floor16(srcWall->wallLight));

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

	void wall_drawMask(RWallSegmentFloat* wallSegment)
	{
		WallCached* cachedWall = wallSegment->srcWall;
		SectorCached* cachedSector = cachedWall->sector;

		RWall* srcWall = cachedWall->wall;
		RSector* sector = srcWall->sector;
		RSector* nextSector = srcWall->nextSector;

		f32 z0 = wallSegment->z0;
		f32 z1 = wallSegment->z1;
		u32 flags1 = sector->flags1;
		u32 nextFlags1 = nextSector->flags1;

		f32 cProj0, cProj1;
		if ((flags1 & SEC_FLAGS1_EXTERIOR) && (nextFlags1 & SEC_FLAGS1_EXT_ADJ))  // ceiling
		{
			cProj0 = cProj1 = s_rcfltState.windowMinY;
		}
		else
		{
			f32 ceilRel = cachedSector->ceilingHeight - s_rcfltState.eyeHeight;
			cProj0 = ((ceilRel*s_rcfltState.focalLenAspect)/z0) + s_rcfltState.projOffsetY;
			cProj1 = ((ceilRel*s_rcfltState.focalLenAspect)/z1) + s_rcfltState.projOffsetY;
		}

		s32 c0pixel = roundFloat(cProj0);
		s32 c1pixel = roundFloat(cProj1);
		if (c0pixel > s_windowMaxY_Pixels && c1pixel > s_windowMaxY_Pixels)
		{
			s32 x = wallSegment->wallX0;
			s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;
			flat_addEdges(length, x, 0, f32(s_windowMaxY_Pixels + 1), 0, f32(s_windowMaxY_Pixels + 1));
			const f32 numerator = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++)
			{
				s_rcfltState.depth1d[x] = solveForZ(wallSegment, x, numerator);
				s_columnTop[x] = s_windowMaxY_Pixels;
			}

			srcWall->visible = 0;
			srcWall->seen = JTRUE;
			return;
		}

		f32 fProj0, fProj1;
		if ((sector->flags1 & SEC_FLAGS1_PIT) && (nextFlags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))	// floor
		{
			fProj0 = fProj1 = s_rcfltState.windowMaxY;
		}
		else
		{
			f32 floorRel = cachedSector->floorHeight - s_rcfltState.eyeHeight;
			fProj0 = ((floorRel*s_rcfltState.focalLenAspect)/z0) + s_rcfltState.projOffsetY;
			fProj1 = ((floorRel*s_rcfltState.focalLenAspect)/z1) + s_rcfltState.projOffsetY;
		}

		s32 f0pixel = roundFloat(fProj0);
		s32 f1pixel = roundFloat(fProj1);
		if (f0pixel < s_windowMinY_Pixels && f1pixel < s_windowMinY_Pixels)
		{
			s32 x = wallSegment->wallX0;
			s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;
			flat_addEdges(length, x, 0, f32(s_windowMinY_Pixels - 1), 0, f32(s_windowMinY_Pixels - 1));

			const f32 numerator = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++)
			{
				s_rcfltState.depth1d[x] = solveForZ(wallSegment, x, numerator);
				s_columnBot[x] = s_windowMinY_Pixels;
			}
			srcWall->visible = 0;
			srcWall->seen = JTRUE;
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
			y0 = dydxCeil *xStartOffset + cProj0;
			y1 = dydxFloor*xStartOffset + fProj0;
		}

		flat_addEdges(length, x, dydxFloor, y1, dydxCeil, y0);
		f32 nextFloor = fixed16ToFloat(nextSector->floorHeight);
		f32 nextCeil  = fixed16ToFloat(nextSector->ceilingHeight);
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

				s_rcfltState.depth1d[x] = solveForZ(wallSegment, x, numerator);
				y0 += dydxCeil;
				y1 += dydxFloor;
			}
		}

		srcWall->seen = JTRUE;
	}

	void wall_drawBottom(RWallSegmentFloat* wallSegment)
	{
		WallCached* cachedWall = wallSegment->srcWall;
		SectorCached* cachedSector = cachedWall->sector;

		RWall* srcWall = cachedWall->wall;
		RSector* sector = srcWall->sector;
		RSector* nextSector = srcWall->nextSector;
		TextureData* tex = *srcWall->botTex;

		f32 z0 = wallSegment->z0;
		f32 z1 = wallSegment->z1;

		f32 cProj0, cProj1;
		if ((sector->flags1 & SEC_FLAGS1_EXTERIOR) && (nextSector->flags1 & SEC_FLAGS1_EXT_ADJ))
		{
			cProj0 = s_rcfltState.windowMinY;
			cProj1 = cProj0;
		}
		else
		{
			f32 ceilRel = cachedSector->ceilingHeight - s_rcfltState.eyeHeight;
			cProj0 = (ceilRel*s_rcfltState.focalLenAspect)/z0 + s_rcfltState.projOffsetY;
			cProj1 = (ceilRel*s_rcfltState.focalLenAspect)/z1 + s_rcfltState.projOffsetY;
		}

		s32 cy0 = roundFloat(cProj0);
		s32 cy1 = roundFloat(cProj1);
		if (cy0 > s_windowMaxY_Pixels && cy1 >= s_windowMaxY_Pixels)
		{
			srcWall->visible = 0;
			s32 x = wallSegment->wallX0;
			s32 length = wallSegment->wallX1 - x + 1;

			flat_addEdges(length, x, 0, f32(s_windowMaxY_Pixels + 1), 0, f32(s_windowMaxY_Pixels + 1));

			f32 num = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++)
			{
				s_rcfltState.depth1d[x] = solveForZ(wallSegment, x, num);
				s_columnTop[x] = s_windowMaxY_Pixels;
			}
			srcWall->seen = JTRUE;
			return;
		}

		f32 floorRel = cachedSector->floorHeight - s_rcfltState.eyeHeight;
		f32 fProj0 = (floorRel*s_rcfltState.focalLenAspect)/z0 + s_rcfltState.projOffsetY;
		f32 fProj1 = (floorRel*s_rcfltState.focalLenAspect)/z1 + s_rcfltState.projOffsetY;

		s32 fy0 = roundFloat(fProj0);
		s32 fy1 = roundFloat(fProj1);
		if (fy0 < s_windowMinY_Pixels && fy1 < s_windowMinY_Pixels)
		{
			// Wall is above the top of the screen.
			srcWall->visible = 0;
			s32 x = wallSegment->wallX0;
			s32 length = wallSegment->wallX1 - x + 1;

			flat_addEdges(length, x, 0, f32(s_windowMinY_Pixels - 1), 0, f32(s_windowMinY_Pixels - 1));

			f32 num = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++)
			{
				s_rcfltState.depth1d[x] = solveForZ(wallSegment, x, num);
				s_columnBot[x] = s_windowMinY_Pixels;
			}
			srcWall->seen = JTRUE;
			return;
		}

		f32 floorRelNext = fixed16ToFloat(nextSector->floorHeight) - s_rcfltState.eyeHeight;
		f32 fNextProj0 = (floorRelNext*s_rcfltState.focalLenAspect)/z0 + s_rcfltState.projOffsetY;
		f32 fNextProj1 = (floorRelNext*s_rcfltState.focalLenAspect)/z1 + s_rcfltState.projOffsetY;

		s32 xOffset = wallSegment->wallX0 - wallSegment->wallX0_raw;
		s32 length  = wallSegment->wallX1 - wallSegment->wallX0 + 1;
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
			ceil_dYdX  = (cProj1 - cProj0) / lengthRawFixed;
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
		if ((yTop0 > s_windowMinY_Pixels || yTop1 > s_windowMinY_Pixels) && sector->ceilingHeight < nextSector->floorHeight)
		{
			wall_addAdjoinSegment(length, wallSegment->wallX0, floorNext_dYdX, fNextProj0, ceil_dYdX, cProj0, wallSegment);
		}

		if (yTop0 > s_windowMaxY_Pixels && yTop1 > s_windowMaxY_Pixels)
		{
			s32 bot = s_windowMaxY_Pixels + 1;
			f32 num = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++, yC += ceil_dYdX)
			{
				s32 yC_pixel = min(roundFloat(yC), s_windowBot[x]);
				s_columnTop[x] = yC_pixel - 1;
				s_columnBot[x] = bot;
				s_rcfltState.depth1d[x] = solveForZ(wallSegment, x, num);
			}
			srcWall->seen = JTRUE;
			return;
		}

		f32 u0 = wallSegment->uCoord0;
		f32 num = solveForZ_Numerator(wallSegment);
		s_texHeightMask = tex->height - 1;
		JBool flipHorz  = ((srcWall->flags1 & WF1_FLIP_HORIZ)!=0) ? JTRUE : JFALSE;
		JBool illumSign = ((srcWall->flags1 & WF1_ILLUM_SIGN)!=0) ? JTRUE : JFALSE;

		f32 signU0 = 0, signU1 = 0;
		ColumnFunction signFullbright = nullptr, signLit = nullptr;
		TextureData* signTex = setupSignTexture(cachedWall, &signU0, &signU1, &signFullbright, &signLit);

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
					uCoord = u0 + (dxView*wallSegment->uScale) + cachedWall->botOffset.x;
				}
				else
				{
					f32 dz = z - z0;
					uCoord = u0 + (dz*wallSegment->uScale) + cachedWall->botOffset.x;
				}
				s_rcfltState.depth1d[x] = z;
				if (s_yPixelCount > 0)
				{
					s32 widthMask = tex->width - 1;
					s32 texelU = floorFloat(uCoord) & widthMask;
					if (flipHorz != 0)
					{
						texelU = widthMask - texelU;
					}

					f32 vCoordStep = cachedWall->botTexelHeight / (yBot - yTop + 1.0f);
					f32 v0 = (yBot - f32(yBot_pixel) + 0.5f) * vCoordStep;
					s_vCoordFixed = floatToFixed20(v0 + cachedWall->botOffset.z);
					s_vCoordStep  = floatToFixed20(vCoordStep);

					s_texImage = &tex->image[texelU << tex->logSizeY];
					s_columnOut = &s_display[yTop_pixel * s_width + x];
					s_columnLight = computeLighting(z, floor16(srcWall->wallLight));
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
						f32 signYBase = yBot + (cachedWall->signOffset.z/vCoordStep);
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
		srcWall->seen = JTRUE;
	}

	void wall_drawTop(RWallSegmentFloat* wallSegment)
	{
		WallCached* cachedWall = wallSegment->srcWall;
		SectorCached* cachedSector = cachedWall->sector;

		RWall* srcWall = cachedWall->wall;
		RSector* sector = srcWall->sector;
		RSector* next = srcWall->nextSector;
		TextureData* texture = *srcWall->topTex;

		f32 z0 = wallSegment->z0;
		f32 z1 = wallSegment->z1;
		f32 num = solveForZ_Numerator(wallSegment);
				
		s32 x0 = wallSegment->wallX0;
		s32 lengthInPixels = wallSegment->wallX1 - wallSegment->wallX0 + 1;

		f32 ceilRel = cachedSector->ceilingHeight - s_rcfltState.eyeHeight;
		f32 yC0 =((ceilRel*s_rcfltState.focalLenAspect)/z0) + s_rcfltState.projOffsetY;
		f32 yC1 =((ceilRel*s_rcfltState.focalLenAspect)/z1) + s_rcfltState.projOffsetY;

		s32 yC0_pixel = roundFloat(yC0);
		s32 yC1_pixel = roundFloat(yC1);

		s_texHeightMask = texture->height - 1;

		if (yC0_pixel > s_windowMaxY_Pixels && yC1_pixel > s_windowMaxY_Pixels)
		{
			srcWall->visible = 0;
			for (s32 i = 0; i < lengthInPixels; i++) { s_columnTop[x0 + i] = s_windowMaxY_Pixels; }
			flat_addEdges(lengthInPixels, x0, 0, f32(s_windowMaxY_Pixels + 1), 0, f32(s_windowMaxY_Pixels + 1));
			for (s32 i = 0, x = x0; i < lengthInPixels; i++, x++)
			{
				s_rcfltState.depth1d[x] = solveForZ(wallSegment, x, num);
				s_columnTop[x] = s_windowMaxY_Pixels;
			}
			srcWall->seen = JTRUE;
			return;
		}

		f32 yF0, yF1;
		if ((sector->flags1 & SEC_FLAGS1_PIT) && (next->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
		{
			yF0 = yF1 = f32(s_windowMaxY_Pixels);
		}
		else
		{
			f32 floorRel = cachedSector->floorHeight - s_rcfltState.eyeHeight;
			yF0 = (floorRel*s_rcfltState.focalLenAspect)/z0 + s_rcfltState.projOffsetY;
			yF1 = (floorRel*s_rcfltState.focalLenAspect)/z1 + s_rcfltState.projOffsetY;
		}

		s32 yF0_pixel = roundFloat(yF0);
		s32 yF1_pixel = roundFloat(yF1);
		if (yF0_pixel < s_windowMinY_Pixels && yF1_pixel < s_windowMinY_Pixels)
		{
			srcWall->visible = 0;
			for (s32 i = 0; i < lengthInPixels; i++) { s_columnBot[x0 + i] = s_windowMinY_Pixels; }
			flat_addEdges(lengthInPixels, x0, 0, f32(s_windowMinY_Pixels - 1), 0, f32(s_windowMinY_Pixels - 1));
			for (s32 i = 0, x = x0; i < lengthInPixels; i++, x++)
			{
				s_rcfltState.depth1d[x] = solveForZ(wallSegment, x, num);
				s_columnBot[x] = s_windowMinY_Pixels;
			}
			srcWall->seen = JTRUE;
			return;
		}

		f32 next_ceilRel = fixed16ToFloat(next->ceilingHeight) - s_rcfltState.eyeHeight;
		f32 next_yC0 = (next_ceilRel*s_rcfltState.focalLenAspect)/z0 + s_rcfltState.projOffsetY;
		f32 next_yC1 = (next_ceilRel*s_rcfltState.focalLenAspect)/z1 + s_rcfltState.projOffsetY;

		f32 xOffset = f32(wallSegment->wallX0 - wallSegment->wallX0_raw);
		f32 length  = f32(wallSegment->wallX1_raw - wallSegment->wallX0_raw);
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
		if ((next_yC0_pixel < s_windowMaxY_Pixels || next_yC1_pixel < s_windowMaxY_Pixels) && (sector->floorHeight > next->ceilingHeight))
		{
			wall_addAdjoinSegment(lengthInPixels, x0, floor_dYdX, yF0, next_ceil_dYdX, next_yC0, wallSegment);
		}

		if (next_yC0_pixel < s_windowMinY_Pixels && next_yC1_pixel < s_windowMinY_Pixels)
		{
			for (s32 i = 0; i < lengthInPixels; i++) { s_columnTop[x0 + i] = s_windowMinY_Pixels - 1; }
			for (s32 i = 0, x = x0; i < lengthInPixels; i++, x++)
			{
				yF0_pixel = roundFloat(yF0);
				if (yF0_pixel > s_windowBot[x])
				{
					yF0_pixel = s_windowBot[x];
				}

				s_columnBot[x] = yF0_pixel + 1;
				s_rcfltState.depth1d[x] = solveForZ(wallSegment, x, num);
				yF0 += floor_dYdX;
			}
			srcWall->seen = JTRUE;
			return;
		}

		f32 uCoord0 = wallSegment->uCoord0;
		JBool flipHorz  = ((srcWall->flags1 & WF1_FLIP_HORIZ)!=0) ? JTRUE : JFALSE;
		JBool illumSign = ((srcWall->flags1 & WF1_ILLUM_SIGN)!=0) ? JTRUE : JFALSE;

		f32 signU0 = 0, signU1 = 0;
		ColumnFunction signFullbright = nullptr, signLit = nullptr;
		TextureData* signTex = setupSignTexture(cachedWall, &signU0, &signU1, &signFullbright, &signLit);

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

			f32 uScale = wallSegment->uScale;
			f32 uCoord0 = wallSegment->uCoord0 + cachedWall->topOffset.x;
			f32 uCoord = uCoord0 + ((wallSegment->orient == WORIENT_DZ_DX) ? dxView*uScale : (z - z0)*uScale);

			s_rcfltState.depth1d[x] = z;
			if (s_yPixelCount > 0)
			{
				s32 widthMask = texture->width - 1;
				s32 texelU = floorFloat(uCoord) & widthMask;
				if (flipHorz)
				{
					texelU = widthMask - texelU;
				}
				assert(texelU >= 0 && texelU <= widthMask);

				f32 vCoordStep = (cachedWall->topTexelHeight) / (next_yC0 - yC0 + 1.0f);
				s_vCoordFixed  = floatToFixed20(cachedWall->topOffset.z + (next_yC0 - f32(next_yC0_pixel) + 0.5f)*vCoordStep);
				s_vCoordStep   = floatToFixed20(vCoordStep);
				s_texImage = &texture->image[texelU << texture->logSizeY];

				s_columnOut = &s_display[yC0_pixel * s_width + x];
				s_columnLight = computeLighting(z, floor16(srcWall->wallLight));
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
					f32 signYBase = next_yC0 + (cachedWall->signOffset.z/vCoordStep);
					s32 y0 = max(floorFloat(signYBase - (f32(signTex->height)/vCoordStep) + 1.5f), yC0_pixel);
					s32 y1 = min(floorFloat(signYBase + 0.5f), next_yC0_pixel);
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

			yC0 += ceil_dYdX;
			next_yC0 += next_ceil_dYdX;
			yF0 += floor_dYdX;
		}
		
		srcWall->seen = JTRUE;
	}

	void wall_drawTopAndBottom(RWallSegmentFloat* wallSegment)
	{
		WallCached* cachedWall = wallSegment->srcWall;
		SectorCached* cachedSector = cachedWall->sector;

		RWall* srcWall = cachedWall->wall;
		RSector* sector = srcWall->sector;
		TextureData* topTex = *srcWall->topTex;

		f32 z0 = wallSegment->z0;
		f32 z1 = wallSegment->z1;
		s32 x0 = wallSegment->wallX0;
		f32 xOffset = f32(wallSegment->wallX0 - wallSegment->wallX0_raw);
		s32 length  = wallSegment->wallX1 - wallSegment->wallX0 + 1;
		f32 lengthRaw = f32(wallSegment->wallX1_raw - wallSegment->wallX0_raw);

		f32 ceilRel = cachedSector->ceilingHeight - s_rcfltState.eyeHeight;
		f32 cProj0 = (ceilRel*s_rcfltState.focalLenAspect)/z0 + s_rcfltState.projOffsetY;
		f32 cProj1 = (ceilRel*s_rcfltState.focalLenAspect)/z1 + s_rcfltState.projOffsetY;

		s32 c0_pixel = roundFloat(cProj0);
		s32 c1_pixel = roundFloat(cProj1);

		if (c0_pixel > s_windowMaxY_Pixels && c1_pixel > s_windowMaxY_Pixels)
		{
			srcWall->visible = 0;
			for (s32 i = 0; i < length; i++) { s_columnTop[x0 + i] = s_windowMaxY_Pixels; }

			flat_addEdges(length, x0, 0, f32(s_windowMaxY_Pixels + 1), 0, f32(s_windowMaxY_Pixels + 1));
			f32 num = solveForZ_Numerator(wallSegment);
			for (s32 i = 0, x = x0; i < length; i++, x++)
			{
				s_rcfltState.depth1d[x] = solveForZ(wallSegment, x, num);
				s_columnTop[x] = s_windowMaxY_Pixels;
			}
			srcWall->seen = JTRUE;
			return;
		}

		f32 floorRel = cachedSector->floorHeight - s_rcfltState.eyeHeight;
		f32 fProj0 = (floorRel*s_rcfltState.focalLenAspect)/z0 + s_rcfltState.projOffsetY;
		f32 fProj1 = (floorRel*s_rcfltState.focalLenAspect)/z1 + s_rcfltState.projOffsetY;

		s32 f0_pixel = roundFloat(fProj0);
		s32 f1_pixel = roundFloat(fProj1);
		if (f0_pixel < s_windowMinY_Pixels && f1_pixel < s_windowMinY_Pixels)
		{
			srcWall->visible = 0;
			for (s32 i = 0; i < length; i++) { s_columnBot[x0 + i] = s_windowMinY_Pixels; }

			flat_addEdges(length, x0, 0, f32(s_windowMinY_Pixels - 1), 0, f32(s_windowMinY_Pixels - 1));
			f32 num = solveForZ_Numerator(wallSegment);

			for (s32 i = 0, x = x0; i < length; i++, x++)
			{
				s_rcfltState.depth1d[x] = solveForZ(wallSegment, x, num);
				s_columnBot[x] = s_windowMinY_Pixels;
			}
			srcWall->seen = JTRUE;
			return;
		}

		RSector* nextSector = srcWall->nextSector;
		f32 next_ceilRel = fixed16ToFloat(nextSector->ceilingHeight) - s_rcfltState.eyeHeight;
		f32 next_cProj0 = (next_ceilRel*s_rcfltState.focalLenAspect)/z0 + s_rcfltState.projOffsetY;
		f32 next_cProj1 = (next_ceilRel*s_rcfltState.focalLenAspect)/z1 + s_rcfltState.projOffsetY;

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
		if (cn0_pixel >= s_windowMinY_Pixels || cn1_pixel >= s_windowMinY_Pixels)
		{
			f32 u0 = wallSegment->uCoord0;
			f32 num = solveForZ_Numerator(wallSegment);
			s_texHeightMask = topTex->height - 1;
			JBool flipHorz = ((srcWall->flags1 & WF1_FLIP_HORIZ)!=0) ? JTRUE : JFALSE;

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
					u = u0 + (dxView*wallSegment->uScale) + cachedWall->topOffset.x;
				}
				else
				{
					f32 dz = z - z0;
					u = u0 + (dz*wallSegment->uScale) + cachedWall->topOffset.x;
				}
				s_rcfltState.depth1d[x] = z;
				if (s_yPixelCount > 0)
				{
					s32 widthMask = topTex->width - 1;
					s32 texelU = floorFloat(u) & widthMask;
					if (flipHorz)
					{
						texelU = widthMask - texelU;
					}
					f32 vCoordStep = cachedWall->topTexelHeight / (yC1 - yC0 + 1.0f);
					f32 yOffset = yC1 - f32(yC1_pixel) + 0.5f;
					s_vCoordFixed = floatToFixed20(yOffset*vCoordStep + cachedWall->topOffset.z);
					s_vCoordStep  = floatToFixed20(vCoordStep);

					s_texImage = &topTex->image[texelU << topTex->logSizeY];
					s_columnOut = &s_display[yC0_pixel * s_width + x];
					s_columnLight = computeLighting(z, floor16(srcWall->wallLight));

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
			for (s32 i = 0; i < length; i++) { s_columnTop[x0 + i] = s_windowMinY_Pixels - 1; }
		}

		f32 next_floorRel = fixed16ToFloat(nextSector->floorHeight) - s_rcfltState.eyeHeight;
		f32 next_fProj0 = (next_floorRel*s_rcfltState.focalLenAspect)/z0 + s_rcfltState.projOffsetY;
		f32 next_fProj1 = (next_floorRel*s_rcfltState.focalLenAspect)/z1 + s_rcfltState.projOffsetY;

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
		TextureData* botTex = *srcWall->botTex;
		f0_pixel = roundFloat(next_fProj0);
		f1_pixel = roundFloat(next_fProj1);

		if (f0_pixel <= s_windowMaxY_Pixels || f1_pixel <= s_windowMaxY_Pixels)
		{
			f32 u0 = wallSegment->uCoord0;
			f32 num = solveForZ_Numerator(wallSegment);

			s_texHeightMask = botTex->height - 1;
			JBool flipHorz  = ((srcWall->flags1 & WF1_FLIP_HORIZ)!=0) ? JTRUE : JFALSE;
			JBool illumSign = ((srcWall->flags1 & WF1_ILLUM_SIGN)!=0) ? JTRUE : JFALSE;

			f32 signU0 = 0, signU1 = 0;
			ColumnFunction signFullbright = nullptr, signLit = nullptr;
			TextureData* signTex = setupSignTexture(cachedWall, &signU0, &signU1, &signFullbright, &signLit);

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
						uCoord = u0 + (dxView*wallSegment->uScale) + cachedWall->botOffset.x;
					}
					else
					{
						f32 dz = z - z0;
						uCoord = u0 + (dz*wallSegment->uScale) + cachedWall->botOffset.x;
					}
					s_rcfltState.depth1d[x] = z;
					if (s_yPixelCount > 0)
					{
						s32 widthMask = botTex->width - 1;
						s32 texelU = floorFloat(uCoord) & widthMask;
						if (flipHorz)
						{
							texelU = widthMask - texelU;
						}
						f32 vCoordStep = (cachedWall->botTexelHeight) / (yF1 - yF0 + 1.0f);
						s_vCoordFixed  = floatToFixed20(cachedWall->botOffset.z + (yF1 - f32(yF1_pixel) + 0.5f)*vCoordStep);
						s_vCoordStep   = floatToFixed20(vCoordStep);

						s_texImage = &botTex->image[texelU << botTex->logSizeY];
						s_columnOut = &s_display[yF0_pixel * s_width + x];
						s_columnLight = computeLighting(z, floor16(srcWall->wallLight));

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
							f32 signYBase = yF1 + (cachedWall->signOffset.z/vCoordStep);
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
			for (s32 i = 0; i < length; i++) { s_columnBot[x0 + i] = s_windowMaxY_Pixels + 1; }
		}
		flat_addEdges(length, x0, floor_dYdX, fProj0, ceil_dYdX, cProj0);

		s32 next_f0_pixel = roundFloat(next_fProj0);
		s32 next_f1_pixel = roundFloat(next_fProj1);
		s32 next_c0_pixel = roundFloat(next_cProj0);
		s32 next_c1_pixel = roundFloat(next_cProj1);
		if ((next_f0_pixel <= s_windowMinY_Pixels && next_f1_pixel <= s_windowMinY_Pixels) || (next_c0_pixel >= s_windowMaxY_Pixels && next_c1_pixel >= s_windowMaxY_Pixels) || (nextSector->floorHeight <= nextSector->ceilingHeight))
		{
			srcWall->seen = JTRUE;
			return;
		}

		wall_addAdjoinSegment(length, x0, next_floor_dYdX, next_fProj0 - 1.0f, next_ceil_dYdX, next_cProj0 + 1.0f, wallSegment);
		srcWall->seen = JTRUE;
	}

	// Parts of the code inside 's_height == SKY_BASE_HEIGHT' are based on the original DOS exe.
	// Other parts of those same conditionals are modified to handle higher resolutions.
	void wall_drawSkyTop(RSector* sector)
	{
		if (s_wallMaxCeilY < s_windowMinY_Pixels) { return; }
		TFE_ZONE("Draw Sky");

		TextureData* texture = *sector->ceilTex;
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

		for (s32 x = s_windowMinX_Pixels; x <= s_windowMaxX_Pixels; x++)
		{
			const s32 y0 = s_windowTop[x];
			const s32 y1 = min(s_columnTop[x], s_windowBot[x]);

			s_yPixelCount = y1 - y0 + 1;
			if (s_yPixelCount > 0)
			{
				if (s_height == SKY_BASE_HEIGHT)
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask - y1) - s_rcfltState.skyPitchOffset - fixed16ToFloat(sector->ceilOffset.z));
				}
				else
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask) - (f32(y1)*heightScale) - s_rcfltState.skyPitchOffset - fixed16ToFloat(sector->ceilOffset.z));
				}

				s32 texelU = (floorFloat(fixed16ToFloat(sector->ceilOffset.x) - s_rcfltState.skyYawOffset + s_rcfltState.skyTable[x]) ) & texWidthMask;
				s_texImage = &texture->image[texelU << texture->logSizeY];
				s_columnOut = &s_display[y0*s_width + x];
				drawColumn_Fullbright();
			}
		}
	}

	void wall_drawSkyTopNoWall(RSector* sector)
	{
		TFE_ZONE("Draw Sky");
		const TextureData* texture = *sector->ceilTex;

		f32 heightScale;
		f32 vCoordStep;
		// In the original code (at the original 200p resolution) the sky is setup to step exactly one texel per vertical pixel
		// However with higher resolutions this must be scaled to look the same.
		if (s_height == SKY_BASE_HEIGHT)
		{
			vCoordStep  = 1.0f;
			heightScale = 1.0f;
		}
		else
		{
			vCoordStep  = f32(SKY_BASE_HEIGHT) / f32(s_height);
			heightScale = vCoordStep;
		}
		s_vCoordStep = floatToFixed20(vCoordStep);

		s_texHeightMask = texture->height - 1;
		for (s32 x = s_windowMinX_Pixels; x <= s_windowMaxX_Pixels; x++)
		{
			const s32 y0 = s_windowTop[x];
			const s32 y1 = min(s_screenYMidFlt - 1, s_windowBot[x]);

			s_yPixelCount = y1 - y0 + 1;
			if (s_yPixelCount > 0)
			{
				if (s_height == SKY_BASE_HEIGHT)
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask - y1) - s_rcfltState.skyPitchOffset - fixed16ToFloat(sector->ceilOffset.z));
				}
				else
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask) - (f32(y1)*heightScale) - s_rcfltState.skyPitchOffset - fixed16ToFloat(sector->ceilOffset.z));
				}

				s32 widthMask = texture->width - 1;
				s32 texelU = floorFloat(fixed16ToFloat(sector->ceilOffset.x) - s_rcfltState.skyYawOffset + s_rcfltState.skyTable[x]) & widthMask;
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
		if (s_wallMinFloorY > s_windowMaxY_Pixels) { return; }
		TFE_ZONE("Draw Sky");

		TextureData* texture = *sector->floorTex;
		f32 heightScale;
		f32 vCoordStep;
		// In the original code (at the original 200p resolution) the sky is setup to step exactly one texel per vertical pixel
		// However with higher resolutions this must be scaled to look the same.
		if (s_height == SKY_BASE_HEIGHT)
		{
			vCoordStep  = 1.0f;
			heightScale = 1.0f;
		}
		else
		{
			vCoordStep  = f32(SKY_BASE_HEIGHT) / f32(s_height);
			heightScale = vCoordStep;
		}
		s_vCoordStep = floatToFixed20(vCoordStep);

		s_texHeightMask = texture->height - 1;
		const s32 texWidthMask = texture->width - 1;

		for (s32 x = s_windowMinX_Pixels; x <= s_windowMaxX_Pixels; x++)
		{
			const s32 y0 = max(s_columnBot[x], s_windowTop[x]);
			const s32 y1 = s_windowBot[x];

			s_yPixelCount = y1 - y0 + 1;
			if (s_yPixelCount > 0)
			{
				if (s_height == SKY_BASE_HEIGHT)
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask - y1) - s_rcfltState.skyPitchOffset - fixed16ToFloat(sector->floorOffset.z));
				}
				else
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask) - (f32(y1)*heightScale) - s_rcfltState.skyPitchOffset - fixed16ToFloat(sector->floorOffset.z));
				}

				s32 texelU = floorFloat(fixed16ToFloat(sector->floorOffset.x) - s_rcfltState.skyYawOffset + s_rcfltState.skyTable[x]) & texWidthMask;
				s_texImage = &texture->image[texelU << texture->logSizeY];
				s_columnOut = &s_display[y0*s_width + x];
				drawColumn_Fullbright();
			}
		}
	}

	void wall_drawSkyBottomNoWall(RSector* sector)
	{
		TFE_ZONE("Draw Sky");
		const TextureData* texture = *sector->floorTex;

		f32 heightScale;
		f32 vCoordStep;
		// In the original code (at the original 200p resolution) the sky is setup to step exactly one texel per vertical pixel
		// However with higher resolutions this must be scaled to look the same.
		if (s_height == SKY_BASE_HEIGHT)
		{
			vCoordStep  = 1.0f;
			heightScale = 1.0f;
		}
		else
		{
			vCoordStep  = f32(SKY_BASE_HEIGHT) / f32(s_height);
			heightScale = vCoordStep;
		}
		s_vCoordStep = floatToFixed20(vCoordStep);

		s_texHeightMask = texture->height - 1;
		for (s32 x = s_windowMinX_Pixels; x <= s_windowMaxX_Pixels; x++)
		{
			const s32 y0 = max(s_screenYMidFlt, s_windowTop[x]);
			const s32 y1 = s_windowBot[x];

			s_yPixelCount = y1 - y0 + 1;
			if (s_yPixelCount > 0)
			{
				if (s_height == SKY_BASE_HEIGHT)
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask - y1) - s_rcfltState.skyPitchOffset - fixed16ToFloat(sector->floorOffset.z));
				}
				else
				{
					s_vCoordFixed = floatToFixed20(f32(s_texHeightMask) - (f32(y1)*heightScale) - s_rcfltState.skyPitchOffset - fixed16ToFloat(sector->floorOffset.z));
				}

				s32 widthMask = texture->width - 1;
				s32 texelU = floorFloat(fixed16ToFloat(sector->floorOffset.x) - s_rcfltState.skyYawOffset + s_rcfltState.skyTable[x]) & widthMask;
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
	f32 solveForZ_Numerator(RWallSegmentFloat* wallSegment)
	{
		const f32 z0 = wallSegment->z0;

		f32 numerator;
		if (wallSegment->orient == WORIENT_DZ_DX)
		{
			numerator = z0 - (wallSegment->slope * wallSegment->x0View);
		}
		else  // WORIENT_DX_DZ
		{
			numerator = wallSegment->x0View - (wallSegment->slope * z0);
		}

		return numerator;
	}
	
	// Solve for perspective correct Z at the current x pixel coordinate.
	f32 solveForZ(RWallSegmentFloat* wallSegment, s32 x, f32 numerator, f32* outViewDx/*=nullptr*/)
	{
		f32 z;	// perspective correct z coordinate at the current x pixel coordinate.
		if (wallSegment->orient == WORIENT_DZ_DX)
		{
			// Solve for viewspace X at the current pixel x coordinate in order to get dx in viewspace.
			const f32 fx = f32(x);
			// Scale halfWidthOverX by focal length to account for widescreen.
			// Note in the original code s_focalLength == s_halfWidth, so in that case the code is functionally equivalent.
			const f32 halfWidthOverX = ((fx != s_rcfltState.halfWidth) ? s_rcfltState.focalLength / (fx - s_rcfltState.halfWidth) : s_rcfltState.focalLength);
			f32 den = halfWidthOverX - wallSegment->slope;
			// Avoid divide by zero.
			if (den == 0.0f) { den = 1.0f; }

			f32 xView = numerator/den;
			// Use the saved x0View to get dx in viewspace.
			f32 dxView = xView - wallSegment->x0View;
			// avoid recalculating for u coordinate computation.
			if (outViewDx) { *outViewDx = dxView; }
			// Once we have dx in viewspace, we multiply by the slope (dZ/dX) in order to get dz.
			f32 dz = dxView * wallSegment->slope;
			// Then add z0 to get z(x) that is perspective correct.
			z = wallSegment->z0 + dz;
		}
		else  // WORIENT_DX_DZ
		{
			// Directly solve for Z at the current pixel x coordinate.
			// Scale xOverHalfWidth by focal length to account for widescreen.
			// Note in the original code s_focalLength == s_halfWidth, so in that case the code is functionally equivalent.
			const f32 xOverHalfWidth = (f32(x) - s_rcfltState.halfWidth) / s_rcfltState.focalLength;
			f32 den = xOverHalfWidth - wallSegment->slope;
			// Avoid divide by 0.
			if (den == 0.0f) { den = 1.0f; }

			z = numerator / den;
		}

		return z;
	}

	void drawColumn_Fullbright()
	{
		fixed44_20 vCoordFixed = s_vCoordFixed;
		const u8* tex = s_texImage;
		const s32 end = s_yPixelCount - 1;

		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width, vCoordFixed += s_vCoordStep)
		{
			const s32 v = floor20(vCoordFixed) & s_texHeightMask;
			s_columnOut[offset] = tex[v];
		}
	}

	void drawColumn_Lit()
	{
		fixed44_20 vCoordFixed = s_vCoordFixed;
		const u8* tex = s_texImage;
		const s32 end = s_yPixelCount - 1;

		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width, vCoordFixed += s_vCoordStep)
		{
			const s32 v = floor20(vCoordFixed) & s_texHeightMask;
			s_columnOut[offset] = s_columnLight[tex[v]];
		}
	}

	void drawColumn_Fullbright_Trans()
	{
		fixed44_20 vCoordFixed = s_vCoordFixed;
		const u8* tex = s_texImage;
		const s32 end = s_yPixelCount - 1;

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
		fixed44_20 vCoordFixed = s_vCoordFixed;
		const u8* tex = s_texImage;
		const s32 end = s_yPixelCount - 1;

		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width, vCoordFixed += s_vCoordStep)
		{
			const s32 v = floor20(vCoordFixed) & s_texHeightMask;
			const u8 c = tex[v];
			if (c) { s_columnOut[offset] = s_columnLight[c]; }
		}
	}

	void wall_addAdjoinSegment(s32 length, s32 x0, f32 top_dydx, f32 y1, f32 bot_dydx, f32 y0, RWallSegmentFloat* wallSegment)
	{
		if (s_adjoinSegCount < MAX_ADJOIN_SEG)
		{
			f32 lengthFlt = f32(length - 1);
			f32 y0End = y0;
			if (bot_dydx != 0)
			{
				y0End += (bot_dydx * lengthFlt);
			}
			f32 y1End = y1;
			if (top_dydx != 0)
			{
				y1End += (top_dydx * lengthFlt);
			}
			edgePair_setup(length, x0, top_dydx, y1End, y1, bot_dydx, y0, y0End, s_rcfltState.adjoinEdge);

			s_rcfltState.adjoinEdge++;
			s_adjoinSegCount++;

			*s_rcfltState.adjoinSegment = wallSegment;
			s_rcfltState.adjoinSegment++;
		}
	}

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
	void sprite_drawFrame(u8* basePtr, WaxFrame* frame, SecObject* obj, vec3_float* cachedPosVS)
	{
		if (!frame) { return; }

		const WaxCell* cell = WAX_CellPtr(basePtr, frame);
		const f32 z = cachedPosVS->z;
		const s32 flip = frame->flip;
		// Make sure the sprite isn't behind the near plane.
		if (z < 1.0f) { return; }
		JBool drawn = JFALSE;

		const f32 widthWS  = fixed16ToFloat(frame->widthWS);
		const f32 heightWS = fixed16ToFloat(frame->heightWS);
		const f32 fOffsetX = fixed16ToFloat(frame->offsetX);
		const f32 fOffsetY = fixed16ToFloat(frame->offsetY);

		const f32 x0 = cachedPosVS->x - fOffsetX;
		const f32 yOffset = heightWS  - fOffsetY;
		const f32 y0 = cachedPosVS->y - yOffset;

		const f32 rcpZ = 1.0f/z;
		const f32 projX0 = x0*s_rcfltState.focalLength   *rcpZ + s_rcfltState.projOffsetX;
		const f32 projY0 = y0*s_rcfltState.focalLenAspect*rcpZ + s_rcfltState.projOffsetY;

		s32 x0_pixel = roundFloat(projX0);
		s32 y0_pixel = roundFloat(projY0);
		if (x0_pixel > s_windowMaxX_Pixels || y0_pixel > s_windowMaxY_Pixels)
		{
			return;
		}

		const f32 x1 = x0 + widthWS;
		const f32 y1 = y0 + heightWS;
		const f32 projX1 = x1*s_rcfltState.focalLength   *rcpZ + s_rcfltState.projOffsetX;
		const f32 projY1 = y1*s_rcfltState.focalLenAspect*rcpZ + s_rcfltState.projOffsetY;

		s32 x1_pixel = roundFloat(projX1);
		s32 y1_pixel = roundFloat(projY1);
		if (x1_pixel < s_windowMinX_Pixels || y1_pixel < s_windowMinY_Pixels)
		{
			return;
		}

		const s32 length = x1_pixel - x0_pixel + 1;
		if (length <= 0)
		{
			return;
		}

		const f32 height = projY1 - projY0 + 1.0f;
		const f32 width  = projX1 - projX0 + 1.0f;
		const f32 uCoordStep = f32(cell->sizeX) / width;
		const f32 vCoordStep = f32(cell->sizeY) / height;
		s_vCoordStep = floatToFixed20(vCoordStep);

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

		// Compute the lighting for the whole sprite.
		s_columnLight = computeLighting(z, 0);

		// Figure out the correct column function.
		ColumnFunction spriteColumnFunc;
		if (s_columnLight && !(obj->flags & OBJ_FLAG_FULLBRIGHT) && !s_flatLighting)
		{
			spriteColumnFunc = s_columnFunc[COLFUNC_LIT_TRANS];
		}
		else
		{
			spriteColumnFunc = s_columnFunc[COLFUNC_FULLBRIGHT_TRANS];
		}

		// Draw
		const s32 compressed = cell->compressed;
		u8* imageData = (u8*)cell + sizeof(WaxCell);

		s32 n;
		u8* image;
		if (compressed == 1)
		{
			n = -1;
			image = imageData + (cell->sizeX * sizeof(u32));
		}
		else
		{
			n = 0;
			image = imageData;
		}

		if (x0_pixel > x1_pixel)
		{
			return;
		}

		// This should be set to handle all sizes, repeating is not required.
		s_texHeightMask = 0xffff;

		const u32* columnOffset = (u32*)(basePtr + cell->columnOffset);
		for (s32 x = x0_pixel; x <= x1_pixel; x++, uCoord += uCoordStep)
		{
			if (z < s_rcfltState.depth1d[x])
			{
				s32 y0 = y0_pixel;
				s32 y1 = y1_pixel;

				const s32 top = s_objWindowTop[x];
				if (y0 < top)
				{
					y0 = top;
				}
				const s32 bot = s_objWindowBot[x];
				if (y1 > bot)
				{
					y1 = bot;
				}

				s_yPixelCount = y1 - y0 + 1;
				if (s_yPixelCount > 0)
				{
					const f32 vOffset = f32(y1_pixel - y1);
					s_vCoordFixed = floatToFixed20(vOffset*vCoordStep);

					s32 texelU = min(cell->sizeX-1, floorFloat(uCoord));
					if (flip)
					{
						texelU = cell->sizeX - texelU - 1;
					}

					if (compressed)
					{
						const u8* colPtr = (u8*)cell + columnOffset[texelU];

						// Decompress the column into "work buffer."
						assert(cell->sizeY <= 1024 && texelU >= 0 && texelU < cell->sizeX);
						sprite_decompressColumn(colPtr, s_workBuffer, cell->sizeY);
						s_texImage = (u8*)s_workBuffer;
					}
					else
					{
						s_texImage = (u8*)image + columnOffset[texelU];
					}
					// Output.
					s_columnOut = &s_display[y0 * s_width + x];
					// Draw the column.
					spriteColumnFunc();
					if (s_yPixelCount > 1) { drawn = JTRUE; }
				}
			}
		}

		if (drawn && s_drawnSpriteCount < MAX_DRAWN_SPRITE_STORE)
		{
			s_drawnSprites[s_drawnSpriteCount++] = obj;
		}
	}
}  // RClassic_Float

}  // TFE_Jedi