#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "rmath.h"
#include "rlimits.h"

struct RSector;
struct EdgePair;
struct TextureFrame;

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
	fixed16 texelLength;

	// Wall heights in texels.
	fixed16 topTexelHeight;
	fixed16 midTexelHeight;
	fixed16 botTexelHeight;

	// Texture Offsets
	fixed16 topUOffset;
	fixed16 topVOffset;

	fixed16 midUOffset;
	fixed16 midVOffset;

	fixed16 botUOffset;
	fixed16 botVOffset;

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
	fixed16 z0;			// Depth
	fixed16 z1;
	fixed16 uCoord0;	// Texture U coordinate offset based on clipping.
	fixed16 x0View;		// Viewspace vertex 0 X coordinate.
	s32 wallX0;			// Clipped projected X Coordinates.
	s32 wallX1;
	u8  orient;			// Wall orientation (WORIENT_DZ_DX or WORIENT_DX_DZ)
	u8  pad8[3];
	fixed16 slope;		// Wall slope
	fixed16 uScale;		// dUdX or dUdZ based on wall orientation.
};

namespace RClassicWall
{
	void wall_process(RWall* wall);
	s32  wall_mergeSort(RWallSegment* segOutList, s32 availSpace, s32 start, s32 count);

	void wall_drawSolid(RWallSegment* wallSegment);
	void wall_drawTransparent(RWallSegment* wallSegment, EdgePair* edge);
	void wall_drawMask(RWallSegment* wallSegment);
	void wall_drawBottom(RWallSegment* wallSegment);
	void wall_drawTop(RWallSegment* wallSegment);
	void wall_drawTopAndBottom(RWallSegment* wallSegment);

	void wall_drawSkyTop(RSector* sector);
	void wall_drawSkyTopNoWall(RSector* sector);
	void wall_drawSkyBottom(RSector* sector);
	void wall_drawSkyBottomNoWall(RSector* sector);

	void wall_addAdjoinSegment(s32 length, s32 x0, fixed16 top_dydx, fixed16 y1, fixed16 bot_dydx, fixed16 y0, RWallSegment* wallSegment);
}
