#include "rflat.h"
#include "rsector.h"
#include "fixedPoint.h"
#include "rmath.h"
#include "rcommon.h"

using namespace RendererClassic;
using namespace FixedPoint;
using namespace RMath;

namespace RClassicFlat
{
	void flat_setup(s32 length, s32 x0, s32 dyFloor_dx, s32 yFloor1, s32 yFloor, s32 dyCeil_dx, s32 yCeil, s32 yCeil1, FlatEdges* flat)
	{
		const s32 yF0 = round16(yFloor);
		const s32 yF1 = round16(yFloor1);
		const s32 yC0 = round16(yCeil);
		const s32 yC1 = round16(yCeil1);

		flat->yCeil0 = yCeil;
		flat->yCeil1 = yCeil1;
		flat->dyCeil_dx = dyCeil_dx;
		if (yC0 < yC1)
		{
			flat->yPixel_C0 = yC0;
			flat->yPixel_C1 = yC1;
		}
		else
		{
			flat->yPixel_C0 = yC1;
			flat->yPixel_C1 = yC0;
		}

		flat->yFloor0 = yFloor;
		flat->yFloor1 = yFloor1;
		flat->dyFloor_dx = dyFloor_dx;
		if (yF0 > yF1)
		{
			flat->yPixel_F0 = yF0;
			flat->yPixel_F1 = yF1;
		}
		else
		{
			flat->yPixel_F0 = yF1;
			flat->yPixel_F1 = yF0;
		}

		flat->lengthInPixels = length;
		flat->x0 = x0;
		flat->x1 = x0 + length - 1;
	}

	void flat_addEdges(s32 length, s32 x0, s32 dyFloor_dx, s32 yFloor, s32 dyCeil_dx, s32 yCeil)
	{
		if (s_flatCount < MAX_SEG && length > 0)
		{
			const s32 lengthFixed = (length - 1) << 16;

			s32 yCeil1 = yCeil;
			if (dyCeil_dx != 0)
			{
				yCeil1 += mul16(dyCeil_dx, lengthFixed);
			}
			s32 yFloor1 = yFloor;
			if (dyFloor_dx != 0)
			{
				yFloor1 += mul16(dyFloor_dx, lengthFixed);
			}

			flat_setup(length, x0, dyFloor_dx, yFloor1, yFloor, dyCeil_dx, yCeil, yCeil1, s_lowerFlatEdge);

			if (s_lowerFlatEdge->yPixel_C1 - 1 > s_wallMaxCeilY)
			{
				s_wallMaxCeilY = s_lowerFlatEdge->yPixel_C1 - 1;
			}
			if (s_lowerFlatEdge->yPixel_F1 + 1 < s_wallMinFloorY)
			{
				s_wallMinFloorY = s_lowerFlatEdge->yPixel_F1 + 1;
			}
			if (s_wallMaxCeilY < s_windowMinY)
			{
				s_wallMaxCeilY = s_windowMinY;
			}
			if (s_wallMinFloorY > s_windowMaxY)
			{
				s_wallMinFloorY = s_windowMaxY;
			}

			s_lowerFlatEdge++;
			s_flatCount++;
		}
	}

	void flat_drawCeiling(RSector* sector, FlatEdges* edges, s32 count)
	{
		// TODO
	}

	void flat_drawFloor(RSector* sector, FlatEdges* edges, s32 count)
	{
		// TODO
	}
}