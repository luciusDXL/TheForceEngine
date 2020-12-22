#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Image Loading
// TODO: Replace DeviL with libPNG? or STB_IMAGE?
// Can use std_image.h and std_image_write.h for reading and writing.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

namespace TFE_GIF
{
	bool startGif(const char* path, u32 width, u32 height, u32 fps);
	void addFrame(const u8* imageData);
	bool write();
}
