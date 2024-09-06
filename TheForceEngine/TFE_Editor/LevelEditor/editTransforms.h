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
	enum TransformMode
	{
		TRANS_MOVE = 0,
		TRANS_ROTATE,
		TRANS_SCALE,
		TRANS_COUNT
	};

	enum WallMoveMode
	{
		WMM_NORMAL = 0,
		WMM_FREE,
		WMM_COUNT
	};

	void edit_setTransformMode(TransformMode mode = TRANS_MOVE);
	TransformMode edit_getTransformMode();
	void edit_enableMoveTransform(bool enable = false);
	void edit_setWallMoveMode(WallMoveMode mode);
	void edit_setTransformChange();
	// Only do this is specific cases.
	void edit_applyTransformChange();

	void edit_transform(s32 mx = 0, s32 my = 0);

	// Tools
	bool edit_isTransformToolActive();
	bool edit_interactingWithGizmo();
	bool edit_gizmoChangedGeo();
	void edit_resetGizmo();
	Vec3f edit_gizmoCursor3d();
	Vec3f edit_getTransformCenter();
	Vec3f edit_getTransformRotation();
	u32 edit_getTransformRotationHover();
	f32 edit_getRotationGizmoAngle();
	f32 edit_getRotationGizmoAngleDegrees();
}
