#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "rmath.h"

struct RSector;

struct FlatEdges
{
	// Ceiling
	s32 yCeil0;
	s32 yCeil1;
	s32 dyCeil_dx;

	s32 yPixel_C0;
	s32 yPixel_C1;

	// Floor
	s32 yFloor0;
	s32 yFloor1;
	s32 dyFloor_dx;

	s32 yPixel_F0;
	s32 yPixel_F1;

	// Screen X parameters, [x0, x1] is inclusive screen pixels.
	s32 lengthInPixels;
	s32 x0;
	s32 x1;
};

namespace RClassicFlat
{
	void flat_addEdges(s32 length, s32 x0, s32 dyFloor_dx, s32 yFloor, s32 dyCeil_dx, s32 yCeil);
	void flat_setup(s32 length, s32 x0, s32 dyFloor_dx, s32 yFloor1, s32 yFloor, s32 dyCeil_dx, s32 yCeil, s32 yCeil1, FlatEdges* wallPart);

	void flat_drawCeiling(RSector* sector, FlatEdges* edges, s32 count);
	void flat_drawFloor(RSector* sector, FlatEdges* edges, s32 count);
}