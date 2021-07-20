#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Level/core_math.h>
#include <TFE_Level/rsector.h>
#include "rlimits.h"

struct TextureFrame;
struct RWall;
struct SecObject;

namespace TFE_JediRenderer
{
	enum SectorUpdateFlags
	{
		SEC_UPDATE = (1 << 0),
		SEC_UPDATE_GEO = (1 << 1),
		SEC_UPDATE_ALL = SEC_UPDATE | SEC_UPDATE_GEO
	};
			
	// Values saved at a given level of the stack while traversing sectors.
	// This is 80 bytes in the original DOS code but will be more in The Force Engine
	// due to 64 bit pointers.
	struct SectorSaveValues
	{
		RSector* curSector;
		RSector* prevSector;
		void*   depth1d;
		u32     windowX0;
		u32     windowX1;
		s32     windowMinY;
		s32     windowMaxY;
		s32     windowMaxCeil;
		s32     windowMinFloor;
		s32     wallMaxCeilY;
		s32     wallMinFloorY;
		u32     windowMinX;
		u32     windowMaxX;
		s32*    windowTop;
		s32*    windowBot;
		s32*    windowTopPrev;
		s32*    windowBotPrev;
		s32     sectorAmbient;
		s32     scaledAmbient;
		s32     sectorAmbientFraction;
	};

	struct EdgePair;

	class TFE_Sectors
	{
	public:
		// Common
		void setMemoryPool(MemoryPool* memPool);
		void allocate(u32 count);
		void copyFrom(const TFE_Sectors* src);

		RSector* get();
		u32 getCount();

		void clear(RSector* sector);
		void computeAdjoinWindowBounds(EdgePair* adjoinEdges);

		// Sub-Renderer specific
		virtual void draw(RSector* sector) = 0;
		virtual void subrendererChanged() = 0;

		// Tests if a point (p2) is to the left, on or right of an infinite line (p0 -> p1).
		// Return: >0 p2 is on the left of the line.
		//         =0 p2 is on the line.
		//         <0 p2 is on the right of the line.
		inline f32 isLeft(Vec2f p0, Vec2f p1, Vec2f p2)
		{
			return (p1.x - p0.x) * (p2.z - p0.z) - (p2.x - p0.x) * (p1.z - p0.z);
		}

	protected:
		SectorSaveValues s_sectorStack[MAX_ADJOIN_DEPTH];

		RSector* s_curSector;
		RSector* s_rsectors;
		MemoryPool* s_memPool;
		SecObject* s_objBuffer[MAX_VIEW_OBJ_COUNT];
		u32 s_sectorCount;
	};
}
