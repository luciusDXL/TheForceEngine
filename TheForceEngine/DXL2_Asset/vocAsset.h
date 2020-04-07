#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Level/Map
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

	s32 getIndex(const char* name);
	SoundBuffer* getFromIndex(s32 index);
};
