#include "rsector.h"
#include "rwall.h"
#include "robject.h"
#include "level.h"

namespace TFE_Level
{
	// TODO: Game Variables referenced by internal sector functions.
	// Where should this be stored?
	static fixed16_16 s_playerHeight;
	static u32 s_playerSecMoved = 0;
	static SecObject* s_playerObject;

	// Internal Forward Declarations
	void sector_computeWallDirAndLength(RWall* wall);
	void sector_moveWallVertex(RWall* wall, fixed16_16 offsetX, fixed16_16 offsetZ);
	u32  sector_objOverlapsWall(RWall* wall, SecObject* obj, s32* objSide);
	u32  sector_canWallMove(RWall* wall, fixed16_16 offsetX, fixed16_16 offsetZ);
	void sector_moveObjects(RSector* sector, u32 flags, fixed16_16 offsetX, fixed16_16 offsetZ);

	f32 isLeft(Vec2f p0, Vec2f p1, Vec2f p2);
	
	/////////////////////////////////////////////////
	// API Implementation
	/////////////////////////////////////////////////
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
		sector->dirtyFlags |= SDF_HEIGHTS;

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

	fixed16_16 sector_getMaxObjectHeight(RSector* sector)
	{
		s32 maxObjHeight = 0;
		s32 count = sector->objectCount;
		SecObject** objectList = sector->objectList;

		if (!sector->objectCount)
		{
			return 0;
		}

		for (; count > 0; objectList++)
		{
			SecObject* obj = *objectList;
			if (obj)
			{
				maxObjHeight = max(maxObjHeight, obj->worldHeight + ONE_16);
				count--;
			}
		}
		return maxObjHeight;
	}
			
	u32 sector_moveWalls(RSector* sector, fixed16_16 delta, fixed16_16 dirX, fixed16_16 dirZ, u32 flags)
	{
		sector->dirtyFlags |= SDF_VERTICES;

		fixed16_16 offsetX = mul16(delta, dirX);
		fixed16_16 offsetZ = mul16(delta, dirZ);

		u32 sectorBlocked = 0;
		s32 wallCount = sector->wallCount;
		RWall* wall = sector->walls;
		for (s32 i = 0; i < wallCount && !sectorBlocked; i++, wall++)
		{
			if (wall->flags1 & WF1_WALL_MORPHS)
			{
				sectorBlocked |= sector_canWallMove(wall, offsetX, offsetZ);

				RWall* mirror = wall->mirrorWall;
				if (wall->mirrorWall && (mirror->flags1 & WF1_WALL_MORPHS))
				{
					sectorBlocked |= sector_canWallMove(mirror, offsetX, offsetZ);
				}
			}
		}

		if (!sectorBlocked)
		{
			wall = sector->walls;
			for (s32 i = 0; i < wallCount; i++, wall++)
			{
				if (wall->flags1 & WF1_WALL_MORPHS)
				{
					sector_moveWallVertex(wall, offsetX, offsetZ);
					RWall* mirror = wall->mirrorWall;
					if (mirror && (mirror->flags1 & WF1_WALL_MORPHS))
					{
						sector_moveWallVertex(mirror, offsetX, offsetZ);
					}
				}
			}
			sector_moveObjects(sector, flags, offsetX, offsetZ);
			sector_computeBounds(sector);
		}

		return !sectorBlocked ? 0xffffffff : 0;
	}

	void sector_changeWallLight(RSector* sector, fixed16_16 delta)
	{
		sector->dirtyFlags |= SDF_WALL_LIGHT;

		RWall* wall = sector->walls;
		s32 wallCount = sector->wallCount;
		for (s32 i = 0; i < wallCount; i++, wall++)
		{
			if (wall->flags1 & WF1_CHANGE_WALL_LIGHT)
			{
				wall->wallLight += delta;
			}
		}
	}

	void sector_scrollWalls(RSector* sector, fixed16_16 offsetX, fixed16_16 offsetZ)
	{
		RWall* wall = sector->walls;
		s32 wallCount = sector->wallCount;
		sector->dirtyFlags |= SDF_WALL_OFFSETS;

		const u32 scrollFlags = WF1_SCROLL_SIGN_TEX | WF1_SCROLL_BOT_TEX | WF1_SCROLL_MID_TEX | WF1_SCROLL_TOP_TEX;
		for (s32 i = 0; i < wallCount; i++, wall++)
		{
			if (wall->flags1 & scrollFlags)
			{
				if (wall->flags1 & WF1_SCROLL_TOP_TEX)
				{
					wall->topOffset.x += offsetX;
					wall->topOffset.z += offsetZ;
				}
				if (wall->flags1 & WF1_SCROLL_MID_TEX)
				{
					wall->midOffset.x += offsetX;
					wall->midOffset.z += offsetZ;
				}
				if (wall->flags1 & WF1_SCROLL_BOT_TEX)
				{
					wall->botOffset.x += offsetX;
					wall->botOffset.z += offsetZ;
				}
				if (wall->flags1 & WF1_SCROLL_SIGN_TEX)
				{
					wall->signOffset.x += offsetX;
					wall->signOffset.z += offsetZ;
				}
			}
		}
	}

