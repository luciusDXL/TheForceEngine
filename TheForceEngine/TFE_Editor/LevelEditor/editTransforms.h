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

	enum MoveAxis
	{
		AXIS_X = FLAG_BIT(0),
		AXIS_Y = FLAG_BIT(1),
		AXIS_Z = FLAG_BIT(2),
		AXIS_XZ = AXIS_X | AXIS_Z,
		AXIS_XYZ = AXIS_X | AXIS_Y | AXIS_Z
	};

	void edit_setTransformMode(TransformMode mode = TRANS_MOVE);
	TransformMode edit_getTransformMode();
	void edit_enableMoveTransform(bool enable = false);
	void edit_setTransformChange();
	// Only do this is specific cases.
	void edit_applyTransformChange();

	void edit_transform(s32 mx = 0, s32 my = 0);
	Vec3f edit_getTransformAnchor();
	Vec3f edit_getTransformPos();
	void edit_setTransformAnchor(Vec3f anchor);
	void edit_setTransformPos(Vec3f pos);
	u32 edit_getMoveAxis();

	// Tools
	bool edit_isTransformToolActive();
	bool edit_interactingWithGizmo();
	bool edit_gizmoChangedGeo();
	void edit_resetGizmo();
	Vec3f edit_gizmoCursor3d();
	Vec3f edit_getTransformRotation();
	u32 edit_getTransformRotationHover();
	f32 edit_getRotationGizmoAngle();
	f32 edit_getRotationGizmoAngleDegrees();
}
