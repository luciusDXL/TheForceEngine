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
	RWall* mirrorWall;
	s32 drawFlags;
	s32 mirror;

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
	fixed16_16 texelLength;

	// Wall heights in texels.
	fixed16_16 topTexelHeight;
	fixed16_16 midTexelHeight;
	fixed16_16 botTexelHeight;

	// Texture Offsets
	fixed16_16 topUOffset;
	fixed16_16 topVOffset;

	fixed16_16 midUOffset;
	fixed16_16 midVOffset;

	fixed16_16 botUOffset;
	fixed16_16 botVOffset;

	fixed16_16 signUOffset;
	fixed16_16 signVOffset;

	fixed16_16 wallDirX;
	fixed16_16 wallDirZ;
	// Wall length (fixed point).
	fixed16_16 length;

	// Update Frame
	s32 drawFrame;

	// Flags
	u32 flags1;
	u32 flags2;
	u32 flags3;

	fixed16_16 worldX0;
	fixed16_16 worldZ0;

	// Lighting
	s32 wallLight;

	// Wall angle.
	s32 angle;
};

struct RWallSegment
{
	RWall* srcWall;		// Source wall
	s32 wallX0_raw;		// Projected wall X coordinates before adjoin/wall clipping.
	s32 wallX1_raw;
	fixed16_16 z0;			// Depth
	fixed16_16 z1;
	fixed16_16 uCoord0;	// Texture U coordinate offset based on clipping.
	fixed16_16 x0View;		// Viewspace vertex 0 X coordinate.
	s32 wallX0;			// Clipped projected X Coordinates.
	s32 wallX1;
	u8  orient;			// Wall orientation (WORIENT_DZ_DX or WORIENT_DX_DZ)
	u8  pad8[3];
	fixed16_16 slope;		// Wall slope
	fixed16_16 uScale;		// dUdX or dUdZ based on wall orientation.
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

	void wall_addAdjoinSegment(s32 length, s32 x0, fixed16_16 top_dydx, fixed16_16 y1, fixed16_16 bot_dydx, fixed16_16 y0, RWallSegment* wallSegment);

	void wall_setupAdjoinDrawFlags(RWall* wall);
	void wall_computeTexelHeights(RWall* wall);
}
