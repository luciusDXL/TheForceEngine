#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>
#include <string>

#define UI_SCALE(x) f32((x) * TFE_Editor::s_editorConfig.fontScale / 100)

namespace TFE_Editor
{
	struct EditorConfig
	{
		char editorPath[TFE_MAX_PATH] = "";
		char exportPath[TFE_MAX_PATH] = "";
		s32 fontScale = 100;
		s32 thumbnailSize = 64;
		// Level editor
		f32 curve_segmentSize = 2.0f;
	};
	enum EditorFontConst
	{
		FONT_SIZE_COUNT = 5,
	};

	bool configSetupRequired();
	bool loadConfig();
	bool saveConfig();
	bool configUi();

	s32 fontScaleToIndex(s32 scale);
	s32 fontIndexToScale(s32 index);

	extern EditorConfig s_editorConfig;
}
