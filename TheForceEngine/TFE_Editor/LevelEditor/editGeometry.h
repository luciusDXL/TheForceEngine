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
		Vec2f controlPoint = { 0 };
		f32 drawHeight[2] = { 0 };
		bool shapeComplete = false;
		// Grid Move
		bool gridMoveStart = false;
		BoolMode boolMode = BMODE_MERGE;
	};
	extern GeometryEdit s_geoEdit;

	void editGeometry_init();

	bool isInIntList(s32 value, std::vector<s32>* list);
	void insertIntoIntList(s32 value, std::vector<s32>* list);

	void handleSectorDraw(RayHitInfo* hitInfo);
	void handleSectorExtrude(RayHitInfo* hitInfo);
	
	Vec3f extrudePoint2dTo3d(const Vec2f pt2d);
	Vec3f extrudePoint2dTo3d(const Vec2f pt2d, f32 height);

	void createSectorFromRect();
	void createSectorFromShape();
	s32 insertVertexIntoSector(EditorSector* sector, Vec2f newVtx);

	bool gridCursorIntersection(Vec3f* pos);

	void buildCurve(const Vec2f& a, const Vec2f& b, const Vec2f& c, std::vector<Vec2f>* curve);
	s32  getCurveSegDelta();
	void setCurveSegDelta(s32 newDelta = 0);

	void evaluateQuadraticBezier(const Vec2f& a, const Vec2f& b, const Vec2f& c, f32 t, Vec2f* pos, Vec2f* nrm = nullptr);
	f32  signedDistQuadraticBezier(const Vec2f& p0, const Vec2f& p1, const Vec2f& pc, const Vec2f& pos, f32& t);
	f32  getQuadraticBezierArcLength(const Vec2f& a, const Vec2f& b, const Vec2f& c, f32 t = 1.0f, s32 maxIterationCount = 8, Vec2f* table = nullptr);
}
