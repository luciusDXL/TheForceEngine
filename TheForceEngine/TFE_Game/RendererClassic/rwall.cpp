#include "rwall.h"
#include "rflat.h"
#include "rlighting.h"
#include "rsector.h"
#include "redgePair.h"
#include "fixedPoint.h"
#include "rmath.h"
#include "rcommon.h"
#include <TFE_Renderer/renderer.h>
#include <TFE_System/system.h>
#include <TFE_System/math.h>
#include <TFE_System/profiler.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Asset/levelAsset.h>
#include <TFE_Asset/levelObjectsAsset.h>
#include <TFE_Asset/colormapAsset.h>
#include <TFE_Game/level.h>
#include <TFE_Asset/spriteAsset.h>
#include <TFE_Asset/textureAsset.h>
#include <TFE_Asset/paletteAsset.h>
#include <TFE_Asset/colormapAsset.h>
#include <TFE_Asset/modelAsset.h>
#include <TFE_LogicSystem/logicSystem.h>
#include <TFE_FrontEndUI/console.h>
#include <algorithm>
#include <assert.h>

using namespace RendererClassic;
using namespace FixedPoint;
using namespace RMath;
using namespace RClassicFlat;
using namespace RClassicLighting;
using namespace RClassicEdgePair;

namespace RClassicWall
{
	#define SKY_BASE_HEIGHT 200

	enum SegSide
	{
		FRONT = 0xffff,
		BACK = 0,
	};

	static fixed16_16 s_segmentCross;
	static s32 s_texHeightMask;
	static s32 s_yPixelCount;
	static fixed16_16 s_vCoordStep;
	static fixed16_16 s_vCoordFixed;
	static const u8* s_columnLight;
	static u8* s_texImage;
	static u8* s_columnOut;

	s32 segmentCrossesLine(fixed16_16 ax0, fixed16_16 ay0, fixed16_16 ax1, fixed16_16 ay1, fixed16_16 bx0, fixed16_16 by0, fixed16_16 bx1, fixed16_16 by1);
	fixed16_16 solveForZ_Numerator(RWallSegment* wallSegment);
	fixed16_16 solveForZ(RWallSegment* wallSegment, s32 x, fixed16_16 numerator, fixed16_16* outViewDx=nullptr);
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

	fixed16_16 frustumIntersect(fixed16_16 x0, fixed16_16 z0, fixed16_16 x1, fixed16_16 z1, fixed16_16 dx, fixed16_16 dz)
	{
		fixed16_16 xz;
		if (!s_enableHighPrecision)
		{
			// Original DOS code.
			xz = mul16(x0, z1) - mul16(z0, x1);
			fixed16_16 dyx = dz - dx;
			if (dyx != 0)
			{
				xz = div16(xz, dyx);
			}
		}
		else
		{
			// Overflow resistent code as seen in later versions of the engine.
			fixed48_16 xzLarge = mul16(fixed48_16(x0), fixed48_16(z1)) - mul16(fixed48_16(z0), fixed48_16(x1));
			fixed48_16 dyx = dz - dx;
			if (dyx != 0)
			{
				xzLarge = div16(xzLarge, dyx);
			}
			xz = fixed16_16(xzLarge);
		}
		return xz;
	}

