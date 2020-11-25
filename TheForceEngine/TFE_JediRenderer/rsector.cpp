#include "rsector.h"
#include "redgePair.h"
#include "robject.h"
#include "rcommon.h"

namespace TFE_JediRenderer
{
	void TFE_Sectors::setMemoryPool(MemoryPool* memPool)
	{
		s_memPool = memPool;
	}

	void TFE_Sectors::allocate(u32 count)
	{
		s_sectorCount = count;
		s_rsectors = (RSector*)s_memPool->allocate(sizeof(RSector) * count);
	}

	void TFE_Sectors::copyFrom(const TFE_Sectors* src)
	{
		if (!src) { return; }
		s_curSector = src->s_curSector;
		s_rsectors = src->s_rsectors;
		s_memPool = src->s_memPool;
		s_sectorCount = src->s_sectorCount;
	}

	RSector* TFE_Sectors::get()
	{
		return s_rsectors;
	}

	u32 TFE_Sectors::getCount()
	{
		return s_sectorCount;
	}

	s32 TFE_Sectors::wallSortX(const void* r0, const void* r1)
	{
		return ((const RWallSegment*)r0)->wallX0 - ((const RWallSegment*)r1)->wallX0;
	}
		
	void TFE_Sectors::clear(RSector* sector)
	{
		sector->vertexCount = 0;
		sector->wallCount = 0;
		sector->objectCount = 0;
		sector->secHeight.f32 = 0;
		sector->id = 0;
		sector->prevDrawFrame = 0;
		sector->objectCapacity = 0;
		sector->verticesWS = nullptr;
		sector->verticesVS = nullptr;
		sector->self = sector;
	}

	void TFE_Sectors::computeAdjoinWindowBounds(EdgePair* adjoinEdges)
	{
		s32 yC = adjoinEdges->yPixel_C0;
		if (yC > s_windowMinY)
		{
			s_windowMinY = yC;
		}
		s32 yF = adjoinEdges->yPixel_F0;
		if (yF < s_windowMaxY)
		{
			s_windowMaxY = yF;
		}
		yC = adjoinEdges->yPixel_C1;
		if (yC > s_windowMaxCeil)
		{
			s_windowMaxCeil = yC;
		}
		yF = adjoinEdges->yPixel_F1;
		if (yF < s_windowMinFloor)
		{
			s_windowMinFloor = yF;
		}
		s_wallMaxCeilY = s_windowMinY - 1;
		s_wallMinFloorY = s_windowMaxY + 1;
		s_windowMinX = adjoinEdges->x0;
		s_windowMaxX = adjoinEdges->x1;
		s_windowTopPrev = s_windowTop;
		s_windowBotPrev = s_windowBot;
		s_prevSector = s_curSector;
	}

	void TFE_Sectors::removeObject(RSector* sector, SecObject* objToDel)
	{
		if (sector != objToDel->sector)
		{
			SecObject** obj = sector->objectList;
			for (s32 i = sector->objectCount - 1; i >= 0; i--, obj++)
			{
				SecObject* curObj = *obj;
				while (!curObj)
				{
					obj++;
					curObj = *obj;
				}

				if (curObj == objToDel)
				{
					*obj = nullptr;
					sector->objectCount--;
					return;
				}
			}
		}
	}

	void TFE_Sectors::addObject(RSector* sector, SecObject* obj)
	{
		if (sector != obj->sector)
		{
			if (obj->sector)
			{
				removeObject(sector, obj);
			}
			/*
			if (obj->typeFlags & OTFLAG_PLAYER)	// 128<<24 = (1 << 31) -> Player
			{
			}
			*/
		}
		// TODO: Handle player stuff.

		s32 objCount = sector->objectCount;
		s32 objectCapacity = sector->objectCapacity;
		if (objCount == objectCapacity)
		{
			SecObject** list;
			if (!objectCapacity)
			{
				// allocate 20 / 4 = 5
				list = (SecObject**)malloc(sizeof(SecObject*) * 5);
				sector->objectList = list;
			}
			else
			{
				sector->objectList = (SecObject**)realloc(sector->objectList, sizeof(SecObject*) * (objectCapacity + 5));
				list = sector->objectList + objectCapacity;
			}
			memset(list, 0, sizeof(SecObject*) * 5);
			sector->objectCapacity += 5;
		}

		SecObject** list = sector->objectList;
		if (sector->objectCapacity > 0)
		{
			for (s32 i = 0; i < sector->objectCapacity; i++, list++)
			{
				if (!(*list))
				{
					*list = obj;
					obj->index = i;
					sector->objectCount++;
					obj->sector = sector;
					break;
				}
			}
		}
	}
}  // TFE_JediRenderer
