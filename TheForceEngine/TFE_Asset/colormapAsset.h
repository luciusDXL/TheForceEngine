#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Level/Map
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

#define COLOR_COUNT 256
#define LIGHT_LEVELS 32
#define LIGHT_SOURCE_LEVELS 128

struct Palette256;

struct ColorMap
{
	// Color(L,C) = colorMap[COLOR_COUNT * L + C]
	// where L = light level (0 - LIGHT_LEVELS - 1)
	//       C = color (0 - COLOR_COUNT - 1)
	u8 colorMap[COLOR_COUNT * LIGHT_LEVELS];
	// Light source ramp where 0 = closest, LIGHT_SOURCE_LEVELS - 1 = farthest.
	u8 lightSourceRamp[LIGHT_SOURCE_LEVELS];
};

namespace TFE_ColorMap
{
	ColorMap* allocate(const char* name);
	ColorMap* get(const char* name);
	void freeAll();

	ColorMap* generateColorMap(const char* newFile, const Palette256* pal);
}
