#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "levelEditorData.h"
#include <vector>
#include <string>

namespace LevelEditor
{
	void findHoveredEntity2d(Vec2f worldPos);
	void handleEntityInsert(Vec3f worldPos, RayHitInfo* info = nullptr);
	void entityComputeDragSelect();

	void addEntityToSector(EditorSector* sector, Entity* entity, Vec3f* pos);
	void handleEntityEdit(RayHitInfo* hitInfo, const Vec3f& rayDir);
}
