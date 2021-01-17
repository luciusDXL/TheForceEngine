#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

struct VoxelModel;

namespace TFE_JediRenderer
{
	struct SecObject;
	namespace RClassic_Float
	{
		void robjVoxel_init();
		void robjVoxel_draw(SecObject* obj, VoxelModel* model);
	}
}
