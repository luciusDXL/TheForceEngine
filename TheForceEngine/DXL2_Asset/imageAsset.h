#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Image Loading
// TODO: Replace DeviL with libPNG? or STB_IMAGE?
// Can use std_image.h and std_image_write.h for reading and writing.
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>

struct Image
{
	u32 width  = 0u;
	u32 height = 0u;
	u32* data  = nullptr;
};

namespace DXL2_Image
{
	void init();
	void shutdown();

	Image* get(const char* imagePath);
	void free(Image* image);
	void freeAll();
}
