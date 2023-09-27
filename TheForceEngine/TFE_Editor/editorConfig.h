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

namespace TFE_Editor
{
	struct EditorConfig
	{
		char editorPath[TFE_MAX_PATH] = "";
		char exportPath[TFE_MAX_PATH] = "";
		s32 fontScale = 100;
		s32 thumbnailSize = 64;
	};

	bool configSetupRequired();
	bool loadConfig();
	bool saveConfig();
	bool configUi();

	extern EditorConfig s_editorConfig;
}
