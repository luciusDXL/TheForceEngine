#pragma once
//////////////////////////////////////////////////////////////////////
// GPU 2D antialiased line drawing
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

struct Grid
{
	Vec2f origin = { 0 };
	Vec2f axis[2] = { {1.0f, 0.0f}, {0.0f, 1.0f} };
	f32 size = 1.0f;
	f32 height = 0.0f;
};

inline void resetGrid(Grid& grid)
{
	grid.origin = { 0 };
	grid.axis[0] = { 1.0f, 0.0f };
	grid.axis[1] = { 0.0f, 1.0f };
}

inline Vec2f posToGrid(const Grid& grid, Vec2f pos)
{
	const Vec2f offset = { pos.x - grid.origin.x, pos.z - grid.origin.z };
	const Vec2f gridPos =
	{
		offset.x * grid.axis[0].x + offset.z * grid.axis[0].z,
		offset.x * grid.axis[1].x + offset.z * grid.axis[1].z
	};
	return gridPos;
}

inline Vec3f posToGrid(const Grid& grid, Vec3f pos)
{
	const Vec2f offset = { pos.x - grid.origin.x, pos.z - grid.origin.z };
	const Vec3f gridPos =
	{
		offset.x * grid.axis[0].x + offset.z * grid.axis[0].z,
		pos.y,
		offset.x * grid.axis[1].x + offset.z * grid.axis[1].z
	};
	return gridPos;
}

inline Vec2f gridToPos(const Grid& grid, Vec2f gridPos)
{
	const Vec2f pos =
	{
		gridPos.x * grid.axis[0].x + gridPos.z * grid.axis[1].x + grid.origin.x,
		gridPos.x * grid.axis[0].z + gridPos.z * grid.axis[1].z + grid.origin.z,
	};
	return pos;
}

inline Vec3f gridToPos(const Grid& grid, Vec3f gridPos)
{
	const Vec3f pos =
	{
		gridPos.x * grid.axis[0].x + gridPos.z * grid.axis[1].x + grid.origin.x,
		gridPos.y,
		gridPos.x * grid.axis[0].z + gridPos.z * grid.axis[1].z + grid.origin.z,
	};
	return pos;
}
