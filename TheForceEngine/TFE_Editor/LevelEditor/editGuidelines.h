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
#include "guidelines.h"
#include "selection.h"
#include "sharedState.h"

namespace LevelEditor
{
	struct GuidelinesEdit
	{
		Guideline guidelines = {};
		// Draw
		bool drawStarted = false;
		bool shapeComplete = false;
		DrawMode drawMode = DMODE_RECT;
		Vec2f drawCurPos = { 0 };
		Vec2f controlPoint = { 0 };
	};

	void editGuidelines_init();
	void handleGuidelinesEdit(RayHitInfo* hitInfo);

	extern GuidelinesEdit s_editGuidelines;
}
