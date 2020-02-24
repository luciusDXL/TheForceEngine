#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Level/Map
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>

struct SpriteAngle
{
	s16 frameCount;
	s16 frameIndex[32];
};

struct SpriteAnim
{
	s32 worldWidth;
	s32 worldHeight;
	s32 frameRate;

	s32 angleCount;
	s16 rect[4];
	SpriteAngle angles[32];
};

struct Frame
{
	s32 InsertX;		// Insertion point, X coordinate
						// Negative values shift the cell left; Positive values shift the cell right
	s32 InsertY;		// Insertion point, Y coordinate
						// Negative values shift the cell up; Positive values shift the cell down
	s32 Flip;			// 0 = not flipped
						// 1 = flipped horizontally

	s32 width;
	s32 height;
	s16 rect[4];
	u8* image;
};

struct Sprite
{
	s32 animationCount;

	// Structure.
	SpriteAnim* anim;
	Frame* frames;
	u8* imageData;
	s16 rect[4];

	// Memory.
	u8* memory;
};
namespace DXL2_Sprite
{
	Frame* getFrame(const char* name);
	Sprite* getSprite(const char* name);
	void freeAll();
}
