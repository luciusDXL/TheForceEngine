#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include "../rsector.h"
#include "../rmath.h"

struct Sector;
struct SectorWall;
struct Texture;

struct RWall;
struct SecObject;
struct TextureFrame;

namespace TFE_JediRenderer
{
	class TFE_Sectors_Fixed : public TFE_Sectors
	{
	public:
		// Sub-Renderer specific
		void update(u32 sectorId, u32 updateFlags = SEC_UPDATE_ALL) override;
		void draw(RSector* sector) override;
		void copy(RSector* out, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures) override;

		void setupWallDrawFlags(RSector* sector) override;
		void adjustHeights(RSector* sector, fixed16_16 floorOffset, fixed16_16 ceilOffset, fixed16_16 secondHeightOffset) override;
		void computeBounds(RSector* sector) override;

	private:
		void saveValues(s32 index);
		void restoreValues(s32 index);
		void adjoin_computeWindowBounds(EdgePair* adjoinEdges);
	};
}  // TFE_JediRenderer
