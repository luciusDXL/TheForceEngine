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
	enum LevelEditorFlags
	{
		LEVEDITOR_FLAG_RUN_TFE = FLAG_BIT(0),
		LEVEDITOR_FLAG_NO_ENEMIES = FLAG_BIT(1),
		LEVEDITOR_FLAG_EASY = FLAG_BIT(2),
		LEVEDITOR_FLAG_HARD = FLAG_BIT(3),
	};

	struct EditorConfig
	{
		char editorPath[TFE_MAX_PATH] = "";
		char exportPath[TFE_MAX_PATH] = "";
		s32 fontScale = 100;
		s32 thumbnailSize = 64;
		// Level editor
		s32 interfaceFlags = 0;
		f32 curve_segmentSize = 2.0f;
		bool waitForPlayCompletion = true;
		char darkForcesPort[TFE_MAX_PATH] = "";
		char outlawsPort[TFE_MAX_PATH] = "";
		char darkForcesAddCmdLine[TFE_MAX_PATH] = "";
		char outlawsAddCmdLine[TFE_MAX_PATH] = "";
		u32 levelEditorFlags = LEVEDITOR_FLAG_RUN_TFE;
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
