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
		s_grid.origin = { 0 };
		s_grid.axis[0] = { 1.0f, 0.0f };
		s_grid.axis[1] = { 0.0f, 1.0f };
	}

	void alignToEdge(Vec2f v0, Vec2f v1)
	{
		const Vec2f xAxis = { v1.x - v0.x, v1.z - v0.z };
		s_grid.origin = v0;
		s_grid.axis[0] = TFE_Math::normalize(&xAxis);
		s_grid.axis[1] = { -s_grid.axis[0].z, s_grid.axis[0].x };
	}

	Vec2f posToGrid(Vec2f pos)
	{
		const Vec2f offset = { pos.x - s_grid.origin.x, pos.z - s_grid.origin.z };
		const Vec2f gridPos =
		{
			offset.x * s_grid.axis[0].x + offset.z * s_grid.axis[0].z,
			offset.x * s_grid.axis[1].x + offset.z * s_grid.axis[1].z
		};
		return gridPos;
	}

	Vec3f posToGrid(Vec3f pos)
	{
		const Vec2f offset = { pos.x - s_grid.origin.x, pos.z - s_grid.origin.z };
		const Vec3f gridPos =
		{
			offset.x * s_grid.axis[0].x + offset.z * s_grid.axis[0].z,
			pos.y,
			offset.x * s_grid.axis[1].x + offset.z * s_grid.axis[1].z
		};
		return gridPos;
	}

	Vec2f gridToPos(Vec2f gridPos)
	{
		const Vec2f pos =
		{
			gridPos.x * s_grid.axis[0].x + gridPos.z * s_grid.axis[1].x + s_grid.origin.x,
			gridPos.x * s_grid.axis[0].z + gridPos.z * s_grid.axis[1].z + s_grid.origin.z,
		};
		return pos;
	}

	Vec3f gridToPos(Vec3f gridPos)
	{
		const Vec3f pos =
		{
			gridPos.x * s_grid.axis[0].x + gridPos.z * s_grid.axis[1].x + s_grid.origin.x,
			gridPos.y,
			gridPos.x * s_grid.axis[0].z + gridPos.z * s_grid.axis[1].z + s_grid.origin.z,
		};
		return pos;
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
			pos->x = floorf(pos->x / s_grid.size + 0.5f) * s_grid.size;
			pos->z = floorf(pos->z / s_grid.size + 0.5f) * s_grid.size;
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
			pos->x = floorf(pos->x / s_grid.size + 0.5f) * s_grid.size;
			pos->z = floorf(pos->z / s_grid.size + 0.5f) * s_grid.size;
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

		// Determine which projection we are using (XY or ZY).
		// This should match the way grids are rendered on surfaces.
		const f32 dx = fabsf(g1.x - g0.x);
		const f32 dz = fabsf(g1.z - g0.z);
		f32 s;
		if (dx >= dz)  // X-intersect with line segment.
		{
			snapToGrid(&pos.x);
			s = (pos.x - v0->x) / (v1->x - v0->x);
		}
		else  // Z-intersect with line segment.
		{
			snapToGrid(&pos.z);
			s = (pos.z - v0->z) / (v1->z - v0->z);
		}
		pos = { v0->x + s * (v1->x - v0->x), pos.y, v0->z + s * (v1->z - v0->z) };
		snapToGrid(&pos.y);
	}

	f32 snapToEdgeGrid(Vec2f v0, Vec2f v1, Vec2f& pos)
	{
		// Determine which projection we are using (XY or ZY).
		// This should match the way grids are rendered on surfaces.
		const f32 dx = fabsf(v1.x - v0.x);
		const f32 dz = fabsf(v1.z - v0.z);
		f32 s;
		if (dx >= dz)  // X-intersect with line segment.
		{
			snapToGrid(&pos.x);
			s = (pos.x - v0.x) / (v1.x - v0.x);
		}
		else  // Z-intersect with line segment.
		{
			snapToGrid(&pos.z);
			s = (pos.z - v0.z) / (v1.z - v0.z);
		}
		pos = { v0.x + s * (v1.x - v0.x), v0.z + s * (v1.z - v0.z) };
		return s;
	}

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
}
