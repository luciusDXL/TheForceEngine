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
	s32 vertexCount;		// number of vertices.
	vec2* verticesWS;		// world space and view space XZ vertex positions.
	vec2* verticesVS;
	s32 wallCount;			// number of walls.
	RWall* walls;			// wall list.
	s32 floorHeight;		// floor height (Y); -Y up, larger = lower.
	s32 ceilingHeight;		// ceiling height (Y); -Y up, smaller = higher.
	s32 secHeight;			// second height; equals floor height if second height in the data = 0.
	s32 ambientFixed;		// sector ambient in fixed point in the range of [0.0, 31.0]

	// Textures
	TextureFrame* floorTex;
	TextureFrame* ceilTex;

	// Texture offsets
	s32 floorOffsetX;
	s32 floorOffsetZ;
	s32 ceilOffsetX;
	s32 ceilOffsetZ;

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
	
	void sector_setCurrent(RSector* sector);
	void sector_update(u32 sectorId);
	void sector_draw();
	void sector_copy(RSector* out, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures);
}
