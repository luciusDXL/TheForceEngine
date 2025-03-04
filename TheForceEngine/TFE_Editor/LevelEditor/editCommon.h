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
	// Helper sector list to track changes.
	extern SectorList s_sectorChangeList;
	extern std::vector<s32> s_idList;
	// General movement.
	extern bool s_moveStarted;
	extern EditorSector* s_transformWallSector;
	extern s32 s_transformWallIndex;
	extern Vec2f s_moveStartPos;
	extern Vec2f s_moveStartPos1;
	extern Vec2f* s_moveVertexPivot;
	extern Vec3f s_prevPos;
	extern Vec2f s_wallNrm;
	extern Vec2f s_wallTan;
	extern Vec3f s_rayDir;

	void handleFeatureEditInput(Vec2f worldPos, RayHitInfo* info = nullptr);
	void handleDelete(bool hasFeature);
}
