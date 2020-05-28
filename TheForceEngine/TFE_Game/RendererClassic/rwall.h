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

enum WallOrient
{
	WORIENT_DZ_DX = 0,
	WORIENT_DX_DZ = 1
};

enum WallDrawFlags
{
	WDF_MIDDLE      = 0,
	WDF_TOP         = 1,
	WDF_BOT         = 2,
	WDF_TOP_AND_BOT = 3,
};

struct RWall
{
	s32 visible;		
	RSector* sector;	
	RSector* nextSector;
	s32 drawFlags;		

	// Vertices (worldspace)
	vec2* w0;
	vec2* w1;

	// Vertices (viewspace).
	vec2* v0;
	vec2* v1;

	// Textures.
	TextureFrame* topTex;
	TextureFrame* midTex;
	TextureFrame* botTex;
	TextureFrame* signTex;

	// Wall length in texels.
	s32 texelLength;

	// Wall heights in texels.
	s32 topTexelHeight;
	s32 midTexelHeight;
	s32 botTexelHeight;

	// Texture Offsets
	s32 topUOffset;
	s32 topVOffset;

	s32 midUOffset;
	s32 midVOffset;

	s32 botUOffset;
	s32 botVOffset;

	// Update Frame
	s32 drawFrame;

	// Flags
	u32 flags1;
	u32 flags2;
	u32 flags3;

	// Lighting
	s32 wallLight;
};

struct RWallSegment
{
	RWall* srcWall;		// Source wall
	s32 wallX0_raw;		// Projected wall X coordinates before adjoin/wall clipping.
	s32 wallX1_raw;
	s32 z0;				// Depth
	s32 z1;
	s32 uCoord0;		// Texture U coordinate offset based on clipping.
	s32 x0View;			// Viewspace vertex 0 X coordinate.
	s32 wallX0;			// Clipped projected X Coordinates.
	s32 wallX1;
	u8  orient;			// Wall orientation (WORIENT_DZ_DX or WORIENT_DX_DZ)
	u8  pad8[3];
	s32 slope;			// Wall slope
	s32 uScale;			// dUdX or dUdZ based on wall orientation.
};

namespace RClassicWall
{
	void wall_process(RWall* wall);
	s32  wall_mergeSort(RWallSegment* segOutList, s32 availSpace, s32 start, s32 count);
	void wall_drawSolid(RWallSegment* wallSegment);
}
