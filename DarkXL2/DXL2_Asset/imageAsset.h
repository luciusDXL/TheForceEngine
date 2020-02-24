#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Level/Map
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>

struct Image
{
	u32 width = 0u;
	u32 height = 0u;
	u32* data = nullptr;
};

namespace DXL2_Image
{
	void init();
	void shutdown();

	Image* get(const char* imagePath);
	void free(Image* image);
	void freeAll();
}
