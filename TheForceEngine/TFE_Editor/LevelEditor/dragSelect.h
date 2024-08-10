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
	enum DragSelectMode
	{
		DSEL_SET = 0,
		DSEL_ADD,
		DSEL_REM,
		DSEL_COUNT
	};

	struct DragSelect
	{
		DragSelectMode mode = DSEL_SET;
		bool active = false;
		bool moved = false;
		Vec2i startPos = { 0 };
		Vec2i curPos = { 0 };
	};

	extern DragSelect s_dragSelect;
}