	void sector_adjustTextureWallOffsets_Floor(RSector* sector, fixed16_16 floorDelta)
	{
		sector->dirtyFlags |= SDF_WALL_OFFSETS;

		RWall* wall = sector->walls;
		s32 wallCount = sector->wallCount;
		for (s32 i = 0; i < wallCount; i++, wall++)
		{
			fixed16_16 textureOffset = floorDelta * 8;
			if (wall->flags1 & WF1_TEX_ANCHORED)
			{
				if (wall->nextSector)
				{
					wall->botOffset.z -= textureOffset;
				}
				else
				{
					wall->midOffset.z -= textureOffset;
				}
			}
			if (wall->flags1 & WF1_SIGN_ANCHORED)
			{
				wall->signOffset.z -= textureOffset;
			}
		}
	}

	void sector_adjustTextureMirrorOffsets_Floor(RSector* sector, fixed16_16 floorDelta)
	{
		RWall* wall = sector->walls;
		s32 wallCount = sector->wallCount;
		for (s32 i = 0; i < wallCount; i++, wall++)
		{
			RWall* mirror = wall->mirrorWall;
			if (mirror)
			{
				mirror->sector->dirtyFlags |= SDF_WALL_OFFSETS;

				fixed16_16 textureOffset = -floorDelta * 8;
				if (mirror->flags1 & WF1_TEX_ANCHORED)
				{
					if (mirror->nextSector)
					{
						mirror->botOffset.z -= textureOffset;
					}
					else
					{
						mirror->midOffset.z -= textureOffset;
					}
				}
				if (mirror->flags1 & WF1_SIGN_ANCHORED)
				{
					mirror->signOffset.z -= textureOffset;
				}
			}
		}
	}

	void sector_addObject(RSector* sector, SecObject* obj)
	{
		// TODO
		if (sector != obj->sector)
		{

		}
	}
	
	// Use the floating point point inside polygon algorithm.
	RSector* sector_which3D(fixed16_16 dx, fixed16_16 dy, fixed16_16 dz)
	{
		fixed16_16 ix = dx;
		fixed16_16 iz = dz;
		fixed16_16 y = dy;
		
		RSector* sector = s_sectors;
		RSector* foundSector = nullptr;
		s32 sectorUnitArea = 0;
		s32 prevSectorUnitArea = INT_MAX;

		for (u32 i = 0; i < s_sectorCount; i++, sector++)
		{
			if (y >= sector->ceilingHeight && y <= sector->floorHeight)
			{
				const fixed16_16 sectorMaxX = sector->boundsMax.x;
				const fixed16_16 sectorMinX = sector->boundsMin.x;
				const fixed16_16 sectorMaxZ = sector->boundsMax.z;
				const fixed16_16 sectorMinZ = sector->boundsMin.z;

				const s32 dxInt = floor16(sectorMaxX - sectorMinX) + 1;
				const s32 dzInt = floor16(sectorMaxZ - sectorMinZ) + 1;
				sectorUnitArea = dzInt * dxInt;
				
				s32 insideBounds = 0;
				if (ix >= sectorMinX && ix <= sectorMaxX && iz >= sectorMinZ && iz <= sectorMaxZ && sector_pointInside(sector, ix, iz))
				{
					// pick the containing sector with the smallest area.
					if (sectorUnitArea < prevSectorUnitArea)
					{
						prevSectorUnitArea = sectorUnitArea;
						foundSector = sector;
					}
				}
			}
		}

		return foundSector;
	}

	// Uses the "Winding Number" test for a point in polygon.
	// The point is considered inside if the winding number is greater than 0.
	// Note that this is different than DF's "crossing" algorithm.
	// TODO: Maybe? Replace algorithms.
	bool sector_pointInside(RSector* sector, fixed16_16 x, fixed16_16 z)
	{
		RWall* wall = sector->walls;
		s32 wallCount = sector->wallCount;
		s32 wn = 0;

		const Vec2f point = { fixed16ToFloat(x), fixed16ToFloat(z) };
		for (s32 w = 0; w < wallCount; w++, wall++)
		{
			vec2_fixed* w1 = wall->w0;
			vec2_fixed* w0 = wall->w1;

			Vec2f p0 = { fixed16ToFloat(w0->x), fixed16ToFloat(w0->z) };
			Vec2f p1 = { fixed16ToFloat(w1->x), fixed16ToFloat(w1->z) };

			if (p0.z <= z)
			{
				// Upward crossing, if the point is left of the edge than it intersects.
				if (p1.z > z && isLeft(p0, p1, point) > 0)
				{
					wn++;
				}
			}
			else
			{
				// Downward crossing, if point is right of the edge it intersects.
				if (p1.z <= z && isLeft(p0, p1, point) < 0)
				{
					wn--;
				}
			}
		}

		// The point is only outside if the winding number is less than or equal to 0.
		return wn > 0;
	}

