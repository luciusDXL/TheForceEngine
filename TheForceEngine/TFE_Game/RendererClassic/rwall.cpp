#include "rwall.h"
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
		return 0;
	}

	void wall_drawSolid(RWallSegment* wallSegment)
	{
	}
}
