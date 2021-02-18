#pragma once
//////////////////////////////////////////////////////////////////////
// Elevator update functions.
//////////////////////////////////////////////////////////////////////
typedef s32(*InfUpdateFunc)(InfElevator* elev, s32 delta);

s32 infUpdate_moveHeights(InfElevator* elev, s32 delta)
{
	RSector* sector = elev->sector;
	s32 secHeightDelta = 0;
	s32 ceilDelta = 0;
	s32 floorDelta = 0;

	switch (elev->type)
	{
	case IELEV_MOVE_CEILING:
		ceilDelta = delta;
		break;
	case IELEV_MOVE_FLOOR:
		floorDelta = delta;
		break;
	case IELEV_MOVE_OFFSET:
		secHeightDelta = delta;
		break;
	case IELEV_MOVE_FC:
		floorDelta = delta;
		ceilDelta = delta;
		break;
	}

	s32 maxObjHeight = sector_getMaxObjectHeight(sector);
	if (maxObjHeight)
	{
		secHeightDelta;
		maxObjHeight += 0x4000;	// maxObjHeight + 0.25
		if (secHeightDelta)
		{
			s32 height = sector->floorHeight.f16_16 + sector->secHeight.f16_16;
			s32 maxObjHeightAbove = 0;
			s32 maxObjectHeightBelow = 0;
			s32 objCount = sector->objectCount;
			SecObject** objList = sector->objectList;
			for (; objCount > 0; objList++)
			{
				SecObject* obj = *objList;
				if (!obj)
				{
					continue;
				}

				objCount--;
				s32 objHeight = obj->worldHeight + ONE_16;
				// Object is below the second height
				if (obj->posWS.y.f16_16 > height)
				{
					if (objHeight > maxObjectHeightBelow)
					{
						maxObjectHeightBelow = objHeight;
					}
				}
				// Object is on or above the second height
				else
				{
					if (objHeight > maxObjHeightAbove)
					{
						maxObjHeightAbove = objHeight;
					}
				}
			}

			// The new second height after adjustment (only second height so far).
			height += secHeightDelta;
			// Difference between the base floor height and the adjusted second height.
			s32 floorDelta = sector->floorHeight.f16_16 - (height + ONE_16);
			// Difference betwen the adjusted second height and the ceiling.
			s32 ceilDelta = height - sector->ceilingHeight.f16_16;
			if (floorDelta < maxObjectHeightBelow || ceilDelta < maxObjHeightAbove)
			{
				// If there are objects in the way, set the next stop as the previous.
				elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
				floorDelta = 0;
				secHeightDelta = 0;
				ceilDelta = 0;
			}
		}
		else
		{
			s32 floorHeight = sector->floorHeight.f16_16 + floorDelta;
			s32 ceilHeight = sector->ceilingHeight.f16_16 + ceilDelta;
			s32 height = floorHeight - ceilHeight;
			if (height < maxObjHeight)
			{
				// Not sure why it needs to check again...
				s32 maxObjHeight = sector_getMaxObjectHeight(sector);
				s32 spaceNeeded = maxObjHeight + 0x4000;	// maxObjHeight + 0.25
				floorHeight = sector->floorHeight.f16_16 + floorDelta;
				ceilHeight = sector->ceilingHeight.f16_16 + ceilDelta;
				s32 spaceAvail = floorHeight - ceilHeight;

				// If the height between floor and ceiling is too small for the tallest object AND
				// If the floor is moving up or the ceiling is moving down and this is NOT a crushing sector.
				if (maxObjHeight > 0 && spaceAvail < spaceNeeded && (floorDelta < 0 || ceilDelta > 0) && !(sector->flags1 & SEC_FLAGS1_CRUSHING))
				{
					Stop* stop = inf_advanceStops(elev->stops, 0, -1);
					floorDelta = 0;
					secHeightDelta = 0;
					ceilDelta = 0;
					elev->nextStop = stop;
				}
			}
		}
	}

	if (floorDelta)
	{
		inf_adjustTextureWallOffsets_Floor(sector, floorDelta);
		inf_adjustTextureMirrorOffsets_Floor(sector, floorDelta);
	}
	sector_adjustHeights(sector, floorDelta, ceilDelta, secHeightDelta);

	// Apply adjustments to "slave" sectors.
	Slave* child = (Slave*)allocator_getHead(elev->slaves);
	while (child)
	{
		if (floorDelta)
		{
			inf_adjustTextureWallOffsets_Floor(child->sector, floorDelta);
			inf_adjustTextureMirrorOffsets_Floor(child->sector, floorDelta);
		}
		sector_adjustHeights(child->sector, floorDelta, ceilDelta, secHeightDelta);
		child = (Slave*)allocator_getNext(elev->slaves);
	}
	return *elev->value;
}

s32 infUpdate_moveWall(InfElevator* elev, s32 delta)
{
	// TODO
	return 0;
}

s32 infUpdate_rotateWall(InfElevator* elev, s32 delta)
{
	// TODO
	return 0;
}

s32 infUpdate_scrollWall(InfElevator* elev, s32 delta)
{
	// TODO
	return 0;
}

s32 infUpdate_scrollFlat(InfElevator* elev, s32 delta)
{
	// TODO
	return 0;
}

s32 infUpdate_changeAmbient(InfElevator* elev, s32 delta)
{
	RSector* sector = elev->sector;
	sector->ambient.f16_16 += delta;

	Slave* child = (Slave*)allocator_getHead(elev->slaves);
	while (child)
	{
		child->sector->ambient.f16_16 += delta;
		child = (Slave*)allocator_getNext(elev->slaves);
	}
	return sector->ambient.f16_16;
}

s32 infUpdate_changeWallLight(InfElevator* elev, s32 delta)
{
	// TODO
	return 0;
}
