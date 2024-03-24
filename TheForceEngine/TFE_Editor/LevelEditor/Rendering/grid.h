#pragma once
//////////////////////////////////////////////////////////////////////
// An OpenGL system to blit a texture to the screen. Uses a hardcoded
// basic shader + fullscreen triangle.
//
// Additional optional features can be added later such as filtering,
// bloom and GPU color conversion.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Editor/LevelEditor/levelEditorData.h>
#include <TFE_RenderShared/gridDef.h>

namespace LevelEditor
{
	extern Grid s_grid;

	void resetGrid();
	void alignGridToEdge(Vec2f v0, Vec2f v1);

	Vec2f posToGrid(Vec2f pos);
	Vec3f posToGrid(Vec3f pos);
	Vec2f gridToPos(Vec2f gridPos);
	Vec3f gridToPos(Vec3f gridPos);

	void snapToGridY(f32* value);
	void snapToGrid(Vec2f* pos);
	void snapToGrid(Vec3f* pos);

	void snapToSurfaceGrid(EditorSector* sector, EditorWall* wall, Vec3f& pos);
	f32  snapToEdgeGrid(Vec2f v0, Vec2f v1, Vec2f& pos);
	f32  snapAlongPath(const Vec2f& startPos, const Vec2f& path, const Vec2f& moveStart, f32 pathOffset);

	Vec3f rayGridPlaneHit(const Vec3f& origin, const Vec3f& rayDir);
}
