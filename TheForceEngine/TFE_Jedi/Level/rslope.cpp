#include <climits>
#include <cstring>

#include "levelData.h"
#include "rslope.h"
#include "rsector.h"
#include "rwall.h"

namespace TFE_Jedi
{
	const f32 c_angleToRadians = TWO_PI / 16384.0f;

	ChunkedArray* s_slopes = nullptr;
	void slope_computeNormalData(RSlope* slope);

	void slope_init(MemoryRegion* region)
	{
		s_slopes = TFE_Memory::createChunkedArray(sizeof(RSlope), 256, 1, region);
	}

	// Allocate a slope and store the data for later calculations.
	RSlope* slope_alloc(s32 hingeSector, s32 hingeWall, RSector* sector, f32 angle)
	{
		RSlope* slope = (RSlope*)TFE_Memory::allocFromChunkedArray(s_slopes);
		*slope = {};

		slope->planeAngle = angle;
		slope->hingeWall = (RWall*)hingeWall;
		slope->extra = hingeSector;
		slope->sector = sector;

		return slope;
	}

	// Compute the slope data.
	void slope_compute(RSlope* slope, f32 height)
	{
		// Fill in the real data.
		RSector* hingeSector = &s_levelState.sectors[slope->extra];
		const s32 hingeWall = (s32)(slope->hingeWall);
		slope->hingeWall = &hingeSector->walls[hingeWall];

		// Calculate the values.
		slope->left  = { fixed16ToFloat(slope->hingeWall->w0->x), height, fixed16ToFloat(slope->hingeWall->w0->z) };
		slope->right = { fixed16ToFloat(slope->hingeWall->w1->x), height, fixed16ToFloat(slope->hingeWall->w1->z) };
		slope_computeNormalData(slope);
	}
		
	f32 slope_getDistanceFromPlane(RSlope* slope, const vec3_float& pos)
	{
		return pos.x*slope->normal.x + pos.z*slope->normal.z + pos.y - slope->rightDotNrm;
	}

	f32 slope_getElevationAngle(RSlope* slope, const vec2_float& dirXZ)
	{
		f32 alt = slope_getElevByDirection(slope, dirXZ);
		f32 sign = alt > 0.0f ? 1.0f : -1.0f;
		return sign * atanf(sign*alt);
	}

	////////////////////////////////////
	// Internal
	////////////////////////////////////
	void slope_computeNormalData(RSlope* slope)
	{
		const f32 angleRadians = slope->planeAngle * c_angleToRadians;
		const vec3_float basePoint = slope->right;
		const vec3_float s = { slope->left.x - slope->right.x, slope->left.y - slope->right.y, slope->left.z - slope->right.z };
		// t is perpendicular to s.
		vec3_float t = { -s.z, 0.0f, s.x };
		// get another point on the plane.
		const f32 xzMag = sqrtf(s.x*s.x + s.z*s.z);
		const vec3_float p = { basePoint.x + t.x, basePoint.y + xzMag * tanf(angleRadians), basePoint.z + t.z };
		t = { p.x - basePoint.x, p.y - basePoint.y, p.z - basePoint.z };
		// compute the base normal vector.
		vec3_float n = { s.z*t.y - s.y*t.z, s.y*t.x - s.x*t.y, s.x*t.z - s.z*t.x };
		f32 rcpY = 1.0f / n.y;
		// Normal scaled so that Y = 1.
		slope->normal.x = n.x * rcpY;
		slope->normal.z = n.z * rcpY;

		// compute useful dotProd (remember normalY = 1)
		slope->rightDotNrm = basePoint.x * slope->normal.x + basePoint.y + basePoint.z * slope->normal.z;

		// basis vectors.
		normalizeVec3(&s, &slope->s);
		normalizeVec3(&t, &slope->t);
		slope->sDotRight = slope->s.x*basePoint.x + slope->s.y*basePoint.y + slope->s.z*basePoint.z;
		slope->tDotRight = slope->t.x*basePoint.x + slope->t.y*basePoint.y + slope->t.z*basePoint.z;
	}
} // namespace TFE_Jedi