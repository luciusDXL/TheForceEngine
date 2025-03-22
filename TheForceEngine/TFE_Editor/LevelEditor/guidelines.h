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

namespace LevelEditor
{
	extern s32 s_hoveredGuideline;
	extern s32 s_curGuideline;

	void guideline_clearSelection();
	void guideline_delete(s32 id);

	void guideline_computeBounds(Guideline* guideline);
	void guideline_computeKnots(Guideline* guideline);
	void guideline_computeSubdivision(Guideline* guideline);
	bool guideline_getClosestPoint(const Guideline* guideline, const Vec2f& pos, s32* edgeIndex, s32* offsetIndex, f32* t, Vec2f* closestPoint = nullptr);
	Vec2f guideline_getPosition(const Guideline* guideline, s32 edgeIndex, s32 offsetIndex, f32 t);

	Vec2f snapToGridOrGuidelines2d(Vec2f pos);
}
