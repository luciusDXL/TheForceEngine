#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Image Loading
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <SDL_image.h>

namespace TFE_Image
{
	void init();
	void shutdown();

	SDL_Surface* get(const char* imagePath);
	SDL_Surface* loadFromMemory(const u8* buffer, size_t size);
	void free(SDL_Surface* image);
	void freeAll();

	void writeImage(const char* path, u32 width, u32 height, u32* pixelData);

	size_t writeImageToMemory(u8* output, u32 srcw, u32 srch, u32 dstw, u32 dsth, const u32* pixelData);
	void readImageFromMemory(SDL_Surface** output, size_t size, const u32* pixelData);
}
