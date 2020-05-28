#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "rmath.h"

struct RWall;

struct RSector
{
	s32 u00;			// 0x00
	s32 u01;			// 0x04
	s32 u02;			// 0x08
	s32 prevDrawFrame;	// 0x0c  -- previous frame where the sector was drawn.
	s32 vertexCount;	// 0x10
	vec2* verticesWS;	// 0x14
	vec2* verticesVS;	// 0x18 (this is a pointer, but walls is a guess)
	s32 wallCount;		// 0x1c
	RWall* walls;		// 0x20
	s32 floorHeight;	// 0x24
	s32 ceilingHeight;	// 0x28
	s32 secHeight;		// 0x2c	-- final second height (guess, its the same as floorHeight in test cases)
	s32 u60;			// 0x30
	s32 u61;			// 0x34
	s32 u62;			// 0x38
	s32 ambientFixed;	// 0x3c	 -- fixed point sector ambient.
	u8  u30[0x20];		// 0x40
	s32 u4;				// 0x60 = 2?
	u32* ptr;			// 0x64  -- could be something else.
	u8  u50[0x18];		// 0x68
	s32 startWall;		// 0x78
	s32 drawWallCnt;	// 0x7C
	u32 flags1;			// 0x80
};
