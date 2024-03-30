#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <string>

namespace LevelEditor
{
	void system_print(std::string& msg);
	void system_printError(std::string& msg);
	void system_printWarning(std::string& msg);
}
