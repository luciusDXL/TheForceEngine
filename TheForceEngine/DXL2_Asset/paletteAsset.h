#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Level/Map
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>

struct Palette256
{
	u32 colors[256];
};

namespace DXL2_Palette
{
	void createDefault256();
	Palette256* get256(const char* name);
	Palette256* getPalFromPltt(const char* name, const char* archivePath);
	Palette256* getDefault256();
	void freeAll();
}
