#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include "rmath.h"
#include "rlimits.h"

struct TextureFrame;

// Temporary.
struct Sector;
struct SectorWall;
struct Texture;

namespace TFE_JediRenderer
{
	enum SectorUpdateFlags
	{
		SEC_UPDATE = (1 << 0),
		SEC_UPDATE_GEO = (1 << 1),

		SEC_UPDATE_ALL = SEC_UPDATE | SEC_UPDATE_GEO
	};

	struct RWall;
	struct SecObject;

	struct RSector
	{
		RSector* self;
		s32 id;

		s32 prevDrawFrame;		// previous frame that this sector was drawn/updated.
		s32 prevDrawFrame2;		// previous frame drawn (again...)
		s32 vertexCount;		// number of vertices.
		vec2* verticesWS;		// world space and view space XZ vertex positions.
		vec2* verticesVS;
		s32 wallCount;			// number of walls.
		RWall*  walls;			// wall list.

		// Render heights.
		decimal floorHeight;	// floor height (Y); -Y up, larger = lower.
		decimal ceilingHeight;	// ceiling height (Y); -Y up, smaller = higher.
		decimal secHeight;		// second height; equals floor height if second height in the data = 0.
		decimal ambient;		// sector ambient in fixed point in the range of [0.0, MAX_LIGHT_LEVEL]

		// Collision heights.
		decimal colFloorHeight;
		decimal colCeilHeight;
		decimal colSecHeight;
		decimal colMinHeight;

		// Textures
		TextureFrame* floorTex;
		TextureFrame* ceilTex;

		// Texture offsets
		decimal floorOffsetX;
		decimal floorOffsetZ;
		decimal ceilOffsetX;
		decimal ceilOffsetZ;

		// Objects
		s32 objectCount;
		SecObject** objectList;
		s32 objectCapacity;

		// Rendering
		s32 startWall;			// wall segment start index for rendering
		s32 drawWallCnt;		// wall segment draw count for rendering
		u32 flags1;				// sector flags.
		u32 flags2;
		u32 flags3;
		s32 layer;

		// Bounds
		decimal minX;
		decimal minZ;
		decimal maxX;
		decimal maxZ;
	};

	// Values saved at a given level of the stack while traversing sectors.
	// This is 80 bytes in the original DOS code but will be more in The Force Engine
	// due to 64 bit pointers.
	struct SectorSaveValues
	{
		RSector* curSector;
		RSector* prevSector;
		decimalPtr depth1d;
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
		s32     scaledAmbient2k;
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

		static s32 wallSortX(const void* r0, const void* r1);

		// Sub-Renderer specific
		virtual void update(u32 sectorId, u32 updateFlags = SEC_UPDATE_ALL) = 0;
		virtual void draw(RSector* sector) = 0;
		virtual void copy(RSector* out, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures) = 0;

		virtual void setupWallDrawFlags(RSector* sector) = 0;
		virtual void adjustHeights(RSector* sector, decimal floorOffset, decimal ceilOffset, decimal secondHeightOffset) = 0;
		virtual void computeBounds(RSector* sector) = 0;

	protected:
		SectorSaveValues s_sectorStack[MAX_ADJOIN_DEPTH];

		RSector* s_curSector;
		RSector* s_rsectors;
		MemoryPool* s_memPool;
		u32 s_sectorCount;
	};
}
