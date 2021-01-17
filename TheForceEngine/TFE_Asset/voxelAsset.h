#pragma once
//////////////////////////////////////////////////////////////////////
// Jedi specific structures for 3DOs
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <string>
#include <vector>

// RLE compressed voxel column.
struct VoxelColumn
{
	u32 dataSize;
	u8* data;
};

enum SortAxis
{
	POS_X = (1 << 0),
	NEG_X = (1 << 1),
	POS_Z = (1 << 2),
	NEG_Z = (1 << 3),
	// Used for caps.
	NEG_Y = (1 << 0),
	POS_Y = (1 << 1),

	// Helpers to make the code a bit more clear.
	PRIM_X_AXIS = (POS_X | NEG_X),
	PRIM_Z_AXIS = (POS_Z | NEG_Z),
	NEG_X_AXIS = (NEG_X | (NEG_X << 4)),
	NEG_Z_AXIS = (NEG_Z | (NEG_Z << 4)),
};

struct VoxelModel
{
	s32 width;
	s32 height;
	s32 depth;

	f32 worldWidth;
	f32 worldHeight;
	f32 worldDepth;
	f32 radius;

	VoxelColumn* columns;	// width x depth 'VoxelColumns'.
};

namespace TFE_VoxelModel
{
	VoxelModel* get(const char* name, bool yUp = true);
	void freeAll();
}
