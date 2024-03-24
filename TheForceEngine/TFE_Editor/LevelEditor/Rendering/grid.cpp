#include "grid.h"
#include <TFE_System/math.h>
#include <TFE_System/system.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <algorithm>
#include <vector>

namespace LevelEditor
{
	// Assume Y is always vertical.
	Grid s_grid = {};
			
	void resetGrid()
	{
		resetGrid(s_grid);
	}

	void alignGridToEdge(Vec2f v0, Vec2f v1)
	{
		const Vec2f xAxis = { v1.x - v0.x, v1.z - v0.z };
		s_grid.origin = v0;
		s_grid.axis[0] = TFE_Math::normalize(&xAxis);
		s_grid.axis[1] = { -s_grid.axis[0].z, s_grid.axis[0].x };
	}

	Vec2f posToGrid(Vec2f pos)
	{
		return posToGrid(s_grid, pos);
	}

	Vec3f posToGrid(Vec3f pos)
	{
		return posToGrid(s_grid, pos);
	}

	Vec2f gridToPos(Vec2f gridPos)
	{
		return gridToPos(s_grid, gridPos);
	}

	Vec3f gridToPos(Vec3f gridPos)
	{
		return gridToPos(s_grid, gridPos);
	}

	void snapToGrid(f32* value)
	{
		if (!TFE_Input::keyModDown(KEYMOD_ALT) && s_grid.size != 0.0f)
		{
			*value = floorf((*value) / s_grid.size + 0.5f) * s_grid.size;
		}
		else  // Snap to the finest grid.
		{
			*value = floorf((*value) * 65536.0f + 0.5f) / 65536.0f;
		}
	}

	void snapToGridY(f32* value)
	{
		if (!TFE_Input::keyModDown(KEYMOD_ALT) && s_grid.size != 0.0f)
		{
			*value = floorf((*value) / s_grid.size + 0.5f) * s_grid.size;
		}
		else  // Snap to the finest grid.
		{
			*value = floorf((*value) * 65536.0f + 0.5f) / 65536.0f;
		}
	}

	void snapToGrid(Vec2f* pos)
	{
		if (!TFE_Input::keyModDown(KEYMOD_ALT) && s_grid.size != 0.0f)
		{
			Vec2f gpos = posToGrid(*pos);
			gpos.x = floorf(gpos.x / s_grid.size + 0.5f) * s_grid.size;
			gpos.z = floorf(gpos.z / s_grid.size + 0.5f) * s_grid.size;
			*pos = gridToPos(gpos);
		}
		else  // Snap to the finest grid.
		{
			pos->x = floorf(pos->x * 65536.0f + 0.5f) / 65536.0f;
			pos->z = floorf(pos->z * 65536.0f + 0.5f) / 65536.0f;
		}
	}

	void snapToGrid(Vec3f* pos)
	{
		if (!TFE_Input::keyModDown(KEYMOD_ALT) && s_grid.size != 0.0f)
		{
			Vec2f posXZ = { pos->x, pos->z };
			Vec2f gpos = posToGrid(posXZ);
			gpos.x = floorf(gpos.x / s_grid.size + 0.5f) * s_grid.size;
			gpos.z = floorf(gpos.z / s_grid.size + 0.5f) * s_grid.size;
			posXZ = gridToPos(gpos);
			pos->x = posXZ.x;
			pos->z = posXZ.z;
		}
		else  // Snap to the finest grid.
		{
			pos->x = floorf(pos->x * 65536.0f + 0.5f) / 65536.0f;
			pos->z = floorf(pos->z * 65536.0f + 0.5f) / 65536.0f;
		}
	}

	void snapToSurfaceGrid(EditorSector* sector, EditorWall* wall, Vec3f& pos)
	{
		const Vec2f* v0 = &sector->vtx[wall->idx[0]];
		const Vec2f* v1 = &sector->vtx[wall->idx[1]];

		Vec2f g0 = posToGrid(*v0);
		Vec2f g1 = posToGrid(*v1);
		Vec3f gpos = posToGrid(pos);
		// Snap.
		gpos.x = floorf(gpos.x / s_grid.size + 0.5f) * s_grid.size;
		gpos.z = floorf(gpos.z / s_grid.size + 0.5f) * s_grid.size;

		// Determine which projection we are using (XY or ZY).
		// This should match the way grids are rendered on surfaces.
		const f32 dx = fabsf(g1.x - g0.x);
		const f32 dz = fabsf(g1.z - g0.z);
		f32 s;
		if (dx >= dz)  // X-intersect with line segment.
		{
			s = (gpos.x - g0.x) / (g1.x - g0.x);
		}
		else  // Z-intersect with line segment.
		{
			s = (gpos.z - g0.z) / (g1.z - g0.z);
		}
		pos = { v0->x + s * (v1->x - v0->x), pos.y, v0->z + s * (v1->z - v0->z) };
		snapToGridY(&pos.y);
	}

	f32 snapToEdgeGrid(Vec2f v0, Vec2f v1, Vec2f& pos)
	{
		Vec2f g0 = posToGrid(v0);
		Vec2f g1 = posToGrid(v1);
		Vec2f gpos = posToGrid(pos);
		// Snap.
		gpos.x = floorf(gpos.x / s_grid.size + 0.5f) * s_grid.size;
		gpos.z = floorf(gpos.z / s_grid.size + 0.5f) * s_grid.size;

		// Determine which projection we are using (XY or ZY).
		// This should match the way grids are rendered on surfaces.
		const f32 dx = fabsf(g1.x - g0.x);
		const f32 dz = fabsf(g1.z - g0.z);
		f32 s;
		if (dx >= dz)  // X-intersect with line segment.
		{
			s = (gpos.x - g0.x) / (g1.x - g0.x);
		}
		else  // Z-intersect with line segment.
		{
			s = (gpos.z - g0.z) / (g1.z - g0.z);
		}
		pos = { v0.x + s * (v1.x - v0.x), v0.z + s * (v1.z - v0.z) };
		return s;
	}

    // TODO: Fix for rotated grid.
	f32 snapAlongPath(const Vec2f& startPos, const Vec2f& path, const Vec2f& moveStart, f32 pathOffset)
	{
		f32 dX = FLT_MAX, dZ = FLT_MAX;
		f32 offset = FLT_MAX;
		if (fabsf(path.x) > FLT_EPSILON)
		{
			f32 nextPos = moveStart.x + path.x * pathOffset;
			snapToGrid(&nextPos);
			dX = (nextPos - moveStart.x) / path.x;
		}
		if (fabsf(path.z) > FLT_EPSILON)
		{
			f32 nextPos = moveStart.z + path.z * pathOffset;
			snapToGrid(&nextPos);
			dZ = (nextPos - moveStart.z) / path.z;
		}
		if (dX < FLT_MAX && fabsf(dX - pathOffset) < fabsf(dZ - pathOffset))
		{
			offset = dX;
		}
		else if (dZ < FLT_MAX)
		{
			offset = dZ;
		}
		return offset;
	}

	Vec3f rayGridPlaneHit(const Vec3f& origin, const Vec3f& rayDir)
	{
		Vec3f hit = { 0 };
		if (fabsf(rayDir.y) < FLT_EPSILON) { return hit; }

		f32 s = (s_grid.height - origin.y) / rayDir.y;
		if (s <= 0.0f) { return hit; }

		hit.x = origin.x + s * rayDir.x;
		hit.y = origin.y + s * rayDir.y;
		hit.z = origin.z + s * rayDir.z;
		return hit;
	}
}
