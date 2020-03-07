#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Physics
// Handles "physics" interactions in "3D" space, such as collision
// detection, stepping, falling, moving sectors, etc.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Asset/levelAsset.h>

struct GameObject;

static const f32 c_playerRadius = 1.36f;	// Tuned based on texture width, so might need more tuning if FOV changes at all.

enum RayHitPart
{
	// Wall
	HIT_PART_MID = 0,
	HIT_PART_BOT,
	HIT_PART_TOP,
	// Flats
	HIT_PART_FLOOR,
	HIT_PART_CEIL,
	// Unknown
	HIT_PART_UNKNOWN
};

struct Ray
{
	Vec3f origin;
	Vec3f dir;
	s32   originSectorId;
	f32   maxDist;
	bool  objSelect = true;
	s32   layer = -256;
};

struct RayHitInfo
{
	Vec3f hitPoint;
	s32 hitSectorId;
	s32 hitWallId;
	RayHitPart hitPart;
	const GameObject* obj;
};

// Ray hits in order from closest to farthest.
struct MultiRayHitInfo
{
	u32 hitCount;
	// Wall hits
	u16 hitSectorId[16];
	u16 wallHitId[16];
	Vec3f hitPoint[16];

	// Sectors
	u32 sectorCount;
	u16 sectors[16];
};

namespace DXL2_Physics
{
	bool init(LevelData* level);
	void shutdown();

	bool move(const Vec3f* startPoint, const Vec3f* desiredMove, s32 curSectorId, Vec3f* actualMove, s32* newSectorId, f32 height, bool sendInfMsg = true);
	void getValidHeightRange(const Vec3f* pos, s32 curSectorId, f32* floorHeight, f32* visualFloorHeight, f32* ceilHeight);
	void getOverlappingLinesAndSectors(const Vec3f* pos, s32 curSectorId, f32 radius, u32 maxCount, u32* lines, u32* sectors, u32* outLineCount, u32* outSectorCount);
	const Sector** getOverlappingSectors(const Vec3f* pos, s32 curSectorId, f32 radius, u32* outSectorCount);

	bool correctPosition(Vec3f* pos, s32* curSectorId, f32 radius, bool sendInfMsg = true);

	s32 findSector(s32 layer, const Vec2f* pos);
	s32 findSector(const Vec3f* pos);

	// Find the closest wall to 'pos' that lies within sector 'sectorId'
	// Only accept walls within 'maxDist' and in sectors on layer 'layer'
	// Pass -1 to sectorId to choose any sector on the layer, sectorId will be set to the sector.
	// Pass -1 to layer to choose a sector on any layer.
	s32 findClosestWall(s32* sectorId, s32 layer, const Vec2f* pos, f32 maxDist);

	// Traces a ray through the level.
	// Returns true if the ray hit something.
	bool traceRay(const Ray* ray, RayHitInfo* hitInfo);

	// Gather multiple hits until the ray hits a solid surface.
	bool traceRayMulti(const Ray* ray, MultiRayHitInfo* hitInfo);
}
