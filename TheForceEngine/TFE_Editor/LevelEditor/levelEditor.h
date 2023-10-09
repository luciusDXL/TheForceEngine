#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Editor/EditorAsset/editorAsset.h>

namespace LevelEditor
{
	enum LevelEditMode
	{
		LEDIT_DRAW = 1,
		//LEDIT_SMART,	// vertex + wall + height "smart" edit.
		LEDIT_VERTEX,	// vertex only
		LEDIT_WALL,		// wall only in 2D, wall + floor/ceiling in 3D
		LEDIT_SECTOR,
		LEDIT_ENTITY
	};

	enum LevelEditFlags
	{
		LEF_NONE = 0,
		LEF_SHOW_GRID = FLAG_BIT(0),
		LEF_SHOW_LOWER_LAYERS = FLAG_BIT(1),
		LEF_SHOW_INF_COLORS = FLAG_BIT(2),
		LEF_FULLBRIGHT = FLAG_BIT(3),

		LEF_DEFAULT = LEF_SHOW_GRID | LEF_SHOW_LOWER_LAYERS | LEF_SHOW_INF_COLORS
	};

	bool init(TFE_Editor::Asset* asset);
	void destroy();

	void update();
	bool menu();
}
