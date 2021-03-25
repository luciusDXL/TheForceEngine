#include "rsector.h"
#include "rwall.h"
#include "robject.h"

namespace TFE_Level
{
	// Where should this be stored?
	static fixed16_16 s_playerHeight;

	void sector_clear(RSector* sector)
	{
		sector->vertexCount = 0;
		sector->wallCount = 0;
		sector->objectCount = 0;
		sector->secHeight = 0;
		sector->collisionFrame = 0;
		sector->id = 0;
		sector->prevDrawFrame = 0;
		sector->infLink = 0;
		sector->objectCapacity = 0;
		sector->verticesWS = nullptr;
		sector->verticesVS = nullptr;
		sector->self = sector;
	}

	void sector_setupWallDrawFlags(RSector* sector)
	{
		RWall* wall = sector->walls;
		for (s32 w = 0; w < sector->wallCount; w++, wall++)
		{
			if (wall->nextSector)
			{
				RSector* wSector = wall->sector;
				fixed16_16 wFloorHeight = wSector->floorHeight;
				fixed16_16 wCeilHeight = wSector->ceilingHeight;

				RWall* mirror = wall->mirrorWall;
				RSector* mSector = mirror->sector;
				fixed16_16 mFloorHeight = mSector->floorHeight;
				fixed16_16 mCeilHeight = mSector->ceilingHeight;

				wall->drawFlags = WDF_MIDDLE;
				mirror->drawFlags = WDF_MIDDLE;

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
				wall_computeTexelHeights(wall->mirrorWall);
			}
			wall_computeTexelHeights(wall);
		}
	}
		
	void sector_adjustHeights(RSector* sector, fixed16_16 floorOffset, fixed16_16 ceilOffset, fixed16_16 secondHeightOffset)
	{
		// Adjust objects.
		if (sector->objectCount)
		{
			fixed16_16 heightOffset = secondHeightOffset + floorOffset;
			for (s32 i = 0; i < sector->objectCapacity; i++)
			{
				SecObject* obj = sector->objectList[i];
				if (!obj) { continue; }
				
				if (obj->posWS.y == sector->floorHeight)
				{
					obj->posWS.y += floorOffset;
					if (obj->typeFlags & OTFLAG_PLAYER)
					{
						s_playerHeight += floorOffset;
					}
				}

				if (obj->posWS.y == sector->ceilingHeight)
				{
					obj->posWS.y += ceilOffset;
					if (obj->typeFlags & OTFLAG_PLAYER)
					{
						// Why not ceilingOffset?
						s_playerHeight += floorOffset;
					}
				}

				fixed16_16 secHeight = sector->floorHeight + sector->secHeight;
				if (obj->posWS.y == secHeight)
				{
					obj->posWS.y += secondHeightOffset;
					if (obj->typeFlags & OTFLAG_PLAYER)
					{
						// Why not secHeightOffset?
						s_playerHeight += floorOffset;
					}
				}
			}
		}
		// Adjust sector heights.
		sector->ceilingHeight += ceilOffset;
		sector->floorHeight += floorOffset;
		sector->secHeight += secondHeightOffset;

		// Update wall data.
		s32 wallCount = sector->wallCount;
		RWall* wall = sector->walls;
		for (s32 w = 0; w < wallCount; w++, wall++)
		{
			if (wall->nextSector)
			{
				wall_setupAdjoinDrawFlags(wall);
				wall_computeTexelHeights(wall->mirrorWall);
			}
			wall_computeTexelHeights(wall);
		}

		// Update collision data.
		fixed16_16 floorHeight = sector->floorHeight;
		if (sector->flags1 & SEC_FLAGS1_PIT)
		{
			floorHeight += FIXED(100);
		}
		fixed16_16 ceilHeight = sector->ceilingHeight;
		if (sector->flags1 & SEC_FLAGS1_EXTERIOR)
		{
			ceilHeight -= FIXED(100);
		}
		fixed16_16 secHeight = sector->floorHeight + sector->secHeight;
		if (sector->secHeight >= 0 && floorHeight > secHeight)
		{
			secHeight = floorHeight;
		}

		sector->colFloorHeight = floorHeight;
		sector->colCeilHeight = ceilHeight;
		sector->colSecHeight = secHeight;
		sector->colMinHeight = ceilHeight;
	}

	void sector_computeBounds(RSector* sector)
	{
		RWall* wall = sector->walls;
		vec2_fixed* w0 = wall->w0;
		fixed16_16 maxX = w0->x;
		fixed16_16 maxZ = w0->z;
		fixed16_16 minX = maxX;
		fixed16_16 minZ = maxZ;

		wall++;
		for (s32 i = 1; i < sector->wallCount; i++, wall++)
		{
			w0 = wall->w0;

			minX = min(minX, w0->x);
			minZ = min(minZ, w0->z);

			maxX = max(maxX, w0->x);
			maxZ = max(maxZ, w0->z);
		}

		sector->boundsMin.x = minX;
		sector->boundsMax.x = maxX;
		sector->boundsMin.z = minZ;
		sector->boundsMax.z = maxZ;

		// Setup when needed.
		//s_minX = minX;
		//s_maxX = maxX;
		//s_minZ = minZ;
		//s_maxZ = maxZ;
	}
} // namespace TFE_Level