#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/core_math.h>

using namespace TFE_Jedi;

struct Allocator;
struct TextureData;

struct RWall;
struct SecObject;

enum SectorFlags1
{
	SEC_FLAGS1_EXTERIOR      = FLAG_BIT(0),
	SEC_FLAGS1_DOOR          = FLAG_BIT(1),
	SEC_FLAGS1_MAG_SEAL      = FLAG_BIT(2),
	SEC_FLAGS1_EXT_ADJ       = FLAG_BIT(3),
	SEC_FLAGS1_ICE_FLOOR     = FLAG_BIT(4),
	SEC_FLAGS1_SNOW_FLOOR    = FLAG_BIT(5),
	SEC_FLAGS1_EXP_WALL      = FLAG_BIT(6),
	SEC_FLAGS1_PIT           = FLAG_BIT(7),
	SEC_FLAGS1_EXT_FLOOR_ADJ = FLAG_BIT(8),
	SEC_FLAGS1_CRUSHING      = FLAG_BIT(9),
	SEC_FLAGS1_NOWALL_DRAW   = FLAG_BIT(10),
	SEC_FLAGS1_LOW_DAMAGE    = FLAG_BIT(11),
	SEC_FLAGS1_HIGH_DAMAGE   = FLAG_BIT(12),
	SEC_FLAGS1_NO_SMART_OBJ  = FLAG_BIT(13),
	SEC_FLAGS1_SMART_OBJ     = FLAG_BIT(14),
	SEC_FLAGS1_SUBSECTOR     = FLAG_BIT(15),
	SEC_FLAGS1_SAFESECTOR    = FLAG_BIT(16),
	SEC_FLAGS1_RENDERED      = FLAG_BIT(17),
	SEC_FLAGS1_PLAYER        = FLAG_BIT(18),
	SEC_FLAGS1_SECRET        = FLAG_BIT(19),
};

// Added for TFE to support floating point and GPU sub-renderers.
// Floating point or GPU based sector data should be cached and only parts updated based
// on these flags.
enum SectorDirtyFlags
{
	SDF_NONE = 0,
	// Texture Offsets
	SDF_WALL_OFFSETS = FLAG_BIT(0),
	SDF_FLAT_OFFSETS = FLAG_BIT(1),
	// Geometry
	SDF_VERTICES     = FLAG_BIT(2),
	SDF_HEIGHTS      = FLAG_BIT(3),
	SDF_WALL_SHAPE   = FLAG_BIT(4),
	// Lighting
	SDF_AMBIENT      = FLAG_BIT(5),
	// Objects
	SDF_CHANGE_OBJ   = FLAG_BIT(6),
	// Initial setup.
	SDF_INIT_SETUP   = FLAG_BIT(7),
	// Wall change flags.
	SDF_WALL_CHANGE = (SDF_INIT_SETUP | SDF_WALL_OFFSETS | SDF_WALL_SHAPE | SDF_HEIGHTS),
	// Everything.
	SDF_ALL = 0xffffffff
};

enum SectorConstants
{
	SEC_SKY_HEIGHT = FIXED(100),
};

struct RSector
{
	RSector* self;
	s32 id;
	s32 index;				// 'index' was added for TFE to help with sector caching, since id is not necessarily match the index.
	s32 prevDrawFrame;		// previous frame that this sector was drawn/updated.
	s32 prevDrawFrame2;		// previous frame drawn (again...)

	// Vertices.
	s32 vertexCount;		// number of vertices.
	vec2_fixed* verticesWS;	// world space and view space XZ vertex positions.
	vec2_fixed* verticesVS;

	// Walls.
	s32 wallCount;			// number of walls.
	RWall* walls;			// wall list.

	// Render heights.
	fixed16_16 floorHeight;		// floor height (Y); -Y up, larger = lower.
	fixed16_16 ceilingHeight;	// ceiling height (Y); -Y up, smaller = higher.
	fixed16_16 secHeight;		// second height; equals floor height if second height in the data = 0.
	fixed16_16 ambient;			// sector ambient in fixed point in the range of [0.0, MAX_LIGHT_LEVEL]

	// Collision heights.
	fixed16_16 colFloorHeight;
	fixed16_16 colCeilHeight;
	fixed16_16 colSecHeight;
	fixed16_16 colSecCeilHeight;

	// INF
	Allocator* infLink;

	// Textures
	TextureData** floorTex;
	TextureData** ceilTex;

	// Texture offsets
	vec2_fixed floorOffset;
	vec2_fixed ceilOffset;

	// Objects
	s32 objectCount;
	SecObject** objectList;
	s32 objectCapacity;

	// Collision tracking.
	s32 collisionFrame;

	// Rendering wall segment tracking.
	s32 startWall;			// wall segment start index for rendering
	s32 drawWallCnt;		// wall segment draw count for rendering

	// Flags & Layer.
	u32 flags1;				// sector flags.
	u32 flags2;
	u32 flags3;
	s32 layer;

	// Bounds
	vec2_fixed boundsMin;
	vec2_fixed boundsMax;

	// Added for TFE, to support floating point and GPU sub-renderers.
	u32 dirtyFlags;
	u32 searchKey;
};

namespace TFE_Jedi
{
	void sector_clear(RSector* sector);
	void sector_setupWallDrawFlags(RSector* sector);
	void sector_adjustHeights(RSector* sector, fixed16_16 floorOffset, fixed16_16 ceilOffset, fixed16_16 secondHeightOffset);
	void sector_computeBounds(RSector* sector);

	fixed16_16 sector_getMaxObjectHeight(RSector* sector);
	JBool sector_canMoveWalls(RSector* sector, fixed16_16 delta, fixed16_16 dirX, fixed16_16 dirZ, u32 flags);
	void  sector_changeWallLight(RSector* sector, fixed16_16 delta);
	void  sector_scrollWalls(RSector* sector, fixed16_16 offsetX, fixed16_16 offsetZ);

	void sector_adjustTextureWallOffsets_Floor(RSector* sector, fixed16_16 floorDelta);
	void sector_adjustTextureMirrorOffsets_Floor(RSector* sector, fixed16_16 floorDelta);

	void sector_addObject(RSector* sector, SecObject* obj);
	void sector_addObjectDirect(RSector* sector, SecObject* obj);
	void sector_removeObject(SecObject* obj);
	
	RSector* sector_which3D(fixed16_16 dx, fixed16_16 dy, fixed16_16 dz);
	RSector* sector_which3D_Map(fixed16_16 dx, fixed16_16 dz, s32 layer);
	bool sector_pointInside(RSector* sector, fixed16_16 x, fixed16_16 z);
	JBool sector_pointInsideDF(RSector* sector, fixed16_16 x, fixed16_16 z);

	void sector_getFloorAndCeilHeight(RSector* sector, fixed16_16* floorHeight, fixed16_16* ceilHeight);
	void sector_getObjFloorAndCeilHeight(RSector* sector, fixed16_16 y, fixed16_16* floorHeight, fixed16_16* ceilHeight);

	void sector_changeGlobalLightLevel();
	void sector_removeCorpses(RSector* sector);

	JBool sector_canRotateWalls(RSector* sector, angle14_32 angle, fixed16_16 centerX, fixed16_16 centerZ);
	void  sector_rotateWalls(RSector* sector, fixed16_16 centerX, fixed16_16 centerZ, angle14_32 angle, u32 rotateFlags);
	void  sector_rotateObjects(RSector* sector, angle14_32 deltaAngle, fixed16_16 centerX, fixed16_16 centerZ, u32 flags);
}