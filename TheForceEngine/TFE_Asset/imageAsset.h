#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Image Loading
// TODO: Replace DeviL with libPNG? or STB_IMAGE?
// Can use std_image.h and std_image_write.h for reading and writing.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

struct Image
{
	u32 width  = 0u;
	u32 height = 0u;
	u32* data  = nullptr;
};

namespace TFE_Image
{
	void init();
	void shutdown();

	Image* get(const char* imagePath);
	Image* loadFromMemory(const u8* buffer, size_t size);
	void free(Image* image);
	void freeAll();

	void writeImage(const char* path, u32 width, u32 height, u32* pixelData);
}
