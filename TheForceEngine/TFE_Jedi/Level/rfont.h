#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_DarkForces/time.h>

struct Font
{
	TextureData* glyphs;
	u8  minChar;
	u8  maxChar;
	u8  vertSpacing;
	u8  horzSpacing;
	u8  width;
	u8  height;
	u8  pad[2];
};

namespace TFE_Jedi
{
	Font* font_load(FilePath* filePath);
	s32 font_getStringLength(Font* font, const char *str);
}
