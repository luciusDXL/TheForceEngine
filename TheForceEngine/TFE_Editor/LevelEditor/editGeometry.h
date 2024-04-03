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
#include "selection.h"
#include "sharedState.h"

namespace LevelEditor
{
	enum HeightFlags
	{
		HFLAG_NONE = 0,
		HFLAG_SET_FLOOR = FLAG_BIT(0),
		HFLAG_SET_CEIL = FLAG_BIT(1),
		HFLAG_DEFAULT = HFLAG_SET_FLOOR | HFLAG_SET_CEIL,
	};

	enum EdgeId
	{
		EDGE0 = (0 << 8),
		EDGE1 = (1 << 8)
	};

	struct GeometryEdit
	{
		// Extrude
		bool extrudeEnabled = false;
		ExtrudePlane extrudePlane = { 0 };
		// Shape
		std::vector<Vec2f> shape;
		Polygon shapePolygon;
		// Draw
		bool drawStarted = false;
		DrawMode drawMode = DMODE_RECT;
		Vec2f drawCurPos = { 0 };
		f32 drawHeight[2] = { 0 };
		// Grid Move
		bool gridMoveStart = false;
		BoolMode boolMode = BMODE_MERGE;
	};
	extern GeometryEdit s_geoEdit;

	void editGeometry_init();

	void handleSectorDraw(RayHitInfo* hitInfo);
	void handleSectorExtrude(RayHitInfo* hitInfo);
	
	Vec3f extrudePoint2dTo3d(const Vec2f pt2d);
	Vec3f extrudePoint2dTo3d(const Vec2f pt2d, f32 height);

	void createSectorFromRect();
	void createSectorFromShape();
	s32 insertVertexIntoSector(EditorSector* sector, Vec2f newVtx);
	bool snapToLine(Vec2f& pos, f32 maxDist, Vec2f& newPos, FeatureId& snappedFeature);
}
