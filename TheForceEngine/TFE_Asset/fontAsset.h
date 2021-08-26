#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Level/Map
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

struct FontTFE
{
	u8 startChar;
	u8 endChar;
	u8 charCount;
	u8 height;

	u8 maxWidth;
	u8 pad[3];

	u8 width[256];
	u8 step[256];	// x step which may be different than rendered width.
	u32 imageOffset[256];

	u8* imageData;
};

namespace TFE_Font
{
	FontTFE* get(const char* name);
	FontTFE* getFromFont(const char* name, const char* archivePath);
	// create the embedded system font
	FontTFE* createSystemFont6x8();
	void freeAll();
}
