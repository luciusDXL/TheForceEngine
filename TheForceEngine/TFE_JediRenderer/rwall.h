#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "rmath.h"

struct TextureFrame;
struct Allocator;

namespace TFE_JediRenderer
{
	struct RSector;

	enum WallOrient
	{
		WORIENT_DZ_DX = 0,
		WORIENT_DX_DZ = 1
	};

	enum WallDrawFlags
	{
		WDF_MIDDLE = 0,
		WDF_TOP = 1,
		WDF_BOT = 2,
		WDF_TOP_AND_BOT = 3,
	};

	struct RWall
	{
		s32 id;
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
		decimal texelLength;

		// Wall heights in texels.
		decimal topTexelHeight;
		decimal midTexelHeight;
		decimal botTexelHeight;

		// Texture Offsets
		decimal topUOffset;
		decimal topVOffset;

		decimal midUOffset;
		decimal midVOffset;

		decimal botUOffset;
		decimal botVOffset;

		decimal signUOffset;
		decimal signVOffset;

		decimal wallDirX;
		decimal wallDirZ;
		// Wall length (fixed point).
		decimal length;

		// INF
		Allocator* infLink;

		// Update Frame
		s32 drawFrame;

		// Flags
		u32 flags1;
		u32 flags2;
		u32 flags3;

		decimal worldX0;
		decimal worldZ0;

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
		decimal z0;			// Depth
		decimal z1;
		decimal uCoord0;	// Texture U coordinate offset based on clipping.
		decimal x0View;		// Viewspace vertex 0 X coordinate.
		s32 wallX0;			// Clipped projected X Coordinates.
		s32 wallX1;
		u8  orient;			// Wall orientation (WORIENT_DZ_DX or WORIENT_DX_DZ)
		u8  pad8[3];
		decimal slope;		// Wall slope
		decimal uScale;		// dUdX or dUdZ based on wall orientation.
	};
}
