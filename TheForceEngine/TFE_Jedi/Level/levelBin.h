#pragma once
//////////////////////////////////////////////////////////////////////
// LevelBin
// Handles loading of LVB versions of levels, to handle the
// Dark Forces demo or other sources of LVB files.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <vector>

namespace TFE_Jedi
{
	bool level_loadGeometryBin(const char* levelName, std::vector<char>& buffer);
}
