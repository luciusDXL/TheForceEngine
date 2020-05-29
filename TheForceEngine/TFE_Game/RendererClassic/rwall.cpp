#include "rwall.h"
#include "rsector.h"
#include "fixedPoint.h"
#include "rmath.h"
#include "rcommon.h"
#include <TFE_Renderer/renderer.h>
#include <TFE_System/system.h>
#include <TFE_System/math.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Asset/levelAsset.h>
#include <TFE_Asset/levelObjectsAsset.h>
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

namespace RClassicWall
{
	enum SegSide
	{
		FRONT = 0xffff,
		BACK = 0,
	};

	static s32 s_segmentCross;
	s32 segmentCrossesLine(s32 ax0, s32 ay0, s32 ax1, s32 ay1, s32 bx0, s32 by0, s32 bx1, s32 by1);
	s32 solveForZ_Numerator(RWallSegment* wallSegment);
	s32 solveForZ(RWallSegment* wallSegment, s32 x, s32 numerator, s32* outViewDx=nullptr);
	void drawColumn_Fullbright(s32 x, s32 y0, s32 y1);

	// Process the wall and produce an RWallSegment for rendering if the wall is potentially visible.
	void wall_process(RWall* wall)
	{
		const vec2* p0 = wall->v0;
		const vec2* p1 = wall->v1;

		// viewspace wall coordinates.
		s32 x0 = p0->x;
		s32 x1 = p1->x;
		s32 z0 = p0->z;
		s32 z1 = p1->z;

		// x values of frustum lines that pass through (x0,z0) and (x1,z1)
		s32 left0 = -z0;
		s32 left1 = -z1;
		s32 right0 = z0;
		s32 right1 = z1;

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

		s32 dx = x1 - x0;
		s32 dz = z1 - z0;
		// Cull the wall if it is back facing.
		// y0*dx - x0*dy
		const s32 side = mul16(z0, dx) - mul16(x0, dz);
		if (side < 0)
		{
			wall->visible = 0;
			return;
		}

		s32 curU = 0;
		s32 clipLeft = 0;
		s32 clipRight = 0;
		s32 clipX0 = 0;

		s32 texelLen = wall->texelLength;
		s32 texelLenRem = texelLen;

		//////////////////////////////////////////////
		// Clip the Wall Segment by the left and right
		// frustum lines.
		//////////////////////////////////////////////

		// The wall segment extends past the left clip line.
		if (x0 < left0)
		{
			// Intersect the segment (x0, z0),(x1, z1) with the frustum line that passes through (-z0, z0) and (-z1, z1)
			s32 xz = mul16(x0, z1) - mul16(z0, x1);
			s32 dyx = -dz - dx;
			if (dyx != 0)
			{
				xz = div16(xz, dyx);
			}

			// Compute the parametric intersection of the segment and the left frustum line
			// where s is in the range of [0.0, 1.0]
			s32 s = 0;
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
				s32 clipLen = mul16(texelLenRem, s);
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
			s32 xz = mul16(x0, z1) - mul16(z0, x1);
			s32 dyx = dz - dx;
			if (dyx != 0)
			{
				xz = div16(xz, dyx);
			}

			// Compute the parametric intersection of the segment and the left frustum line
			// where s is in the range of [0.0, 1.0]
			// Note we are computing from the right side, i.e. distance from (x1,y1).
			s32 s = 0;
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
				s32 adjLen = texelLen + mul16(texelLenRem, s);
				s32 adjLenMinU = adjLen - curU;

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
		// TODO

		//////////////////////////////////////////////////
		// Project.
		//////////////////////////////////////////////////
		s32 x0proj  = div16(mul16(x0, s_focalLength), z0) + s_halfWidth;
		s32 x1proj  = div16(mul16(x1, s_focalLength), z1) + s_halfWidth;
		s32 x0pixel = round16(x0proj);
		s32 x1pixel = round16(x1proj) - 1;
		if (clipX0 != 0)
		{
			if (x0pixel < s_minScreenX)
			{
				x0 = -ONE_16;
				x1 += ONE_16;
				x0pixel = s_minScreenX;
			}
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
		if (s_nextWall == MAX_SEG)	// ebx = 0
		{
			TFE_System::logWrite(LOG_ERROR, "ClassicRenderer", "Wall_Process : Maximum processed walls exceeded!");
			wall->visible = 0;
			return;
		}
	
		RWallSegment* wallSeg = &s_wallSegListSrc[s_nextWall];
		s_nextWall++;

		if (x0pixel < clipX0)
		{
			x0pixel = clipX0;
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

		s32 slope, den, orient;
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
		if (x0pixel == x1pixel)
		{
			wallSeg->slope = 0;
		}

		wall->visible = 1;
	}

	s32 wall_mergeSort(RWallSegment* segOutList, s32 availSpace, s32 start, s32 count)
	{
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
			//bool processed = (s_drawFrame == srcWall->drawFrame);
			bool processed = false;
			bool insideWindow = ((srcSeg->z0 >= s_minSegZ || srcSeg->z1 >= s_minSegZ) && srcSeg->wallX0 <= s_windowMaxX && srcSeg->wallX1 >= s_windowMinX);
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

					s32 newMinZ = min(newV0->z, newV1->z);
					s32 newMaxZ = max(newV0->z, newV1->z);
					s32 outMinZ = min(outV0->z, outV1->z);
					s32 outMaxZ = max(outV0->z, outV1->z);
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
								 segmentCrossesLine(newV1->x, newV1->z, 0, 0, outV0->x, outV0->z, outV1->x, outV1->z) == 0))		// (newV1, 0) crosses (outV0, outV1)
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

							// We are deleting outCur since it is completely hidden by 'newSeg'
							// Back up 1 step in the loop, so that outNext is processed (it will hold the same location as outCur before deletion).
							curSegOut = curSegOut - 1;
							sortedSeg = sortedSeg - 1;
							outIndex--;
							n--;

							// We are deleting outCur since it is completely hidden by moving all segments from outNext onwards to outCur.
							memmove(outCur, outNext, copyCount * sizeof(RWallSegment));
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
									*splitWall = *sortedSeg;
									splitWall->wallX0 = newSeg->wallX1 + 1;
									splitWallCount++;
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
								if (newV1->z >= newV0->z)
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

	static s32 s_texHeightMask;
	static s32 s_yPixelCount;
	static s32 s_vCoordStep;
	static s32 s_vCoordFixed;
	static u8  s_columnAtten;
	static u8* s_texImage;

	void wall_drawSolid(RWallSegment* wallSegment)
	{
		RWall* srcWall = wallSegment->srcWall;
		RSector* sector = srcWall->sector;
		TextureFrame* texture = srcWall->midTex;

		s32 ceilingHeight = sector->ceilingHeight;
		s32 floorHeight = sector->floorHeight;

		s32 ceilEyeRel  = ceilingHeight - s_eyeHeight;
		s32 floorEyeRel = floorHeight   - s_eyeHeight;

		s32 z0 = wallSegment->z0;
		s32 z1 = wallSegment->z1;

		// s_halfWidthFixed -> s_focalLength
		// halfHeight * tan(fov/2) * aspect, where aspect = 320/200, so:
		// 100 * 1.0 * 320/200 = 160
		s32 y0C = div16(mul16(ceilEyeRel,  s_focalLength), z0) + s_halfHeight;
		s32 y0F = div16(mul16(floorEyeRel, s_focalLength), z0) + s_halfHeight;

		s32 y1C = div16(mul16(ceilEyeRel,  s_focalLength), z1) + s_halfHeight;
		s32 y1F = div16(mul16(floorEyeRel, s_focalLength), z1) + s_halfHeight;

		s32 y0C_pixel = round16(y0C);
		s32 y1C_pixel = round16(y1C);

		s32 y0F_pixel = round16(y0F);
		s32 y1F_pixel = round16(y1F);

		s32 x = wallSegment->wallX0;
		s32 length = wallSegment->wallX1 - wallSegment->wallX0 + 1;
		s32 numerator = solveForZ_Numerator(wallSegment);

		// For some reason we only early-out if the ceiling is below the view.
		if (y0C_pixel > s_windowMaxY && y1C_pixel > s_windowMaxY)
		{
			memset(&s_columnTop[x], s_windowMaxY, length * 4);

			s32 yMax = (s_windowMaxY + 1) << 16;
			//addWallPartScreen(length, x, 0, yMax, 0, yMax);

			for (s32 i = 0; i < length; i++, x++)
			{
				s_depth1d[x] = solveForZ(wallSegment, x, numerator);
			}

			srcWall->visible = 0;
			//srcWall->y1 = -1;
			return;
		}

		s_texHeightMask = texture ? texture->height - 1 : 0;
		TextureFrame* signTex = srcWall->signTex;
		if (signTex)	// ecx
		{
			// 1fd683:
		}

		s32 wallDeltaX = (wallSegment->wallX1_raw - wallSegment->wallX0_raw) << 16;
		s32 dYdXtop = 0, dYdXbot = 0;
		if (wallDeltaX != 0)
		{
			dYdXtop = div16(y1C - y0C, wallDeltaX);
			dYdXbot = div16(y1F - y0F, wallDeltaX);
		}

		s32 clippedXDelta = (wallSegment->wallX0 - wallSegment->wallX0_raw) << 16;
		if (clippedXDelta != 0)
		{
			y0C += mul16(dYdXtop, clippedXDelta);
			y0F += mul16(dYdXbot, clippedXDelta);
		}
		//addWallPartScreen(length, wallSegment->wallX0, dYdXbot, y0F, dYdXtop, y0C);

		const s32 texWidth = texture->width;
		const bool flipHorz = (srcWall->flags1 & WF1_FLIP_HORIZ) != 0;
				
		for (s32 i = 0; i < length; i++, x++)
		{
			s32 top = round16(y0C);
			s32 bot = round16(y0F);
			s_columnBot[x] = bot + 1;
			s_columnTop[x] = top - 1;

			if (top < s_windowTop[x])
			{
				top = s_windowTop[x];
			}
			if (bot > s_windowBot[x])
			{
				bot = s_windowBot[x];
			}
			s_yPixelCount = bot - top + 1;

			s32 dxView = 0;
			s32 z = solveForZ(wallSegment, x, numerator, &dxView);
			s_depth1d[x] = z;

			s32 uScale = wallSegment->uScale;
			s32 uCoord0 = wallSegment->uCoord0 + srcWall->midUOffset;
			s32 uCoord = uCoord0 + ((wallSegment->orient == WORIENT_DZ_DX) ? mul16(dxView, uScale) : mul16(z - z0, uScale));

			if (s_yPixelCount > 0)
			{
				// texture wrapping, assumes texWidth is a power of 2.
				s32 texelU = (uCoord >> 16) & (texWidth - 1);
				// flip texture coordinate if flag set.
				if (flipHorz) { texelU = texWidth - texelU; }

				// Calculate the vertical texture coordinate start and increment.
				s32 wallHeightPixels = y0F - y0C + ONE_16;
				s32 wallHeightTexels = srcWall->midTexelHeight;

				// s_vCoordStep = tex coord "v" step per y pixel step -> dVdY;
				// Note the result is in x.16 fixed point, which is why it must be shifted by 16 again before divide.
				s_vCoordStep = div16(wallHeightTexels, wallHeightPixels);

				// texel offset from the actual fixed point y position and the truncated y position.
				s32 vPixelOffset = y0F - (bot << 16) + HALF_16;

				// scale the texel offset based on the v coord step.
				// the result is the sub-texel offset
				s32 v0 = mul16(s_vCoordStep, vPixelOffset);
				s_vCoordFixed = v0 + srcWall->midVOffset;

				// Texture image data = imageStart + u * texHeight
				s_texImage = texture->image + (texelU << texture->logSizeY);

				// Skip for now and just write directly to the screen...
				// columnOutStart + (x*200 + top) * 80;
				//s_curColumnOut = s_columnOut[x] + s_scaleX80[top];
				//s_columnAtten = computeColumnAtten(z, srcWall->wallLight);
				s_columnAtten = 0;

				// draw the column
				if (s_columnAtten != 0)
				{
					//drawColumn_Lit(top, bot);
				}
				else
				{
					drawColumn_Fullbright(x, top, bot);
				}

				if (signTex)
				{
					// 1FD9E3:
				}
			}

			y0C += dYdXtop;
			y0F += dYdXbot;
		}

		//srcWall->y1 = -1;
	}

	// Determines if segment A is disjoint from the line formed by B - i.e. they do not intersect.
	// Returns 1 if segment A does NOT cross line B or 0 if it does.
	s32 segmentCrossesLine(s32 ax0, s32 ay0, s32 ax1, s32 ay1, s32 bx0, s32 by0, s32 bx1, s32 by1)
	{
		// Convert from 16 fractional bits to 12.
		bx0 >>= 4;
		by0 >>= 4;
		ax1 >>= 4;
		ax0 >>= 4;
		ay0 >>= 4;
		ay1 >>= 4;
		bx1 >>= 4;
		by1 >>= 4;

		// mul16() functions on 12 bit values is equivalent to: a * b / 16
		// [ (a1-b0)x(b1-b0) ].[ (a0-b0)x(b1 - b0) ]
		// In 2D x = "perp product"
		s_segmentCross = mul16(mul16(ax1 - bx0, by1 - by0) - mul16(ay1 - by0, bx1 - bx0),
			                   mul16(ax0 - bx0, by1 - by0) - mul16(ay0 - by0, bx1 - bx0));

		return s_segmentCross > 0 ? 1 : 0;
	}
		
	// When solving for Z, part of the computation can be done once per wall.
	s32 solveForZ_Numerator(RWallSegment* wallSegment)
	{
		const s32 z0 = wallSegment->z0;

		s32 numerator;
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
	s32 solveForZ(RWallSegment* wallSegment, s32 x, s32 numerator, s32* outViewDx/*=nullptr*/)
	{
		s32 z;	// perspective correct z coordinate at the current x pixel coordinate.
		if (wallSegment->orient == WORIENT_DZ_DX)
		{
			// Solve for viewspace X at the current pixel x coordinate in order to get dx in viewspace.
			s32 xView = wallSegment->slope ? div16(numerator, s_column_Y_Over_X[x] - wallSegment->slope) : 0;
			// Use the saved x0View to get dx in viewspace.
			s32 dxView = xView - wallSegment->x0View;
			// avoid recalculating for u coordinate computation.
			if (outViewDx) { *outViewDx = dxView; }
			// Once we have dx in viewspace, we multiply by the slope (dZ/dX) in order to get dz.
			s32 dz = mul16(dxView, wallSegment->slope);
			// Then add z0 to get z(x) that is perspective correct.
			z = wallSegment->z0 + dz;
		}
		else  // WORIENT_DX_DZ
		{
			// Directly solve for Z at the current pixel x coordinate.
			z = wallSegment->slope ? div16(numerator, s_column_X_Over_Y[x] - wallSegment->slope) : 0;
		}

		return z;
	}

	// x coordinate is temporary, just to get something showing.
	void drawColumn_Fullbright(s32 x, s32 y0, s32 y1)
	{
		s32 vCoordFixed = s_vCoordFixed;
		u8* tex = s_texImage;

		s32 v = floor16(vCoordFixed) & s_texHeightMask;
		s32 end = s_yPixelCount - 1;

		s32 offset = end * 80;
		u8* display = &s_display[x + y1*s_width];
		for (s32 i = end; i >= 0; i--, offset -= 80)
		{
			const u8 c = tex[v];
			vCoordFixed += s_vCoordStep;		// edx
			v = floor16(vCoordFixed) & s_texHeightMask;
			//s_curColumnOut[offset] = c;
			// Temporary - replace with proper columnOut code.
			*display = c;
			display -= s_width;
		}
	}
}
