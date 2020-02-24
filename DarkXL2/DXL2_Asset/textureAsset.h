#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Level/Map
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>

enum Opacity
{
	OPACITY_NORMAL = 0,
	OPACITY_TRANS,
	OPACITY_WEAPON,
	OPACITY_COUNT
};

struct TextureFrame
{
	s16 width;
	s16 height;

	s16 uvWidth;	// width and height to use when computing uvs.
	s16 uvHeight;

	u8 logSizeY;
	u8 pad[3];

	Opacity opacity;
	u8* image;
};

struct Texture
{
	char name[32];

	u16 frameCount;		// 1 for regular textures.
	u16 frameRate;

	u8* memory;
	TextureFrame* frames;
};

namespace DXL2_Texture
{
	Texture* get(const char* name);
	void free(Texture* texture);
	void freeAll();
}
