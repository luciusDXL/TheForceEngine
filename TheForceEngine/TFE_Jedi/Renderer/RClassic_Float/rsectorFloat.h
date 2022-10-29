#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include "rwallFloat.h"
#include "rflatFloat.h"
#include "../rsectorRender.h"

struct RWall;
struct SecObject;

namespace TFE_Jedi
{
	struct SectorCached
	{
		RSector* sector;		// base sector.
		WallCached* cachedWalls;
		s32 objectCapacity;
		// Floating point version of view space vertices.
		vec2_float* verticesVS;
		// Space for floating point positions.
		vec3_float* objPosVS;
		// Cached floor and ceiling heights (second height not required for rendering).
		f32 floorHeight;
		f32 ceilingHeight;
		// Cached Texture offsets
		vec2_float floorOffset;
		vec2_float ceilOffset;
	};

	class TFE_Sectors_Float : public TFE_Sectors
	{
	public:
		TFE_Sectors_Float() : m_cachedSectors(nullptr), m_cachedSectorCount(0) {}

		// Sub-Renderer specific
		void destroy() override;
		void reset() override;
		void prepare() override;
		void draw(RSector* sector) override;
		void subrendererChanged() override;

	private:
		void saveValues(s32 index);
		void restoreValues(s32 index);
		void adjoin_computeWindowBounds(EdgePairFloat* adjoinEdges);
		void adjoin_setupAdjoinWindow(s32* winBot, s32* winBotNext, s32* winTop, s32* winTopNext, EdgePairFloat* adjoinEdges, s32 adjoinCount);

		void freeCachedData();
		void allocateCachedData();
		void updateCachedSector(SectorCached* cached, u32 flags);
		void updateCachedWalls(SectorCached* cached, u32 flags);

	public:
		SectorCached* m_cachedSectors = nullptr;
		u32 m_cachedSectorCount = 0;
	};
}  // TFE_Jedi