	//////////////////////////////////////////////////////////
	// Internal
	//////////////////////////////////////////////////////////
	void sector_computeWallDirAndLength(RWall* wall)
	{
		vec2_fixed* w0 = wall->w0;
		vec2_fixed* w1 = wall->w1;
		fixed16_16 dx = w1->x - w0->x;
		fixed16_16 dz = w1->z - w0->z;
		wall->length = computeDirAndLength(dx, dz, &wall->wallDir.x, &wall->wallDir.z);
		wall->angle = vec2ToAngle(dx, dz);
	}

	void sector_moveWallVertex(RWall* wall, fixed16_16 offsetX, fixed16_16 offsetZ)
	{
		// Offset vertex 0.
		wall->w0->x += offsetX;
		wall->w0->z += offsetZ;
		// Update the wall direction and length.
		sector_computeWallDirAndLength(wall);

		// Set the appropriate game value if the player is inside the sector.
		RSector* sector = wall->sector;
		if (sector->flags1 & SEC_FLAGS1_PLAYER)
		{
			s_playerSecMoved = 0xffffffff;
		}

		// Update the previous wall, since it would have changed as well.
		RWall* nextWall = nullptr;
		if (wall->id == 0)
		{
			s32 last = sector->wallCount - 1;
			nextWall = &sector->walls[last];
		}
		else
		{
			nextWall = wall - 1;
		}
		// Compute the wall direction and length of the previous wall.
		sector_computeWallDirAndLength(nextWall);
	}

	// returns 0 if the object does NOT overlap, otherwise non-zero.
	// objSide: 0 = no overlap, -1/+1 front or behind.
	u32 sector_objOverlapsWall(RWall* wall, SecObject* obj, s32* objSide)
	{
		fixed16_16 halfWidth = (obj->worldWidth - SIGN_BIT(obj->worldWidth)) >> 1;
		fixed16_16 quarterWidth = (obj->worldWidth - SIGN_BIT(obj->worldWidth)) >> 2;
		fixed16_16 threeQuartWidth = halfWidth + quarterWidth;
		*objSide = 0;

		RSector* next = wall->nextSector;
		if (next)
		{
			RSector* sector = wall->sector;
			if (obj->posWS.y <= sector->floorHeight)
			{
				fixed16_16 objTop = obj->posWS.y - obj->worldHeight;
				if (objTop > sector->ceilingHeight)
				{
					return 0;
				}
			}
		}

		vec2_fixed* w0 = wall->w0;
		fixed16_16 x0 = w0->x;
		fixed16_16 z0 = w0->z;
		fixed16_16 dirX = wall->wallDir.z;
		fixed16_16 dirZ = wall->wallDir.x;
		fixed16_16 len = wall->length;
		fixed16_16 dx = obj->posWS.x - x0;
		fixed16_16 dz = obj->posWS.z - z0;

		fixed16_16 proj = mul16(dx, dirZ) + mul16(dz, dirX);
		fixed16_16 maxS = threeQuartWidth * 2 + len;	// 1.5 * width + length
		fixed16_16 s = threeQuartWidth + proj;
		if (s <= maxS)
		{
			s32 diff = mul16(dx, dirX) - mul16(dz, dirZ);
			s32 side = (diff >= 0) ? 1 : -1;

			*objSide = side;
			if (side < 0)
			{
				diff = -diff;
			}
			if (diff <= threeQuartWidth)
			{
				return 0xffffffff;
			}
		}
		return 0;
	}

	// returns 0 if the wall is free to move, else non-zero.
	u32 sector_canWallMove(RWall* wall, fixed16_16 offsetX, fixed16_16 offsetZ)
	{
		s32 objSide0;
		// Test the initial position, the code assumes there is no collision at this point.
		sector_objOverlapsWall(wall, s_playerObject, &objSide0);

		vec2_fixed* w0 = wall->w0;
		fixed16_16 z0 = w0->z;
		fixed16_16 x0 = w0->x;
		w0->x += offsetX;
		w0->z += offsetZ;

		s32 objSide1;
		// Then move the wall and test the new position.
		s32 col = sector_objOverlapsWall(wall, s_playerObject, &objSide1);

		// Restore the wall.
		w0->x = x0;
		w0->z = z0;

		if (!col)
		{
			if (objSide0 == 0 || objSide1 == 0 || objSide0 == objSide1)
			{
				return 0;
			}
		}
		return 0xffffffff;
	}

	void sector_moveObjects(RSector* sector, u32 flags, fixed16_16 offsetX, fixed16_16 offsetZ)
	{
		// TODO
		// As far as I can tell, no objects are actually affected in-game by this.
		// So I'm going to leave this empty for now and look deeper into it later once I have 
		// more information.
	}
		
	// Tests if a point (p2) is to the left, on or right of an infinite line (p0 -> p1).
	// Return: >0 p2 is on the left of the line.
	//         =0 p2 is on the line.
	//         <0 p2 is on the right of the line.
	f32 isLeft(Vec2f p0, Vec2f p1, Vec2f p2)
	{
		return (p1.x - p0.x) * (p2.z - p0.z) - (p2.x - p0.x) * (p1.z - p0.z);
	}
} // namespace TFE_Level