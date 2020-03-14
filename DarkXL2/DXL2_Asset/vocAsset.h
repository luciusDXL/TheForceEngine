#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Level/Map
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>

struct SoundBuffer;

namespace DXL2_VocAsset
{
	SoundBuffer* get(const char* name);
	void freeAll();
};
