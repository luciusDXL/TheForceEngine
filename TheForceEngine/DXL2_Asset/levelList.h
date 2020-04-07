#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Level/Map
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>

namespace DXL2_LevelList
{
	bool load();
	void unload();

	u32 getLevelCount();
	const char* getLevelName(u32 index);
	const char* getLevelFileName(u32 index);
}
