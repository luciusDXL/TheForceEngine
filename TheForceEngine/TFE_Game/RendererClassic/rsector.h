#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include "rmath.h"

struct Sector;
struct SectorWall;
struct Texture;

struct RWall;
struct TextureFrame;

struct RSector
{
	s32 prevDrawFrame;		// previous frame that this sector was drawn/updated.
	s32 prevDrawFrame2;		// previous frame drawn (again...)
	s32 vertexCount;		// number of vertices.
	vec2* verticesWS;		// world space and view space XZ vertex positions.
	vec2* verticesVS;
	s32 wallCount;			// number of walls.
	RWall*  walls;			// wall list.
	fixed16 floorHeight;	// floor height (Y); -Y up, larger = lower.
	fixed16 ceilingHeight;	// ceiling height (Y); -Y up, smaller = higher.
	fixed16 secHeight;		// second height; equals floor height if second height in the data = 0.
	fixed16 ambientFixed;	// sector ambient in fixed point in the range of [0.0, 31.0]

	// Textures
	TextureFrame* floorTex;
	TextureFrame* ceilTex;

	// Texture offsets
	fixed16 floorOffsetX;
	fixed16 floorOffsetZ;
	fixed16 ceilOffsetX;
	fixed16 ceilOffsetZ;

	s32 startWall;			// wall segment start index for rendering
	s32 drawWallCnt;		// wall segment draw count for rendering
	u32 flags1;				// sector flags.
	u32 flags2;
	u32 flags3;
};

namespace RClassicSector
{
	void sector_setMemoryPool(MemoryPool* memPool);
	
	void sector_allocate(u32 count);
	RSector* sector_get();
	
	void sector_update(u32 sectorId);
	void sector_draw(RSector* sector);
	void sector_copy(RSector* out, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures);

	// Not for the original code, to be replaced.
	void sector_setupWallDrawFlags(RSector* sector);
}
