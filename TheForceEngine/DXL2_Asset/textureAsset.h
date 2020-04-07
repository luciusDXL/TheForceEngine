#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Level/Map
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>
#include "paletteAsset.h"

enum Opacity
{
	OPACITY_NORMAL = 0,
	OPACITY_TRANS,
	OPACITY_WEAPON,
	OPACITY_COUNT
};

enum TextureLayout
{
	TEX_LAYOUT_VERT = 0,	// default for most textures
	TEX_LAYOUT_HORZ,		// "normal" horizontal layout (used for menus, cutscenes, etc.).
};

struct TextureFrame
{
	s16 width;
	s16 height;

	s16 uvWidth;	// width and height to use when computing uvs.
	s16 uvHeight;

	s16 offsetX;
	s16 offsetY;

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

	TextureLayout layout;
};

namespace DXL2_Texture
{
	Texture* get(const char* name);
	Texture* getFromDelt(const char* name, const char* archivePath);
	Texture* getFromAnim(const char* name, const char* archivePath);
	Texture* getFromPCX(const char* name, const char* archivePath);
	Palette256* getPreviousPalette();
	void free(Texture* texture);
	void freeAll();
}
