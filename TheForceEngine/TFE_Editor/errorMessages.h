#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <vector>

namespace TFE_Editor
{
	enum EditorError
	{
		ERROR_INVALID_EXPORT_PATH = 0,
		ERROR_COUNT,
	};

	const char* getErrorMsg(EditorError err);
}
