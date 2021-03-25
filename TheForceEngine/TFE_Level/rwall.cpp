#include "rwall.h"
#include "rsector.h"

namespace TFE_Level
{
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
			if (wall->drawFlags & WDF_TOP)
			{
				RSector* next = wall->nextSector;
				RSector* cur = wall->sector;
				wall->topTexelHeight = (next->ceilingHeight - cur->ceilingHeight) << 3;
			}
			if (wall->drawFlags & WDF_BOT)
			{
				RSector* cur = wall->sector;
				RSector* next = wall->nextSector;
				wall->botTexelHeight = (cur->floorHeight - next->floorHeight) << 3;
			}

			if (wall->midTex)
			{
				RSector* midSector = wall->sector;

				if (!(wall->drawFlags & WDF_BOT) && !(wall->drawFlags & WDF_TOP))
				{
					fixed16_16 midFloorHeight = wall->sector->floorHeight;
					wall->midTexelHeight = (midFloorHeight - midSector->ceilingHeight) << 3;
				}
				else if (!(wall->drawFlags & WDF_BOT))
				{
					fixed16_16 midFloorHeight = wall->sector->floorHeight;
					wall->midTexelHeight = (midFloorHeight - midSector->ceilingHeight) << 3;
				}
				else if (!(wall->drawFlags & WDF_TOP))
				{
					fixed16_16 midFloorHeight = wall->nextSector->floorHeight;
					wall->midTexelHeight = (midFloorHeight - midSector->ceilingHeight) << 3;
				}
				else // WDF_TOP_AND_BOT
				{
					fixed16_16 midFloorHeight = wall->nextSector->floorHeight;
					wall->midTexelHeight = (midFloorHeight - midSector->ceilingHeight) << 3;
				}
			}
		}
		else
		{
			RSector* midSector = wall->sector;
			fixed16_16 midFloorHeight = midSector->floorHeight;
			wall->midTexelHeight = (midFloorHeight - midSector->ceilingHeight) << 3;
		}
	}

	fixed16_16 wall_computeDirectionVector(RWall* wall)
	{
		// Calculate dx and dz
		fixed16_16 dx = wall->w1->x - wall->w0->x;
		fixed16_16 dz = wall->w1->z - wall->w0->z;

		// The original DOS code converts to floating point here to compute the length but
		// switches back to fixed point for the divide.
		// Convert to floating point
		f32 fdx = fixed16ToFloat(dx);
		f32 fdz = fixed16ToFloat(dz);
		// Compute the squared distance in floating point.
		f32 lenSq = fdx * fdx + fdz * fdz;
		f32 len = sqrtf(lenSq);
		fixed16_16 lenFixed = floatToFixed16(len);

		if (lenFixed)
		{
			wall->wallDir.x = div16(dx, lenFixed);
			wall->wallDir.z = div16(dz, lenFixed);
		}
		else
		{
			wall->wallDir.x = 0;
			wall->wallDir.z = 0;
		}
		return lenFixed;
	}
} // namespace TFE_Level