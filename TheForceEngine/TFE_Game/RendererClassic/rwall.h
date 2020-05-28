#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "rmath.h"

struct RSector;
struct TextureFrame;

#define MAX_SEG 384
#define MAX_SPLIT_WALLS 40

// sizeof(Wall) = 164
struct RWall
{
	s32 n00;			// 0x00
	s32 y1;				// 0x04
	s32 visible;		// 0x08
	RSector* sector;	// 0x0c
	RSector* nextSector;// 0x10
	s32 n14;			// 0x14
	s32 drawFlags;		// 0x18
	s32 n1c;			// 0x1c

	// Vertices (worldspace) - guess.
	vec2* w0;	// 0x20
	vec2* w1;	// 0x24

	// Vertices (viewspace).
	vec2* v0;	// 0x28
	vec2* v1;	// 0x2c

	// Textures.
	TextureFrame* topTex;	// 0x30
	TextureFrame* midTex;	// 0x34
	TextureFrame* botTex;	// 0x38
	TextureFrame* signTex;	// 0x3c

	// Wall length in texels.
	s32 texelLength;	// 0x40	-- fixed point wall length in texels

	// Wall heights in texels.
	s32 topTexelHeight;	// 0x44
	s32 midTexelHeight;	// 0x48
	s32 botTexelHeight;	// 0x4c

	// Texture Offsets
	s32 topUOffset;		// 0x50
	s32 topVOffset;		// 0x54

	s32 midUOffset;		// 0x58
	s32 midVOffset;		// 0x5c

	s32 botUOffset;		// 0x60
	s32 botVOffset;		// 0x64

	// Unknown
	// 0x68 - 0x7c

	// Update Frame
	s32 drawFrame;		// 0x80

	// Unknown
	// 0x84

	// Flags
	u32 flags1;			// 0x88
	u32 flags2;			// 0x8c
	u32 flags3;			// 0x90

	// Unknown
	// 0x94, 0x98

	// Lighting
	s32 wallLight;		// 0x9c

	// Unknown
	// 0xa0
};

struct RWallSegment
{
	RWall* srcWall;		// 0x00
	s32 wallX0_raw;		// 0x04		-- for some reason x0, x1 values are repeated. I'm not sure why, maybe these are the unclipped values.
	s32 wallX1_raw;		// 0x08
	s32 z0; 			// 0x0C
	s32 z1;	 		    // 0x10
	s32 uCoord0;		// 0x14
	s32 x0View;			// 0x18		-- view space x position
	s32 wallX0;			// 0x1C		-- projected wall x0 (after clipping)
	s32 wallX1;			// 0x20		-- projected wall x1
	u8  orient;			// 0x24		-- which orientation to use for the slope, dx/dy or dy/dx
	u8  u2[3];			// 0x25 - 0x27
	s32 slope;			// 0x28		-- wall slope (dx/dy or dy/dx)
	s32 uScale;			// 0x2C		-- I'm not sure exactly what this does but it is used to compute the u coordinate from x coordinate.
};

namespace RClassicWall
{
	void wall_process(RWall* wall);
	s32  wall_mergeSort(RWallSegment* segOutList, s32 availSpace, s32 start, s32 count);
	void wall_drawSolid(RWallSegment* wallSegment);
}
