#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Image Loading
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <SDL_image.h>
#include <TFE_FileSystem/physfswrapper.h>

namespace TFE_Image
{
	void init();
	void shutdown();

	SDL_Surface* get(const char* imagePath, TFE_VPATH vpathid = VPATH_NONE);
	SDL_Surface* loadFromMemory(const void* buffer, size_t size);
	void free(SDL_Surface* image);
	void freeAll();

	// no vpath here since writing is only allowed at VPATH_TFE */
	void writeImage(const char* path, u32 width, u32 height, void* pixelData);

	u32 writeImageToMemory(char* output, u32 srcw, u32 srch, u32 dstw, u32 dsth, void* pixelData);
	void readImageFromMemory(SDL_Surface** output, u32 size, void* pixelData);
}
