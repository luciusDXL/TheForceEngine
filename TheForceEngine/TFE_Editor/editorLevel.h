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
#include <TFE_Editor/editorResources.h>
#include <TFE_Game/igame.h>
#include <string>

namespace TFE_Editor
{
	void level_prepareNew();
	bool level_newLevelUi();

	const char* level_getDarkForcesSlotName(s32 index);
	s32 level_getDarkForcesSlotCount();
}
