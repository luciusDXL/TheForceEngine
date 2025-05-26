#pragma once
//////////////////////////////////////////////////////////////////////
// Slope
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Memory/chunkedArray.h>
#include <TFE_Jedi/Math/core_math.h>

using namespace TFE_Jedi;

struct RWall;
struct RSector;

// Structure derived from Outlaws slope.
// Note: in TFE, angle is floating point instead of integer angle.
struct RSlope
{
	RWall* hingeWall = nullptr;				// wall whose upper or lower edge is the hinge for the plane
	RSector* sector = nullptr;				// parent sector.
	f32 planeAngle = 0.0f;					// in the original code, this is an angle between 0-16384. For TFE, allow for floating point angles.
	f32 minHeight = 0.0f;					// min height of plane.
	f32 maxHeight = 0.0f;					// max height of plane.
	vec2_float normal = { 0 };				// skewed normal (y = 1.0)
	vec3_float left = { 0 }, right = { 0 };	// left and right points on hinge.
	vec3_float s = { 0 }, t = { 0 };		// s = left -> right, t = tangent
	f32 rightDotNrm = 0.0f;					// dot(left, normal)
	f32 sDotRight = 0.0f, tDotRight = 0.0f;
	s32 extra = 0;	// Used to temporarily hold data.
};

namespace TFE_Jedi
{
	void slope_init(MemoryRegion* region);
	// Allocate a slope and store the data for later calculations.
	RSlope* slope_alloc(s32 hingeSector, s32 hingeWall, RSector* sector, f32 angle);
	// Compute the slope data.
	void slope_compute(RSlope* slope, f32 height);

	// Helper functions
	f32 slope_getDistanceFromPlane(RSlope* slope, const vec3_float& pos);
	f32 slope_getElevationAngle(RSlope* slope, const vec2_float& dirXZ);

	inline f32 slope_GetHeightAtXZ(RSlope* slope, vec2_float pos) { return slope->rightDotNrm - pos.x*slope->normal.x - pos.z*slope->normal.z; }
	inline f32 slope_getElevByDirection(RSlope* slope, vec2_float dir) { return -(slope->normal.x*dir.x + slope->normal.z*dir.z); }
}
