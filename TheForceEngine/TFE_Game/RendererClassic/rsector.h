#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include "rmath.h"

enum SectorUpdateFlags
{
	SEC_UPDATE = (1 << 0),
	SEC_UPDATE_GEO = (1 << 1),

	SEC_UPDATE_ALL = SEC_UPDATE | SEC_UPDATE_GEO
};

struct Sector;
struct SectorWall;
struct Texture;

struct RWall;
struct SecObject;
struct TextureFrame;

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
	fixed16 floorHeight;	// floor height (Y); -Y up, larger = lower.
	fixed16 ceilingHeight;	// ceiling height (Y); -Y up, smaller = higher.
	fixed16 secHeight;		// second height; equals floor height if second height in the data = 0.
	fixed16 ambientFixed;	// sector ambient in fixed point in the range of [0.0, MAX_LIGHT_LEVEL]

	// Collision heights.
	fixed16 colFloorHeight;
	fixed16 colCeilHeight;
	fixed16 colSecHeight;
	fixed16 colMinHeight;

	// Textures
	TextureFrame* floorTex;
	TextureFrame* ceilTex;

	// Texture offsets
	fixed16 floorOffsetX;
	fixed16 floorOffsetZ;
	fixed16 ceilOffsetX;
	fixed16 ceilOffsetZ;

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
	fixed16 minX;
	fixed16 minZ;
	fixed16 maxX;
	fixed16 maxZ;
};

namespace RClassicSector
{
	void sector_setMemoryPool(MemoryPool* memPool);
	
	void sector_allocate(u32 count);
	RSector* sector_get();
	
	void sector_update(u32 sectorId, u32 updateFlags = SEC_UPDATE_ALL);
	void sector_draw(RSector* sector);
	void sector_copy(RSector* out, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures);

	void sector_clear(RSector* sector);
	void sector_setupWallDrawFlags(RSector* sector);
	void sector_adjustHeights(RSector* sector, fixed16 floorOffset, fixed16 ceilOffset, fixed16 secondHeightOffset);
	void sector_computeBounds(RSector* sector);
}