	// Process the wall and produce an RWallSegment for rendering if the wall is potentially visible.
	void wall_process(RWall* wall)
	{
		const vec2* p0 = wall->v0;
		const vec2* p1 = wall->v1;

		// viewspace wall coordinates.
		fixed16_16 x0 = p0->x;
		fixed16_16 x1 = p1->x;
		fixed16_16 z0 = p0->z;
		fixed16_16 z1 = p1->z;

		// x values of frustum lines that pass through (x0,z0) and (x1,z1)
		fixed16_16 left0 = -z0;
		fixed16_16 left1 = -z1;
		fixed16_16 right0 = z0;
		fixed16_16 right1 = z1;

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

		fixed16_16 dx = x1 - x0;
		fixed16_16 dz = z1 - z0;
		// Cull the wall if it is back facing.
		// y0*dx - x0*dy
		const fixed16_16 side = mul16(z0, dx) - mul16(x0, dz);
		if (side < 0)
		{
			wall->visible = 0;
			return;
		}

		fixed16_16 curU = 0;
		s32 clipLeft = 0;
		s32 clipRight = 0;
		s32 clipX1_Near = 0;
		s32 clipX0_Near = 0;

		fixed16_16 texelLen = wall->texelLength;
		fixed16_16 texelLenRem = texelLen;

		//////////////////////////////////////////////
		// Clip the Wall Segment by the left and right
		// frustum lines.
		//////////////////////////////////////////////

		// The wall segment extends past the left clip line.
		if (x0 < left0)
		{
			// Intersect the segment (x0, z0),(x1, z1) with the frustum line that passes through (-z0, z0) and (-z1, z1)
			const fixed16_16 xz = frustumIntersect(x0, z0, x1, z1, dx, -dz);

			// Compute the parametric intersection of the segment and the left frustum line
			// where s is in the range of [0.0, 1.0]
			fixed16_16 s = 0;
			if (dz != 0 && RMath::abs(dz) > RMath::abs(dx))
			{
				s = div16(xz - z0, dz);
			}
			else if (dx != 0)
			{
				s = div16(-xz - x0, dx);
			}

			// Update the x0,y0 coordinate of the segment.
			x0 = -xz;
			z0 =  xz;

			if (s != 0)
			{
				// length of the clipped portion of the remaining texel length.
				fixed16_16 clipLen = mul16(texelLenRem, s);
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
			const fixed16_16 xz = frustumIntersect(x0, z0, x1, z1, dx, dz);

			// Compute the parametric intersection of the segment and the left frustum line
			// where s is in the range of [0.0, 1.0]
			// Note we are computing from the right side, i.e. distance from (x1,y1).
			fixed16_16 s = 0;
			if (dz != 0 && RMath::abs(dz) > RMath::abs(dx))
			{
				s = div16(xz - z1, dz);
			}
			else if (dx != 0)
			{
				s = div16(xz - x1, dx);
			}

			// Update the x1,y1 coordinate of the segment.
			x1 = xz;
			z1 = xz;
			if (s != 0)
			{
				fixed16_16 adjLen = texelLen + mul16(texelLenRem, s);
				fixed16_16 adjLenMinU = adjLen - curU;

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
		if ((z0 < 0 || z1 < 0) && segmentCrossesLine(0, 0, 0, -s_halfHeight, x0, x0, x1, z1) != 0)
		{
			wall->visible = 0;
			return;
		}
		if (z0 < ONE_16 && z1 < ONE_16)
		{
			if (clipLeft != 0)
			{
				clipX0_Near = -1;
				x0 = -ONE_16;
			}
			else
			{
				x0 = div16(x0, z0);
			}
			if (clipRight != 0)
			{
				x1 = ONE_16;
				clipX1_Near = -1;
			}
			else
			{
				x1 = div16(x1, z1);
			}
			dx = x1 - x0;
			dz = 0;
			z0 = ONE_16;
			z1 = ONE_16;
		}
		else if (z0 < ONE_16)
		{
			if (clipLeft != 0)
			{
				if (dz != 0)
				{
					fixed16_16 left = div16(z0, dz);
					x0 += mul16(dx, left);
					//curU += mul16(texelLenRem, left);

					dx = x1 - x0;
					texelLenRem = texelLen - curU;
				}
				z0 = ONE_16;
				clipX0_Near = -1;
				dz = z1 - ONE_16;
			}
			else
			{
				// BUG: This is NOT correct but matches the original implementation.
				// I am leaving it as-is to match the original DOS renderer.
				// Fortunately hitting this case is VERY rare in practice.
				x0 = div16(x0, z0);
				z0 = ONE_16;
				dz = z1 - ONE_16;
				dx -= x0;
			}
		}
		else if (z1 < ONE_16)
		{
			if (clipRight != 0)
			{
				if (dz != 0)
				{
					fixed16_16 s = div16(ONE_16 - x1, dz);
					x1 += mul16(dx, s);
					texelLen += mul16(texelLenRem, s);
					texelLenRem = texelLen - curU;
					dx = x1 - x0;
				}
				z1 = ONE_16;
				dz = ONE_16 - z0;
				clipX1_Near = -1;
			}
			else
			{
				// BUG: This is NOT correct but matches the original implementation.
				// I am leaving it as-is to match the original DOS renderer.
				// Fortunately hitting this case is VERY rare in practice.
				x1 = div16(x1, z1);
				z1 = ONE_16;
				dx = x1 - x0;
				dz = z1 - z0;
			}
		}
		
		//////////////////////////////////////////////////
		// Project.
		//////////////////////////////////////////////////
		fixed16_16 x0proj, x1proj;
		if (!s_enableHighPrecision)
		{
			// Original DOS code
			x0proj = div16(mul16(x0, s_focalLength), z0) + s_halfWidth;
			x1proj = div16(mul16(x1, s_focalLength), z1) + s_halfWidth;
		}
		else
		{
			// Improved code that better avoids overflow, as seen in the MAC version.
			x0proj = fusedMulDiv(x0, s_focalLength, z0) + s_halfWidth;
			x1proj = fusedMulDiv(x1, s_focalLength, z1) + s_halfWidth;
		}
		s32 x0pixel = round16(x0proj);
		s32 x1pixel = round16(x1proj) - 1;
		
		// Handle near plane clipping by adjusting the walls to avoid holes.
		if (clipX0_Near != 0 && x0pixel > s_minScreenX)
		{
			x0 = -ONE_16;
			dx = x1 + ONE_16;
			x0pixel = s_minScreenX;
		}
		if (clipX1_Near != 0 && x1pixel < s_maxScreenX)
		{
			dx = ONE_16 - x0;
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
		wallSeg->z0 = z0;
		wallSeg->z1 = z1;
		wallSeg->uCoord0 = curU;
		wallSeg->wallX0 = x0pixel;
		wallSeg->wallX1 = x1pixel;
		wallSeg->x0View = x0;

		fixed16_16 slope, den;
		s32 orient;
		if (RMath::abs(dx) > RMath::abs(dz))
		{
			slope = div16(dz, dx);
			den = dx;
			orient = WORIENT_DZ_DX;
		}
		else
		{
			slope = div16(dx, dz);
			den = dz;
			orient = WORIENT_DX_DZ;
		}

		wallSeg->slope  = slope;
		wallSeg->uScale = div16(texelLenRem, den);
		wallSeg->orient = orient;
		/*if (x0pixel == x1pixel)
		{
			wallSeg->slope = 0;
		}*/

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
			bool insideWindow = ((srcSeg->z0 >= s_windowMinZ || srcSeg->z1 >= s_windowMinZ) && srcSeg->wallX0 <= s_windowMaxX && srcSeg->wallX1 >= s_windowMinX);
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

					fixed16_16 newMinZ = min(newV0->z, newV1->z);
					fixed16_16 newMaxZ = max(newV0->z, newV1->z);
					fixed16_16 outMinZ = min(outV0->z, outV1->z);
					fixed16_16 outMaxZ = max(outV0->z, outV1->z);
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
							if ((segmentCrossesLine(outV0->x, outV0->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) != 0 ||	// (outV0, 0) does NOT cross (newV0, newV1)
								 segmentCrossesLine(outV1->x, outV1->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) != 0) &&	// (outV1, 0) does NOT cross (newV0, newV1)
								(segmentCrossesLine(newV0->x, newV0->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) == 0 ||	// (newV0, 0) crosses (outV0, outV1)
								 segmentCrossesLine(newV1->x, newV1->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) == 0))	// (newV1, 0) crosses (outV0, outV1)
							{
								side = BACK;
							}
						}
						else  // newV0->z >= outV0->z
						{
							side = BACK;
							if ((segmentCrossesLine(newV0->x, newV0->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) != 0 ||	// (newV0, 0) does NOT cross (outV0, outV1)
								 segmentCrossesLine(newV1->x, newV1->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) != 0) &&	// (newV1, 0) does NOT cross (outV0, outV1)
								(segmentCrossesLine(outV0->x, outV0->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) == 0 ||	// (outV0, 0) crosses (newV0, newV1)
								 segmentCrossesLine(outV1->x, outV1->z, 0, 0, newV0->x, newV0->z, newV1->x, newV1->z) == 0))	// (outV1, 0) crosses (newV0, newV1)
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

	void wall_drawSolid(RWallSegment* wallSegment)
	{
		RWall* srcWall = wallSegment->srcWall;
		RSector* sector = srcWall->sector;
		TextureFrame* texture = srcWall->midTex;
		if (!texture) { return; }

		fixed16_16 ceilingHeight = sector->ceilingHeight;
		fixed16_16 floorHeight = sector->floorHeight;

		fixed16_16 ceilEyeRel  = ceilingHeight - s_eyeHeight;
		fixed16_16 floorEyeRel = floorHeight   - s_eyeHeight;

		fixed16_16 z0 = wallSegment->z0;
		fixed16_16 z1 = wallSegment->z1;

		fixed16_16 y0C, y0F, y1C, y1F;
		if (!s_enableHighPrecision)
		{
			// Original DOS code
			y0C = div16(mul16(ceilEyeRel,  s_focalLenAspect), z0) + s_halfHeight;
			y0F = div16(mul16(floorEyeRel, s_focalLenAspect), z0) + s_halfHeight;
			y1C = div16(mul16(ceilEyeRel,  s_focalLenAspect), z1) + s_halfHeight;
			y1F = div16(mul16(floorEyeRel, s_focalLenAspect), z1) + s_halfHeight;
		}
		else
		{
			// Improved code that better avoids overflow, as seen in the MAC version.
			y0C = fusedMulDiv(ceilEyeRel,  s_focalLenAspect, z0) + s_halfHeight;
			y0F = fusedMulDiv(floorEyeRel, s_focalLenAspect, z0) + s_halfHeight;
			y1C = fusedMulDiv(ceilEyeRel,  s_focalLenAspect, z1) + s_halfHeight;
			y1F = fusedMulDiv(floorEyeRel, s_focalLenAspect, z1) + s_halfHeight;
		}

		s32 y0C_pixel = round16(y0C);
		s32 y1C_pixel = round16(y1C);

		s32 y0F_pixel = round16(y0F);
		s32 y1F_pixel = round16(y1F);

		s32 x = wallSegment->wallX0;
		s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;
		fixed16_16 numerator = solveForZ_Numerator(wallSegment);

		// For some reason we only early-out if the ceiling is below the view.
		if (y0C_pixel > s_windowMaxY && y1C_pixel > s_windowMaxY)
		{
			fixed16_16 yMax = intToFixed16(s_windowMaxY + 1);
			flat_addEdges(length, x, 0, yMax, 0, yMax);

			for (s32 i = 0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, numerator);
				s_columnTop[x] = s_windowMaxY;
			}

			srcWall->visible = 0;
			//srcWall->y1 = -1;
			return;
		}

		s_texHeightMask = texture ? texture->height - 1 : 0;
		TextureFrame* signTex = srcWall->signTex;
		fixed16_16 signU0 = 0, signU1 = 0;
		ColumnFunction signFullbright = nullptr, signLit = nullptr;
		if (signTex)
		{
			// Compute the U texel range, the overlay is only drawn within this range.
			signU0 = srcWall->signUOffset;
			signU1 = signU0 + intToFixed16(signTex->width);

			// Determine the column functions based on texture opacity and flags.
			// In the original DOS code, the sign column functions are different but only because they do not apply the texture height mask
			// per pixel. I decided to keep it simple, removing the extra binary AND per pixel is not worth adding 4 extra functions that are
			// mostly redundant.
			if (signTex->opacity == OPACITY_TRANS)
			{
				signFullbright = s_columnFunc[COLFUNC_FULLBRIGHT_TRANS];
				signLit = s_columnFunc[(srcWall->flags1 & WF1_ILLUM_SIGN) ? COLFUNC_FULLBRIGHT_TRANS : COLFUNC_LIT_TRANS];
			}
			else
			{
				signFullbright = s_columnFunc[COLFUNC_FULLBRIGHT];
				signLit = s_columnFunc[(srcWall->flags1 & WF1_ILLUM_SIGN) ? COLFUNC_FULLBRIGHT : COLFUNC_LIT];
			}
		}

		fixed16_16 wallDeltaX = intToFixed16(wallSegment->wallX1_raw - wallSegment->wallX0_raw);
		fixed16_16 dYdXtop = 0, dYdXbot = 0;
		if (wallDeltaX != 0)
		{
			dYdXtop = div16(y1C - y0C, wallDeltaX);
			dYdXbot = div16(y1F - y0F, wallDeltaX);
		}

		fixed16_16 clippedXDelta = intToFixed16(wallSegment->wallX0 - wallSegment->wallX0_raw);
		if (clippedXDelta != 0)
		{
			y0C += mul16(dYdXtop, clippedXDelta);
			y0F += mul16(dYdXbot, clippedXDelta);
		}
		flat_addEdges(length, wallSegment->wallX0, dYdXbot, y0F, dYdXtop, y0C);

		const s32 texWidth = texture ? texture->width : 0;
		const bool flipHorz = (srcWall->flags1 & WF1_FLIP_HORIZ) != 0;
				
		for (s32 i = 0; i < length; i++, x++)
		{
			s32 top = round16(y0C);
			s32 bot = round16(y0F);
			s_columnBot[x] = bot + 1;
			s_columnTop[x] = top - 1;

			top = max(top, s_windowTop[x]);
			bot = min(bot, s_windowBot[x]);
			s_yPixelCount = bot - top + 1;

			fixed16_16 dxView = 0;
			fixed16_16 z = solveForZ(wallSegment, x, numerator, &dxView);
			s_depth1d[x] = z;

			fixed16_16 uScale = wallSegment->uScale;
			fixed16_16 uCoord0 = wallSegment->uCoord0 + srcWall->midUOffset;
			fixed16_16 uCoord = uCoord0 + ((wallSegment->orient == WORIENT_DZ_DX) ? mul16(dxView, uScale) : mul16(z - z0, uScale));

			if (s_yPixelCount > 0)
			{
				// texture wrapping, assumes texWidth is a power of 2.
				s32 texelU = floor16(uCoord) & (texWidth - 1);
				// flip texture coordinate if flag set.
				if (flipHorz) { texelU = texWidth - texelU - 1; }

				// Calculate the vertical texture coordinate start and increment.
				fixed16_16 wallHeightPixels = y0F - y0C + ONE_16;
				fixed16_16 wallHeightTexels = srcWall->midTexelHeight;

				// s_vCoordStep = tex coord "v" step per y pixel step -> dVdY;
				s_vCoordStep = div16(wallHeightTexels, wallHeightPixels);

				// texel offset from the actual fixed point y position and the truncated y position.
				fixed16_16 vPixelOffset = y0F - intToFixed16(bot) + HALF_16;

				// scale the texel offset based on the v coord step.
				// the result is the sub-texel offset
				fixed16_16 v0 = mul16(s_vCoordStep, vPixelOffset);
				s_vCoordFixed = v0 + srcWall->midVOffset;

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
					fixed16_16 signYBase = y0F + div16(srcWall->signVOffset, s_vCoordStep);
					s32 y0 = max(floor16(signYBase - div16(intToFixed16(signTex->height), s_vCoordStep) + ONE_16 + HALF_16), top);
					s32 y1 = min(floor16(signYBase + HALF_16), bot);
					s_yPixelCount = y1 - y0 + 1;

					if (s_yPixelCount > 0)
					{
						s_vCoordFixed = mul16(signYBase - intToFixed16(y1) + HALF_16, s_vCoordStep);
						s_columnOut = &s_display[y0*s_width + x];
						texelU = floor16(uCoord - signU0);
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

		//srcWall->y1 = -1;
	}

	void wall_drawTransparent(RWallSegment* wallSegment, EdgePair* edge)
	{
		RWall* srcWall = wallSegment->srcWall;
		RSector* sector = srcWall->sector;
		TextureFrame* texture = srcWall->midTex;

		fixed16_16 z0 = wallSegment->z0;
		fixed16_16 yC0 = edge->yCeil0;
		fixed16_16 yF0 = edge->yFloor0;
		fixed16_16 uScale = wallSegment->uScale;
		fixed16_16 uCoord0 = wallSegment->uCoord0 + srcWall->midUOffset;
		s32 lengthInPixels = edge->lengthInPixels;

		s_texHeightMask = texture->height - 1;
		s32 flipHorz = (srcWall->flags1 & WF1_FLIP_HORIZ) ? -1 : 0;

		fixed16_16 ceil_dYdX = edge->dyCeil_dx;
		fixed16_16 floor_dYdX = edge->dyFloor_dx;
		fixed16_16 num = solveForZ_Numerator(wallSegment);

		for (s32 i = 0, x = edge->x0; i < lengthInPixels; i++, x++)
		{
			s32 top = s_windowTop[x];
			s32 bot = s_windowBot[x];

			s32 yC_pixel = max(round16(yC0), top);
			s32 yF_pixel = min(round16(yF0), bot);

			s_yPixelCount = yF_pixel - yC_pixel + 1;
			if (s_yPixelCount > 0)
			{
				fixed16_16 dxView;
				fixed16_16 z = solveForZ(wallSegment, x, num, &dxView);
				fixed16_16 uCoord = uCoord0 + ((wallSegment->orient == WORIENT_DZ_DX) ? mul16(dxView, uScale) : mul16(z - z0, uScale));

				s32 widthMask = texture->width - 1;
				s32 texelU = floor16(uCoord) & widthMask;
				if (flipHorz)
				{
					texelU = widthMask - texelU;
				}

				s_texImage = &texture->image[texelU << texture->logSizeY];
				s_vCoordStep  = div16(srcWall->midTexelHeight, yF0 - yC0 + ONE_16);
				s_vCoordFixed = mul16(yF0 - intToFixed16(yF_pixel) + HALF_16, s_vCoordStep) + srcWall->midVOffset;

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

		fixed16_16 z0 = wallSegment->z0;
		fixed16_16 z1 = wallSegment->z1;
		u32 flags1 = sector->flags1;
		u32 nextFlags1 = nextSector->flags1;

		fixed16_16 cProj0, cProj1;
		if ((flags1 & SEC_FLAGS1_EXTERIOR) && (nextFlags1 & SEC_FLAGS1_EXT_ADJ))  // ceiling
		{
			cProj0 = cProj1 = intToFixed16(s_windowMinY);
		}
		else
		{
			fixed16_16 ceilRel = sector->ceilingHeight - s_eyeHeight;
			if (!s_enableHighPrecision)
			{
				// Original DOS code
				cProj0 = div16(mul16(ceilRel, s_focalLenAspect), z0) + s_halfHeight;
				cProj1 = div16(mul16(ceilRel, s_focalLenAspect), z1) + s_halfHeight;
			}
			else
			{
				// Improved code that better avoids overflow, as seen in the MAC version.
				cProj0 = fusedMulDiv(ceilRel, s_focalLenAspect, z0) + s_halfHeight;
				cProj1 = fusedMulDiv(ceilRel, s_focalLenAspect, z1) + s_halfHeight;
			}
		}

		s32 c0pixel = round16(cProj0);
		s32 c1pixel = round16(cProj1);
		if (c0pixel > s_windowMaxY && c1pixel > s_windowMaxY)
		{
			s32 x = wallSegment->wallX0;
			s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;
			flat_addEdges(length, x, 0, intToFixed16(s_windowMaxY + 1), 0, intToFixed16(s_windowMaxY + 1));
			const fixed16_16 numerator = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, numerator);
			}

			srcWall->visible = 0;
			//srcWall->drawFlags = -1;
			return;
		}

		fixed16_16 fProj0, fProj1;
		if ((sector->flags1 & SEC_FLAGS1_PIT) && (nextFlags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))	// floor
		{
			fProj0 = fProj1 = intToFixed16(s_windowMaxY);
		}
		else
		{
			fixed16_16 floorRel = sector->floorHeight - s_eyeHeight;
			if (!s_enableHighPrecision)
			{
				// Original DOS code
				fProj0 = div16(mul16(floorRel, s_focalLenAspect), z0) + s_halfHeight;
				fProj1 = div16(mul16(floorRel, s_focalLenAspect), z1) + s_halfHeight;
			}
			else
			{
				// Improved code that better avoids overflow, as seen in the MAC version.
				fProj0 = fusedMulDiv(floorRel, s_focalLenAspect, z0) + s_halfHeight;
				fProj1 = fusedMulDiv(floorRel, s_focalLenAspect, z1) + s_halfHeight;
			}
		}

		s32 f0pixel = round16(fProj0);
		s32 f1pixel = round16(fProj1);
		if (f0pixel < s_windowMinY && f1pixel < s_windowMinY)
		{
			s32 x = wallSegment->wallX0;
			s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;
			flat_addEdges(length, x, 0, intToFixed16(s_windowMinY - 1), 0, intToFixed16(s_windowMinY - 1));

			const fixed16_16 numerator = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, numerator);
				s_columnBot[x] = s_windowMinY;
			}
			srcWall->visible = 0;
			//srcWall->drawFlags = -1;
			return;
		}

		fixed16_16 xStartOffset = intToFixed16(wallSegment->wallX0 - wallSegment->wallX0_raw);
		s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;

		fixed16_16 numerator = solveForZ_Numerator(wallSegment);
		fixed16_16 lengthRaw = intToFixed16(wallSegment->wallX1_raw - wallSegment->wallX0_raw);
		fixed16_16 dydxCeil = 0;
		fixed16_16 dydxFloor = 0;
		if (lengthRaw != 0)
		{
			dydxCeil = div16(cProj1 - cProj0, lengthRaw);
			dydxFloor = div16(fProj1 - fProj0, lengthRaw);
		}
		fixed16_16 y0 = cProj0;
		fixed16_16 y1 = fProj0;
		s32 x = wallSegment->wallX0;
		if (xStartOffset != 0)
		{
			y0 = mul16(dydxCeil, xStartOffset) + cProj0;
			y1 = mul16(dydxFloor, xStartOffset) + fProj0;
		}

		flat_addEdges(length, x, dydxFloor, y1, dydxCeil, y0);
		fixed16_16 nextFloor = nextSector->floorHeight;
		fixed16_16 nextCeil  = nextSector->ceilingHeight;
		// There is an opening in this wall to the next sector.
		if (nextFloor > nextCeil)
		{
			wall_addAdjoinSegment(length, x, dydxFloor, y1, dydxCeil, y0, wallSegment);
		}
		if (length != 0)
		{
			for (s32 i = 0; i < length; i++, x++)
			{
				s32 y0_pixel = round16(y0);
				s32 y1_pixel = round16(y1);
				s_columnTop[x] = y0_pixel - 1;
				s_columnBot[x] = y1_pixel + 1;

				s_depth1d[x] = solveForZ(wallSegment, x, numerator);
				y0 += dydxCeil;
				y1 += dydxFloor;
			}
		}

		//srcWall->drawFlags = -1;
	}

	void wall_drawBottom(RWallSegment* wallSegment)
	{
		RWall* wall = wallSegment->srcWall;
		RSector* sector = wall->sector;
		RSector* nextSector = wall->nextSector;
		TextureFrame* tex = wall->botTex;

		fixed16_16 z0 = wallSegment->z0;
		fixed16_16 z1 = wallSegment->z1;

		fixed16_16 cProj0, cProj1;
		if ((sector->flags1 & SEC_FLAGS1_EXTERIOR) && (nextSector->flags1 & SEC_FLAGS1_EXT_ADJ))
		{
			cProj1 = intToFixed16(s_windowMinY);
			cProj0 = cProj1;
		}
		else
		{
			fixed16_16 ceilRel = sector->ceilingHeight - s_eyeHeight;
			if (!s_enableHighPrecision)
			{
				// Original DOS code
				cProj0 = div16(mul16(ceilRel, s_focalLenAspect), z0) + s_halfHeight;
				cProj1 = div16(mul16(ceilRel, s_focalLenAspect), z1) + s_halfHeight;
			}
			else
			{
				// Improved code that better avoids overflow, as seen in the MAC version.
				cProj0 = fusedMulDiv(ceilRel, s_focalLenAspect, z0) + s_halfHeight;
				cProj1 = fusedMulDiv(ceilRel, s_focalLenAspect, z1) + s_halfHeight;
			}
		}

		s32 cy0 = round16(cProj0);
		s32 cy1 = round16(cProj1);
		if (cy0 > s_windowMaxY && cy1 >= s_windowMaxY)
		{
			wall->visible = 0;
			s32 x = wallSegment->wallX0;
			s32 length = wallSegment->wallX1 - x + 1;

			flat_addEdges(length, x, 0, s_windowMaxY + 1, 0, s_windowMaxY + 1);

			fixed16_16 num = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, num);
				s_columnTop[x] = s_windowMaxY;
			}
			//wall->y1 = -1;
			return;
		}

		fixed16_16 floorRel = sector->floorHeight - s_eyeHeight;
		fixed16_16 fProj0, fProj1;
		if (!s_enableHighPrecision)
		{
			// Original DOS code
			fProj0 = div16(mul16(floorRel, s_focalLenAspect), z0) + s_halfHeight;
			fProj1 = div16(mul16(floorRel, s_focalLenAspect), z1) + s_halfHeight;
		}
		else
		{
			// Improved code that better avoids overflow, as seen in the MAC version.
			fProj0 = fusedMulDiv(floorRel, s_focalLenAspect, z0) + s_halfHeight;
			fProj1 = fusedMulDiv(floorRel, s_focalLenAspect, z1) + s_halfHeight;
		}
		s32 fy0 = round16(fProj0);
		s32 fy1 = round16(fProj1);
		if (fy0 < s_windowMinY && fy1 < s_windowMinY)
		{
			// Wall is above the top of the screen.
			wall->visible = 0;
			s32 x = wallSegment->wallX0;
			s32 length = wallSegment->wallX1 - x + 1;

			flat_addEdges(length, x, 0, intToFixed16(s_windowMinY - 1), 0, intToFixed16(s_windowMinY - 1));

			fixed16_16 num = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, num);
				s_columnBot[x] = s_windowMinY;
			}
			//wall->y1 = -1;
			return;
		}

		fixed16_16 floorRelNext = nextSector->floorHeight - s_eyeHeight;
		fixed16_16 fNextProj0, fNextProj1;
		if (!s_enableHighPrecision)
		{
			// Original DOS code
			fNextProj0 = div16(mul16(floorRelNext, s_focalLenAspect), z0) + s_halfHeight;
			fNextProj1 = div16(mul16(floorRelNext, s_focalLenAspect), z1) + s_halfHeight;
		}
		else
		{
			// Improved code that better avoids overflow, as seen in the MAC version.
			fNextProj0 = fusedMulDiv(floorRelNext, s_focalLenAspect, z0) + s_halfHeight;
			fNextProj1 = fusedMulDiv(floorRelNext, s_focalLenAspect, z1) + s_halfHeight;
		}
		s32 xOffset = wallSegment->wallX0 - wallSegment->wallX0_raw;
		s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;
		s32 lengthRaw = wallSegment->wallX1_raw - wallSegment->wallX0_raw;
		fixed16_16 lengthRawFixed = intToFixed16(lengthRaw);
		fixed16_16 xOffsetFixed = intToFixed16(xOffset);

		fixed16_16 floorNext_dYdX = 0;
		fixed16_16 floor_dYdX = 0;
		fixed16_16 ceil_dYdX = 0;
		if (lengthRawFixed != 0)
		{
			floorNext_dYdX = div16(fNextProj1 - fNextProj0, lengthRawFixed);
			floor_dYdX = div16(fProj1 - fProj0, lengthRawFixed);
			ceil_dYdX = div16(cProj1 - cProj0, lengthRawFixed);
		}
		if (xOffsetFixed)
		{
			fNextProj0 += mul16(floorNext_dYdX, xOffsetFixed);
			fProj0 += mul16(floor_dYdX, xOffsetFixed);
			cProj0 += mul16(ceil_dYdX, xOffsetFixed);
		}

		fixed16_16 yTop = fNextProj0;
		fixed16_16 yC = cProj0;
		fixed16_16 yBot = fProj0;
		s32 x = wallSegment->wallX0;
		flat_addEdges(length, wallSegment->wallX0, floor_dYdX, fProj0, ceil_dYdX, cProj0);

		s32 yTop0 = round16(fNextProj0);
		s32 yTop1 = round16(fNextProj1);
		if ((yTop0 > s_windowMinY || yTop1 > s_windowMinY) && sector->ceilingHeight < nextSector->floorHeight)
		{
			wall_addAdjoinSegment(length, wallSegment->wallX0, floorNext_dYdX, fNextProj0, ceil_dYdX, cProj0, wallSegment);
		}

		if (yTop0 > s_windowMaxY && yTop1 > s_windowMaxY)
		{
			s32 bot = s_windowMaxY + 1;
			fixed16_16 num = solveForZ_Numerator(wallSegment);
			for (s32 i = 0; i < length; i++, x++, yC += ceil_dYdX)
			{
				s32 yC_pixel = min(round16(yC), s_windowBot[x]);
				s_columnTop[x] = yC_pixel - 1;
				s_columnBot[x] = bot;
				s_depth1d[x] = solveForZ(wallSegment, x, num);
			}
			//wall->y1 = -1;
			return;
		}

		fixed16_16 u0 = wallSegment->uCoord0;
		fixed16_16 num = solveForZ_Numerator(wallSegment);
		s_texHeightMask = tex->height - 1;
		s32 flipHorz = (wall->flags1 & WF1_FLIP_HORIZ) ? -1 : 0;
		s32 illumSign = (wall->flags1 & WF1_ILLUM_SIGN) ? -1 : 0;

		TextureFrame* signTex = wall->signTex;
		fixed16_16 signU0 = 0, signU1 = 0;
		ColumnFunction signFullbright = nullptr, signLit = nullptr;
		if (signTex)
		{
			// Compute the U texel range, the overlay is only drawn within this range.
			signU0 = wall->signUOffset;
			signU1 = signU0 + intToFixed16(signTex->width);

			// Determine the column functions based on texture opacity and flags.
			// In the original DOS code, the sign column functions are different but only because they do not apply the texture height mask
			// per pixel. I decided to keep it simple, removing the extra binary AND per pixel is not worth adding 4 extra functions that are
			// mostly redundant.
			if (signTex->opacity == OPACITY_TRANS)
			{
				signFullbright = s_columnFunc[COLFUNC_FULLBRIGHT_TRANS];
				signLit = s_columnFunc[illumSign ? COLFUNC_FULLBRIGHT_TRANS : COLFUNC_LIT_TRANS];
			}
			else
			{
				signFullbright = s_columnFunc[COLFUNC_FULLBRIGHT];
				signLit = s_columnFunc[illumSign ? COLFUNC_FULLBRIGHT : COLFUNC_LIT];
			}
		}

		if (length > 0)
		{
			for (s32 i = 0; i < length; i++, x++)
			{
				s32 yTop_pixel = round16(yTop);
				s32 yC_pixel   = round16(yC);
				s32 yBot_pixel = round16(yBot);

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
				fixed16_16 dxView;
				fixed16_16 z = solveForZ(wallSegment, x, num, &dxView);
				fixed16_16 uCoord;
				if (wallSegment->orient == WORIENT_DZ_DX)
				{
					uCoord = u0 + mul16(dxView, wallSegment->uScale) + wall->botUOffset;
				}
				else
				{
					fixed16_16 dz = z - z0;
					uCoord = u0 + mul16(dz, wallSegment->uScale) + wall->botUOffset;
				}
				s_depth1d[x] = z;
				if (s_yPixelCount > 0)
				{
					s32 widthMask = tex->width - 1;
					s32 texelU = floor16(uCoord) & widthMask;
					if (flipHorz != 0)
					{
						texelU = widthMask - texelU;
					}

					s_vCoordStep = div16(wall->botTexelHeight, yBot - yTop + ONE_16);
					fixed16_16 v0 = mul16(yBot - intToFixed16(yBot_pixel) + HALF_16, s_vCoordStep);
					s_vCoordFixed = v0 + wall->botVOffset;
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
						fixed16_16 signYBase = yBot + div16(wall->signVOffset, s_vCoordStep);
						s32 y0 = max(floor16(signYBase - div16(intToFixed16(signTex->height), s_vCoordStep) + ONE_16 + HALF_16), yTop_pixel);
						s32 y1 = min(floor16(signYBase + HALF_16), yBot_pixel);
						s_yPixelCount = y1 - y0 + 1;

						if (s_yPixelCount > 0)
						{
							s_vCoordFixed = mul16(signYBase - intToFixed16(y1) + HALF_16, s_vCoordStep);
							s_columnOut = &s_display[y0*s_width + x];
							texelU = floor16(uCoord - signU0);
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
		//wall->y1 = -1;
	}

	void wall_drawTop(RWallSegment* wallSegment)
	{
		RWall* srcWall = wallSegment->srcWall;
		RSector* sector = srcWall->sector;
		RSector* next = srcWall->nextSector;
		TextureFrame* texture = srcWall->topTex;
		fixed16_16 z0 = wallSegment->z0;
		fixed16_16 z1 = wallSegment->z1;
		fixed16_16 num = solveForZ_Numerator(wallSegment);
				
		s32 x0 = wallSegment->wallX0;
		s32 lengthInPixels = wallSegment->wallX1 - wallSegment->wallX0 + 1;

		fixed16_16 ceilRel = sector->ceilingHeight - s_eyeHeight;
		fixed16_16 yC0, yC1;
		if (!s_enableHighPrecision)
		{
			// Original DOS code
			yC0 = div16(mul16(ceilRel, s_focalLenAspect), z0) + s_halfHeight;
			yC1 = div16(mul16(ceilRel, s_focalLenAspect), z1) + s_halfHeight;
		}
		else
		{
			// Improved code that better avoids overflow, as seen in the MAC version.
			yC0 = fusedMulDiv(ceilRel, s_focalLenAspect, z0) + s_halfHeight;
			yC1 = fusedMulDiv(ceilRel, s_focalLenAspect, z1) + s_halfHeight;
		}
		s32 yC0_pixel = round16(yC0);
		s32 yC1_pixel = round16(yC1);

		s_texHeightMask = texture->height - 1;

		if (yC0_pixel > s_windowMaxY && yC1_pixel > s_windowMaxY)
		{
			srcWall->visible = 0;
			for (s32 i = 0; i < lengthInPixels; i++) { s_columnTop[x0 + i] = s_windowMaxY; }
			flat_addEdges(lengthInPixels, x0, 0, intToFixed16(s_windowMaxY + 1), 0, intToFixed16(s_windowMaxY + 1));
			for (s32 i = 0, x = x0; i < lengthInPixels; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, num);
			}
			//srcWall->y1 = -1;
			return;
		}

		fixed16_16 yF0, yF1;
		if ((sector->flags1 & SEC_FLAGS1_PIT) && (next->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
		{
			yF0 = yF1 = intToFixed16(s_windowMaxY);
		}
		else
		{
			fixed16_16 floorRel = sector->floorHeight - s_eyeHeight;
			if (!s_enableHighPrecision)
			{
				// Original DOS code
				yF0 = div16(mul16(floorRel, s_focalLenAspect), z0) + s_halfHeight;
				yF1 = div16(mul16(floorRel, s_focalLenAspect), z1) + s_halfHeight;
			}
			else
			{
				// Improved code that better avoids overflow, as seen in the MAC version.
				yF0 = fusedMulDiv(floorRel, s_focalLenAspect, z0) + s_halfHeight;
				yF1 = fusedMulDiv(floorRel, s_focalLenAspect, z1) + s_halfHeight;
			}
		}
		s32 yF0_pixel = round16(yF0);
		s32 yF1_pixel = round16(yF1);
		if (yF0_pixel < s_windowMinY && yF1_pixel < s_windowMinY)
		{
			srcWall->visible = 0;
			for (s32 i = 0; i < lengthInPixels; i++) { s_columnBot[x0 + i] = s_windowMinY; }
			flat_addEdges(lengthInPixels, x0, 0, intToFixed16(s_windowMinY - 1), 0, intToFixed16(s_windowMinY - 1));
			for (s32 i = 0, x = x0; i < lengthInPixels; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, num);
			}
			//srcWall->y1 = -1;
			return;
		}

		fixed16_16 next_ceilRel = next->ceilingHeight - s_eyeHeight;
		fixed16_16 next_yC0, next_yC1;
		if (!s_enableHighPrecision)
		{
			// Original DOS code
			next_yC0 = div16(mul16(next_ceilRel, s_focalLenAspect), z0) + s_halfHeight;
			next_yC1 = div16(mul16(next_ceilRel, s_focalLenAspect), z1) + s_halfHeight;
		}
		else
		{
			// Improved code that better avoids overflow, as seen in the MAC version.
			next_yC0 = fusedMulDiv(next_ceilRel, s_focalLenAspect, z0) + s_halfHeight;
			next_yC1 = fusedMulDiv(next_ceilRel, s_focalLenAspect, z1) + s_halfHeight;
		}
		fixed16_16 xOffset = intToFixed16(wallSegment->wallX0 - wallSegment->wallX0_raw);
		fixed16_16 length = intToFixed16(wallSegment->wallX1_raw - wallSegment->wallX0_raw);
		fixed16_16 ceil_dYdX = 0;
		fixed16_16 next_ceil_dYdX = 0;
		fixed16_16 floor_dYdX = 0;
		if (length)
		{
			ceil_dYdX = div16(yC1 - yC0, length);
			next_ceil_dYdX = div16(next_yC1 - next_yC0, length);
			floor_dYdX = div16(yF1 - yF0, length);
		}
		if (xOffset)
		{
			yC0 += mul16(ceil_dYdX, xOffset);
			next_yC0 += mul16(next_ceil_dYdX, xOffset);
			yF0 += mul16(floor_dYdX, xOffset);
		}

		flat_addEdges(lengthInPixels, x0, floor_dYdX, yF0, ceil_dYdX, yC0);
		s32 next_yC0_pixel = round16(next_yC0);
		s32 next_yC1_pixel = round16(next_yC1);
		if ((next_yC0_pixel < s_windowMaxY || next_yC1_pixel < s_windowMaxY) && (sector->floorHeight > next->ceilingHeight))
		{
			wall_addAdjoinSegment(lengthInPixels, x0, floor_dYdX, yF0, next_ceil_dYdX, next_yC0, wallSegment);
		}

		if (next_yC0_pixel < s_windowMinY && next_yC1_pixel < s_windowMinY)
		{
			for (s32 i = 0; i < lengthInPixels; i++) { s_columnTop[x0 + i] = s_windowMinY - 1; }
			for (s32 i = 0, x = x0; i < lengthInPixels; i++, x++)
			{
				yF0_pixel = round16(yF0);
				if (yF0_pixel > s_windowBot[x])
				{
					yF0_pixel = s_windowBot[x];
				}

				s_columnBot[x] = yF0_pixel + 1;
				s_depth1d[x] = solveForZ(wallSegment, x, num);
				yF0 += floor_dYdX;
			}
			//srcWall->y1 = -1;
			return;
		}

		fixed16_16 uCoord0 = wallSegment->uCoord0;
		s32 flipHorz = (srcWall->flags1 & WF1_FLIP_HORIZ) ? -1 : 0;
		s32 illumSign = (srcWall->flags1 & WF1_ILLUM_SIGN) ? -1 : 0;

		TextureFrame* signTex = srcWall->signTex;
		fixed16_16 signU0 = 0, signU1 = 0;
		ColumnFunction signFullbright = nullptr, signLit = nullptr;
		if (signTex)
		{
			// Compute the U texel range, the overlay is only drawn within this range.
			signU0 = srcWall->signUOffset;
			signU1 = signU0 + intToFixed16(signTex->width);

			// Determine the column functions based on texture opacity and flags.
			// In the original DOS code, the sign column functions are different but only because they do not apply the texture height mask
			// per pixel. I decided to keep it simple, removing the extra binary AND per pixel is not worth adding 4 extra functions that are
			// mostly redundant.
			if (signTex->opacity == OPACITY_TRANS)
			{
				signFullbright = s_columnFunc[COLFUNC_FULLBRIGHT_TRANS];
				signLit = s_columnFunc[illumSign ? COLFUNC_FULLBRIGHT_TRANS : COLFUNC_LIT_TRANS];
			}
			else
			{
				signFullbright = s_columnFunc[COLFUNC_FULLBRIGHT];
				signLit = s_columnFunc[illumSign ? COLFUNC_FULLBRIGHT : COLFUNC_LIT];
			}
		}

		for (s32 i = 0, x = x0; i < lengthInPixels; i++, x++)
		{
			yC0_pixel = round16(yC0);
			next_yC0_pixel = round16(next_yC0);
			yF0_pixel = round16(yF0);

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

			fixed16_16 dxView;
			fixed16_16 z = solveForZ(wallSegment, x, num, &dxView);

			fixed16_16 uScale = wallSegment->uScale;
			fixed16_16 uCoord0 = wallSegment->uCoord0 + srcWall->topUOffset;
			fixed16_16 uCoord = uCoord0 + ((wallSegment->orient == WORIENT_DZ_DX) ? mul16(dxView, uScale) : mul16(z - z0, uScale));

			s_depth1d[x] = z;
			if (s_yPixelCount > 0)
			{
				s32 widthMask = texture->width - 1;
				s32 texelU = floor16(uCoord) & widthMask;
				if (flipHorz)
				{
					texelU = widthMask - texelU;
				}
				assert(texelU >= 0 && texelU <= widthMask);

				s_vCoordStep = div16(srcWall->topTexelHeight, next_yC0 - yC0 + ONE_16);
				s_vCoordFixed = srcWall->topVOffset + mul16(next_yC0 - intToFixed16(next_yC0_pixel) + HALF_16, s_vCoordStep);
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
					fixed16_16 signYBase = next_yC0 + div16(srcWall->signVOffset, s_vCoordStep);
					s32 y0 = max(floor16(signYBase - div16(intToFixed16(signTex->height), s_vCoordStep) + ONE_16 + HALF_16), yC0_pixel);
					s32 y1 = min(floor16(signYBase + HALF_16), next_yC0_pixel);
					s_yPixelCount = y1 - y0 + 1;

					if (s_yPixelCount > 0)
					{
						s_vCoordFixed = mul16(signYBase - intToFixed16(y1) + HALF_16, s_vCoordStep);
						s_columnOut = &s_display[y0*s_width + x];
						texelU = floor16(uCoord - signU0);
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
		
		//srcWall->y1 = -1;
	}

	void wall_drawTopAndBottom(RWallSegment* wallSegment)
	{
		RWall* srcWall = wallSegment->srcWall;
		RSector* sector = srcWall->sector;
		TextureFrame* topTex = srcWall->topTex;
		fixed16_16 z0 = wallSegment->z0;
		fixed16_16 z1 = wallSegment->z1;
		s32 x0 = wallSegment->wallX0;
		fixed16_16 xOffset   = intToFixed16(wallSegment->wallX0 - wallSegment->wallX0_raw);
		s32 length    =  wallSegment->wallX1 - wallSegment->wallX0 + 1;
		fixed16_16 lengthRaw = intToFixed16(wallSegment->wallX1_raw - wallSegment->wallX0_raw);

		fixed16_16 ceilRel = sector->ceilingHeight - s_eyeHeight;
		fixed16_16 cProj0, cProj1;
		if (!s_enableHighPrecision)
		{
			// Original DOS code
			cProj0 = div16(mul16(ceilRel, s_focalLenAspect), z0) + s_halfHeight;
			cProj1 = div16(mul16(ceilRel, s_focalLenAspect), z1) + s_halfHeight;
		}
		else
		{
			// Improved code that better avoids overflow, as seen in the MAC version.
			cProj0 = fusedMulDiv(ceilRel, s_focalLenAspect, z0) + s_halfHeight;
			cProj1 = fusedMulDiv(ceilRel, s_focalLenAspect, z1) + s_halfHeight;
		}
		s32 c0_pixel = round16(cProj0);
		s32 c1_pixel = round16(cProj1);
				
		if (c0_pixel > s_windowMaxY && c1_pixel > s_windowMaxY)
		{
			srcWall->visible = 0;
			for (s32 i = 0; i < length; i++) { s_columnTop[x0 + i] = s_windowMaxY; }

			flat_addEdges(length, x0, 0, intToFixed16(s_windowMaxY + 1), 0, intToFixed16(s_windowMaxY + 1));
			fixed16_16 num = solveForZ_Numerator(wallSegment);
			for (s32 i = 0, x = x0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, num);
			}
			// srcWall->y1 = -1;
			return;
		}

		fixed16_16 floorRel = sector->floorHeight - s_eyeHeight;
		fixed16_16 fProj0, fProj1;
		if (!s_enableHighPrecision)
		{
			// Original DOS code
			fProj0 = div16(mul16(floorRel, s_focalLenAspect), z0) + s_halfHeight;
			fProj1 = div16(mul16(floorRel, s_focalLenAspect), z1) + s_halfHeight;
		}
		else
		{
			// Improved code that better avoids overflow, as seen in the MAC version.
			fProj0 = fusedMulDiv(floorRel, s_focalLenAspect, z0) + s_halfHeight;
			fProj1 = fusedMulDiv(floorRel, s_focalLenAspect, z1) + s_halfHeight;
		}
		s32 f0_pixel = round16(fProj0);
		s32 f1_pixel = round16(fProj1);
		if (f0_pixel < s_windowMinY && f1_pixel < s_windowMinY)
		{
			srcWall->visible = 0;
			for (s32 i = 0; i < length; i++) { s_columnBot[x0 + i] = s_windowMinY; }

			flat_addEdges(length, x0, 0, intToFixed16(s_windowMinY - 1), 0, intToFixed16(s_windowMinY - 1));
			fixed16_16 num = solveForZ_Numerator(wallSegment);

			for (s32 i = 0, x = x0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, num);
			}
			// srcWall->y1 = -1;
			return;
		}

		RSector* nextSector = srcWall->nextSector;
		fixed16_16 next_ceilRel = nextSector->ceilingHeight - s_eyeHeight;
		fixed16_16 next_cProj0, next_cProj1;
		if (!s_enableHighPrecision)
		{
			// Original DOS code
			next_cProj0 = div16(mul16(next_ceilRel, s_focalLenAspect), z0) + s_halfHeight;
			next_cProj1 = div16(mul16(next_ceilRel, s_focalLenAspect), z1) + s_halfHeight;
		}
		else
		{
			// Improved code that better avoids overflow, as seen in the MAC version.
			next_cProj0 = fusedMulDiv(next_ceilRel, s_focalLenAspect, z0) + s_halfHeight;
			next_cProj1 = fusedMulDiv(next_ceilRel, s_focalLenAspect, z1) + s_halfHeight;
		}
		fixed16_16 ceil_dYdX = 0;
		fixed16_16 next_ceil_dYdX = 0;
		if (lengthRaw != 0)
		{
			ceil_dYdX = div16(cProj1 - cProj0, lengthRaw);
			next_ceil_dYdX = div16(next_cProj1 - next_cProj0, lengthRaw);
		}
		if (xOffset)
		{
			cProj0 += mul16(ceil_dYdX, xOffset);
			next_cProj0 += mul16(next_ceil_dYdX, xOffset);
		}

		fixed16_16 yC0 = cProj0;
		fixed16_16 yC1 = next_cProj0;
		
		s32 cn0_pixel = round16(next_cProj0);
		s32 cn1_pixel = round16(next_cProj1);
		if (cn0_pixel >= s_windowMinY || cn1_pixel >= s_windowMinY)
		{
			fixed16_16 u0 = wallSegment->uCoord0;
			fixed16_16 num = solveForZ_Numerator(wallSegment);
			s_texHeightMask = topTex->height - 1;
			s32 flipHorz = (srcWall->flags1 & WF1_FLIP_HORIZ) ? -1 : 0;

			for (s32 i = 0, x = x0; i < length; i++, x++)
			{
				s32 yC1_pixel = round16(yC1);
				s32 yC0_pixel = round16(yC0);
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
				fixed16_16 dxView;
				fixed16_16 z = solveForZ(wallSegment, x, num, &dxView);
				fixed16_16 u;
				if (wallSegment->orient == WORIENT_DZ_DX)
				{
					u = u0 + mul16(dxView, wallSegment->uScale) + srcWall->topUOffset;
				}
				else
				{
					fixed16_16 dz = z - z0;
					u = u0 + mul16(dz, wallSegment->uScale) + srcWall->topUOffset;
				}
				s_depth1d[x] = z;
				if (s_yPixelCount > 0)
				{
					s32 widthMask = topTex->width - 1;
					s32 texelU = floor16(u) & widthMask;
					if (flipHorz)
					{
						texelU = widthMask - texelU;
					}
					s_vCoordStep = div16(srcWall->topTexelHeight, yC1 - yC0 + ONE_16);
					fixed16_16 yOffset = yC1 - intToFixed16(yC1_pixel) + HALF_16;
					s_vCoordFixed = mul16(yOffset, s_vCoordStep) + srcWall->topVOffset;
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

		fixed16_16 next_floorRel = nextSector->floorHeight - s_eyeHeight;
		fixed16_16 next_fProj0, next_fProj1;
		if (!s_enableHighPrecision)
		{
			// Original DOS code
			next_fProj0 = div16(mul16(next_floorRel, s_focalLenAspect), z0) + s_halfHeight;
			next_fProj1 = div16(mul16(next_floorRel, s_focalLenAspect), z1) + s_halfHeight;
		}
		else
		{
			// Improved code that better avoids overflow, as seen in the MAC version.
			next_fProj0 = fusedMulDiv(next_floorRel, s_focalLenAspect, z0) + s_halfHeight;
			next_fProj1 = fusedMulDiv(next_floorRel, s_focalLenAspect, z1) + s_halfHeight;
		}

		fixed16_16 next_floor_dYdX = 0;
		fixed16_16 floor_dYdX = 0;
		if (lengthRaw > 0)
		{
			next_floor_dYdX = div16(next_fProj1 - next_fProj0, lengthRaw);
			floor_dYdX = div16(fProj1 - fProj0, lengthRaw);
		}
		if (xOffset)
		{
			next_fProj0 += mul16(next_floor_dYdX, xOffset);
			fProj0 += mul16(floor_dYdX, xOffset);
		}

		fixed16_16 yF0 = next_fProj0;
		fixed16_16 yF1 = fProj0;
		TextureFrame* botTex = srcWall->botTex;
		f0_pixel = round16(next_fProj0);
		f1_pixel = round16(next_fProj1);

		if (f0_pixel <= s_windowMaxY || f1_pixel <= s_windowMaxY)
		{
			fixed16_16 u0 = wallSegment->uCoord0;
			fixed16_16 num = solveForZ_Numerator(wallSegment);

			s_texHeightMask = botTex->height - 1;
			s32 flipHorz = (srcWall->flags1 & WF1_FLIP_HORIZ) ? -1 : 0;
			s32 illumSign = (srcWall->flags1 & WF1_ILLUM_SIGN) ? -1 : 0;

			TextureFrame* signTex = srcWall->signTex;
			fixed16_16 signU0 = 0, signU1 = 0;
			ColumnFunction signFullbright = nullptr, signLit = nullptr;
			if (signTex)
			{
				// Compute the U texel range, the overlay is only drawn within this range.
				signU0 = srcWall->signUOffset;
				signU1 = signU0 + intToFixed16(signTex->width);

				// Determine the column functions based on texture opacity and flags.
				// In the original DOS code, the sign column functions are different but only because they do not apply the texture height mask
				// per pixel. I decided to keep it simple, removing the extra binary AND per pixel is not worth adding 4 extra functions that are
				// mostly redundant.
				if (signTex->opacity == OPACITY_TRANS)
				{
					signFullbright = s_columnFunc[COLFUNC_FULLBRIGHT_TRANS];
					signLit = s_columnFunc[illumSign ? COLFUNC_FULLBRIGHT_TRANS : COLFUNC_LIT_TRANS];
				}
				else
				{
					signFullbright = s_columnFunc[COLFUNC_FULLBRIGHT];
					signLit = s_columnFunc[illumSign ? COLFUNC_FULLBRIGHT : COLFUNC_LIT];
				}
			}

			if (length > 0)
			{
				for (s32 i = 0, x = x0; i < length; i++, x++)
				{
					s32 yF0_pixel = round16(yF0);
					s32 yF1_pixel = round16(yF1);
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
					s_yPixelCount = yF1_pixel - yF0_pixel + 1;	// eax

					// Calculate perspective correct Z and U (texture coordinate).
					fixed16_16 dxView;
					fixed16_16 z = solveForZ(wallSegment, x, num, &dxView);
					fixed16_16 uCoord;
					if (wallSegment->orient == WORIENT_DZ_DX)
					{
						uCoord = u0 + mul16(dxView, wallSegment->uScale) + srcWall->botUOffset;
					}
					else
					{
						fixed16_16 dz = z - z0;
						uCoord = u0 + mul16(dz, wallSegment->uScale) + srcWall->botUOffset;
					}
					s_depth1d[x] = z;
					if (s_yPixelCount > 0)
					{
						s32 widthMask = botTex->width - 1;
						s32 texelU = floor16(uCoord) & widthMask;
						if (flipHorz)
						{
							texelU = widthMask - texelU;
						}
						s_vCoordStep = div16(srcWall->botTexelHeight, yF1 - yF0 + ONE_16);
						s_vCoordFixed = srcWall->botVOffset + mul16(yF1 - intToFixed16(yF1_pixel) + HALF_16, s_vCoordStep);
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
							fixed16_16 signYBase = yF1 + div16(srcWall->signVOffset, s_vCoordStep);
							s32 y0 = max(floor16(signYBase - div16(intToFixed16(signTex->height), s_vCoordStep) + ONE_16 + HALF_16), yF0_pixel);
							s32 y1 = min(floor16(signYBase + HALF_16), yF1_pixel);
							s_yPixelCount = y1 - y0 + 1;

							if (s_yPixelCount > 0)
							{
								s_vCoordFixed = mul16(signYBase - intToFixed16(y1) + HALF_16, s_vCoordStep);
								s_columnOut = &s_display[y0*s_width + x];
								texelU = floor16(uCoord - signU0);
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

		s32 next_f0_pixel = round16(next_fProj0);
		s32 next_f1_pixel = round16(next_fProj1);
		s32 next_c0_pixel = round16(next_cProj0);
		s32 next_c1_pixel = round16(next_cProj1);
		if ((next_f0_pixel <= s_windowMinY && next_f1_pixel <= s_windowMinY) || (next_c0_pixel >= s_windowMaxY && next_c1_pixel >= s_windowMaxY) || (nextSector->floorHeight <= nextSector->ceilingHeight))
		{
			// srcWall->y1 = -1;
			return;
		}

		wall_addAdjoinSegment(length, x0, next_floor_dYdX, next_fProj0 - ONE_16, next_ceil_dYdX, next_cProj0 + ONE_16, wallSegment);
		//srcWall->y1 = -1;
	}

	// Parts of the code inside 's_height == SKY_BASE_HEIGHT' are based on the original DOS exe.
	// Other parts of those same conditionals are modified to handle higher resolutions.
	void wall_drawSkyTop(RSector* sector)
	{
		if (s_wallMaxCeilY < s_windowMinY) { return; }
		TFE_ZONE("Draw Sky");

		TextureFrame* texture = sector->ceilTex;
		fixed16_16 heightScale;
		// In the original code (at the original 200p resolution) the sky is setup to step exactly one texel per vertical pixel
		// However with higher resolutions this must be scaled to look the same.
		if (s_height == SKY_BASE_HEIGHT)
		{
			s_vCoordStep = ONE_16;
			heightScale = ONE_16;
		}
		else
		{
			s_vCoordStep = ONE_16 * SKY_BASE_HEIGHT / s_height;
			heightScale = div16(intToFixed16(SKY_BASE_HEIGHT), intToFixed16(s_height));
		}
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
					s_vCoordFixed = intToFixed16(s_texHeightMask - y1) - s_skyPitchOffset - sector->ceilOffsetZ;
				}
				else
				{
					s_vCoordFixed = intToFixed16(s_texHeightMask) - mul16(intToFixed16(y1), heightScale) - s_skyPitchOffset - sector->ceilOffsetZ;
				}

				s32 texelU = ( floor16(sector->ceilOffsetX - s_skyYawOffset + s_skyTable[x]) ) & texWidthMask;
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

		fixed16_16 heightScale;
		// In the original code (at the original 200p resolution) the sky is setup to step exactly one texel per vertical pixel
		// However with higher resolutions this must be scaled to look the same.
		if (s_height == SKY_BASE_HEIGHT)
		{
			s_vCoordStep = ONE_16;
			heightScale = ONE_16;
		}
		else
		{
			s_vCoordStep = ONE_16 * SKY_BASE_HEIGHT / s_height;
			heightScale = div16(intToFixed16(SKY_BASE_HEIGHT), intToFixed16(s_height));
		}

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
					s_vCoordFixed = intToFixed16(s_texHeightMask - y1) - s_skyPitchOffset - sector->ceilOffsetZ;
				}
				else
				{
					s_vCoordFixed = intToFixed16(s_texHeightMask) - mul16(intToFixed16(y1), heightScale) - s_skyPitchOffset - sector->ceilOffsetZ;
				}

				s32 widthMask = texture->width - 1;
				s32 texelU = floor16(sector->ceilOffsetX - s_skyYawOffset + s_skyTable[x]) & widthMask;
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
		fixed16_16 heightScale;
		// In the original code (at the original 200p resolution) the sky is setup to step exactly one texel per vertical pixel
		// However with higher resolutions this must be scaled to look the same.
		if (s_height == SKY_BASE_HEIGHT)
		{
			s_vCoordStep = ONE_16;
			heightScale = ONE_16;
		}
		else
		{
			s_vCoordStep = ONE_16 * SKY_BASE_HEIGHT / s_height;
			heightScale = div16(intToFixed16(SKY_BASE_HEIGHT), intToFixed16(s_height));
		}
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
					s_vCoordFixed = intToFixed16(s_texHeightMask - y1) - s_skyPitchOffset - sector->floorOffsetZ;
				}
				else
				{
					s_vCoordFixed = intToFixed16(s_texHeightMask) - mul16(intToFixed16(y1), heightScale) - s_skyPitchOffset - sector->floorOffsetZ;
				}

				s32 texelU = floor16(sector->floorOffsetX - s_skyYawOffset + s_skyTable[x]) & texWidthMask;
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

		fixed16_16 heightScale;
		// In the original code (at the original 200p resolution) the sky is setup to step exactly one texel per vertical pixel
		// However with higher resolutions this must be scaled to look the same.
		if (s_height == SKY_BASE_HEIGHT)
		{
			s_vCoordStep = ONE_16;
			heightScale = ONE_16;
		}
		else
		{
			s_vCoordStep = ONE_16 * SKY_BASE_HEIGHT / s_height;
			heightScale = div16(intToFixed16(SKY_BASE_HEIGHT), intToFixed16(s_height));
		}

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
					s_vCoordFixed = intToFixed16(s_texHeightMask - y1) - s_skyPitchOffset - sector->floorOffsetZ;
				}
				else
				{
					s_vCoordFixed = intToFixed16(s_texHeightMask) - mul16(intToFixed16(y1), heightScale) - s_skyPitchOffset - sector->floorOffsetZ;
				}

				s32 widthMask = texture->width - 1;
				s32 texelU = floor16(sector->floorOffsetX - s_skyYawOffset + s_skyTable[x]) & widthMask;
				s_texImage = &texture->image[texelU << texture->logSizeY];
				s_columnOut = &s_display[y0*s_width + x];

				drawColumn_Fullbright();
			}
		}
	}
	
	// Determines if segment A is disjoint from the line formed by B - i.e. they do not intersect.
	// Returns 1 if segment A does NOT cross line B or 0 if it does.
	s32 segmentCrossesLine(fixed16_16 ax0, fixed16_16 ay0, fixed16_16 ax1, fixed16_16 ay1, fixed16_16 bx0, fixed16_16 by0, fixed16_16 bx1, fixed16_16 by1)
	{
		// Convert from 16 fractional bits to 12.
		bx0 = fixed16to12(bx0);
		by0 = fixed16to12(by0);
		ax1 = fixed16to12(ax1);
		ax0 = fixed16to12(ax0);
		ay0 = fixed16to12(ay0);
		ay1 = fixed16to12(ay1);
		bx1 = fixed16to12(bx1);
		by1 = fixed16to12(by1);

		// mul16() functions on 12 bit values is equivalent to: a * b / 16
		// [ (a1-b0)x(b1-b0) ].[ (a0-b0)x(b1 - b0) ]
		// In 2D x = "perp product"
		s_segmentCross = mul16(mul16(ax1 - bx0, by1 - by0) - mul16(ay1 - by0, bx1 - bx0),
			                   mul16(ax0 - bx0, by1 - by0) - mul16(ay0 - by0, bx1 - bx0));

		return s_segmentCross > 0 ? 1 : 0;
	}
		
	// When solving for Z, part of the computation can be done once per wall.
	fixed16_16 solveForZ_Numerator(RWallSegment* wallSegment)
	{
		const fixed16_16 z0 = wallSegment->z0;

		fixed16_16 numerator;
		if (wallSegment->orient == WORIENT_DZ_DX)
		{
			numerator = z0 - mul16(wallSegment->slope, wallSegment->x0View);
		}
		else  // WORIENT_DX_DZ
		{
			numerator = wallSegment->x0View - mul16(wallSegment->slope, z0);
		}

		return numerator;
	}
	
	// Solve for perspective correct Z at the current x pixel coordinate.
	fixed16_16 solveForZ(RWallSegment* wallSegment, s32 x, fixed16_16 numerator, fixed16_16* outViewDx/*=nullptr*/)
	{
		fixed16_16 z;	// perspective correct z coordinate at the current x pixel coordinate.
		if (wallSegment->orient == WORIENT_DZ_DX)
		{
			// Solve for viewspace X at the current pixel x coordinate in order to get dx in viewspace.
			fixed16_16 den = s_column_Y_Over_X[x] - wallSegment->slope;
			// Avoid divide by zero.
			if (den == 0) { den = 1; }

			fixed16_16 xView = div16(numerator, den);
			// Use the saved x0View to get dx in viewspace.
			fixed16_16 dxView = xView - wallSegment->x0View;
			// avoid recalculating for u coordinate computation.
			if (outViewDx) { *outViewDx = dxView; }
			// Once we have dx in viewspace, we multiply by the slope (dZ/dX) in order to get dz.
			fixed16_16 dz = mul16(dxView, wallSegment->slope);
			// Then add z0 to get z(x) that is perspective correct.
			z = wallSegment->z0 + dz;
		}
		else  // WORIENT_DX_DZ
		{
			// Directly solve for Z at the current pixel x coordinate.
			fixed16_16 den = s_column_X_Over_Y[x] - wallSegment->slope;
			// Avoid divide by 0.
			if (den == 0) { den = 1; }

			z = div16(numerator, den);
		}

		return z;
	}
		
	void drawColumn_Fullbright()
	{
		fixed16_16 vCoordFixed = s_vCoordFixed;
		u8* tex = s_texImage;

		s32 v = floor16(vCoordFixed) & s_texHeightMask;
		s32 end = s_yPixelCount - 1;

		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width)
		{
			const u8 c = tex[v];
			vCoordFixed += s_vCoordStep;
			v = floor16(vCoordFixed) & s_texHeightMask;
			s_columnOut[offset] = c;
		}
	}

	void drawColumn_Lit()
	{
		fixed16_16 vCoordFixed = s_vCoordFixed;
		u8* tex = s_texImage;

		s32 v = floor16(vCoordFixed) & s_texHeightMask;
		s32 end = s_yPixelCount - 1;

		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width)
		{
			const u8 c = s_columnLight[tex[v]];
			vCoordFixed += s_vCoordStep;
			v = floor16(vCoordFixed) & s_texHeightMask;
			s_columnOut[offset] = c;
		}
	}

	void drawColumn_Fullbright_Trans()
	{
		fixed16_16 vCoordFixed = s_vCoordFixed;
		u8* tex = s_texImage;

		s32 v = floor16(vCoordFixed) & s_texHeightMask;
		s32 end = s_yPixelCount - 1;

		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width)
		{
			const u8 c = tex[v];
			vCoordFixed += s_vCoordStep;
			v = floor16(vCoordFixed) & s_texHeightMask;
			if (c) { s_columnOut[offset] = c; }
		}
	}

	void drawColumn_Lit_Trans()
	{
		fixed16_16 vCoordFixed = s_vCoordFixed;
		u8* tex = s_texImage;

		s32 v = floor16(vCoordFixed) & s_texHeightMask;
		s32 end = s_yPixelCount - 1;

		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width)
		{
			const u8 c = tex[v];
			vCoordFixed += s_vCoordStep;
			v = floor16(vCoordFixed) & s_texHeightMask;
			if (c) { s_columnOut[offset] = s_columnLight[c]; }
		}
	}

	void wall_addAdjoinSegment(s32 length, s32 x0, fixed16_16 top_dydx, fixed16_16 y1, fixed16_16 bot_dydx, fixed16_16 y0, RWallSegment* wallSegment)
	{
		if (s_adjoinSegCount < MAX_ADJOIN_SEG)
		{
			fixed16_16 lengthFixed = intToFixed16(length - 1);
			fixed16_16 y0End = y0;
			if (bot_dydx != 0)
			{
				y0End += mul16(bot_dydx, lengthFixed);
			}
			fixed16_16 y1End = y1;
			if (top_dydx != 0)
			{
				y1End += mul16(top_dydx, lengthFixed);
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
			fixed16_16 wFloorHeight = sector->floorHeight;
			fixed16_16 wCeilHeight = sector->ceilingHeight;
			RSector* nextSector = mirror->sector;
			fixed16_16 mFloorHeight = nextSector->floorHeight;
			fixed16_16 mCeilHeight = nextSector->ceilingHeight;
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
				wall->topTexelHeight = (next->ceilingHeight - cur->ceilingHeight) * 8;
			}
			if (wall->drawFlags & 2)
			{
				RSector* cur = wall->sector;
				RSector* next = wall->nextSector;
				wall->botTexelHeight = (cur->floorHeight - next->floorHeight) * 8;
			}
			if (wall->midTex)
			{
				if (!(wall->drawFlags & 2) && !(wall->drawFlags & 1))
				{
					RSector* midSector = wall->sector;
					fixed16_16 midFloorHeight = wall->sector->floorHeight;
					wall->midTexelHeight = (midFloorHeight - midSector->ceilingHeight) * 8;
				}
				else if (!(wall->drawFlags & 2))
				{
					RSector* midSector = wall->nextSector;
					fixed16_16 midFloorHeight = wall->sector->floorHeight;
					wall->midTexelHeight = (midFloorHeight - midSector->ceilingHeight) * 8;
				}
				else if (!(wall->drawFlags & 1))
				{
					RSector* midSector = wall->sector;
					fixed16_16 midFloorHeight = wall->nextSector->floorHeight;
					wall->midTexelHeight = (midFloorHeight - midSector->ceilingHeight) * 8;
				}
				else
				{
					RSector* midSector = wall->nextSector;
					fixed16_16 midFloorHeight = wall->nextSector->floorHeight;
					wall->midTexelHeight = (midFloorHeight - midSector->ceilingHeight) * 8;
				}
			}
		}
		else
		{
			RSector* midSector = wall->sector;
			s32 midFloorHeight = midSector->floorHeight;
			wall->midTexelHeight = (midFloorHeight - midSector->ceilingHeight) * 8;
		}
	}
}
