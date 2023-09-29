#include "imageAsset.h"
#include <TFE_System/system.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Archive/archive.h>
#include <TFE_FileSystem/memorystream.h>
#include <assert.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_rwops.h>

#ifdef _WIN32
#pragma comment( lib, "SDL2_image.lib" )
#endif

namespace TFE_Image
{
	typedef std::map<std::string, SDL_Surface*> ImageMap;
	static ImageMap s_images;
	static std::vector<u8> s_buffer;

	static SDL_Surface* convertToRGBA(SDL_Surface* src)
	{
		SDL_PixelFormat rgba32 = {
			SDL_PIXELFORMAT_RGBA32,
			NULL,        // Palette
			32,          // BitsPerPixel
			4,           // BytesPerPixel
			{0, 0},      // padding
			0x000000FF,  // Rmask
			0x0000FF00,  // Gmask
			0x00FF0000,  // Bmask
			0xFF000000,  // Amask
			0,           // Rloss
			0,           // Gloss
			0,           // Bloss
			0,           // Aloss 
			0,           // Rshift
			8,           // Gshift
			16,          // Bshift
			24,          // Ashift
			0,
			nullptr
		};
		SDL_Surface* n = SDL_ConvertSurface(src, &rgba32, 0);
		SDL_FreeSurface(src);
		return n;
	}

	void init()
	{
		int ret;
		int flags = IMG_INIT_PNG | IMG_INIT_JPG;
		
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_Image::init");
		ret = IMG_Init(flags);
		if ((ret & flags) != flags)
		{
			TFE_System::logWrite(LOG_ERROR, "ImageAsset", "SDL_image init failed!");
		}
	}

	void shutdown()
	{
		freeAll();
		IMG_Quit();
	}

	SDL_Surface* loadFromMemory(const u8* buffer, size_t size)
	{
		SDL_RWops* memops = SDL_RWFromConstMem(buffer, (s32)size);
		if (!memops)
			return nullptr;

		SDL_Surface* sdlimg = IMG_Load_RW(memops, 1);
		if (!sdlimg)
			return nullptr;
		if (sdlimg->format->BitsPerPixel != 32)
			sdlimg = convertToRGBA(sdlimg);
		if (!sdlimg)
			return nullptr;

		return sdlimg;
	}

	SDL_Surface* get(const char* imagePath)
	{
		ImageMap::iterator iImage = s_images.find(imagePath);
		if (iImage != s_images.end())
		{
			return iImage->second;
		}

		SDL_Surface* sdlimg = IMG_Load(imagePath);
		if (!sdlimg)
			return nullptr;
		if (sdlimg->format->BitsPerPixel != 32)
			sdlimg = convertToRGBA(sdlimg);
		if (!sdlimg)
			return nullptr;
		
		s_images[imagePath] = sdlimg;
		return sdlimg;
	}

	void free(SDL_Surface* image)
	{
		if (!image) { return; }

		SDL_FreeSurface(image);
		ImageMap::iterator iImage = s_images.begin();
		for (; iImage != s_images.end(); ++iImage)
		{
			if (iImage->second == image)
			{
				s_images.erase(iImage);
				break;
			}
		}
	}

	void freeAll()
	{
		ImageMap::iterator iImage = s_images.begin();
		for (; iImage != s_images.end(); ++iImage)
		{
			SDL_Surface* image = iImage->second;
			if (image)
			{
				SDL_FreeSurface(image);
			}
		}
		s_images.clear();
	}

	void writeImage(const char* path, u32 width, u32 height, u32* pixelData)
	{
		// Screenshots are upside down on Windows, so fix it here for now.
		u32* writeBuffer = pixelData;
#ifdef _WIN32
		// Images are upside down.
		s_buffer.resize(width * height * 4);
		const u32* srcBuffer = pixelData;
		u32* dstBuffer = (u32*)s_buffer.data();
		for (u32 y = 0; y < height; y++)
		{
			memcpy(&dstBuffer[y * width], &srcBuffer[(height - y - 1) * width], width * 4);
		}
		writeBuffer = dstBuffer;
#endif

		SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(writeBuffer, width, height,
							     32, width * sizeof(u32), 
							     0xFF, 0xFF00, 0xFF0000, 0xFF000000);
		if (!surf)
		{
			TFE_System::logWrite(LOG_ERROR, "writeImage", "Saving PNG '%s' - cannot allocate surface", path);
			return;
		}
		if (IMG_SavePNG(surf, path) != 0)
		{
			TFE_System::logWrite(LOG_ERROR, "writeImage", "Saving PNG '%s' failed with '%s'", path, SDL_GetError());
		}
	}

	//////////////////////////////////////////////////////
	// Wacky file override to get SDL-Image to write
	// images to memory. SDL_RWmemOps by default do not support writing.
	//////////////////////////////////////////////////////
	static size_t SDLCALL _sdl_wop_mem(struct SDL_RWops* context, const void *ptr,
					   size_t size, size_t num)
	{
		const size_t bytes = num * size;
		const ptrdiff_t space = context->hidden.mem.stop - context->hidden.mem.here;
		if (space >= (ptrdiff_t)bytes)
		{
			memcpy(context->hidden.mem.here, ptr, bytes);
			context->hidden.mem.here += bytes;
			return bytes;
		}
		return 0;
	}

	//////////////////////////////////////////////////////
	// Code to write and read images from memory.
	//////////////////////////////////////////////////////
	size_t writeImageToMemory(u8* output, u32 srcw, u32 srch, u32 dstw,
				  u32 dsth, const u32* pixelData)
	{
		size_t written;
		int ret;

		SDL_Surface* surf = SDL_CreateRGBSurfaceFrom((void *)pixelData, srcw, srch, 32, srcw * sizeof(u32),
							     0xFF, 0xFF00, 0xFF0000, 0xFF000000);
		if (!surf)
			return 0;
		if ((srcw != dstw) || (srch != dsth))
		{
			const SDL_Rect rs = { 0, 0, (int)srcw, (int)srch };
			const SDL_Rect rd = { 0, 0, (int)dstw, (int)dsth };
			SDL_Surface* scaled = SDL_CreateRGBSurface(0, dstw, dsth, 32,
								  0xFF, 0xFF00, 0xFF0000, 0xFF000000);
			if (!scaled)
			{
				SDL_FreeSurface(surf);
				return 0;
			}
			ret = SDL_SoftStretch(surf, &rs, scaled, &rd);
			if (ret != 0)
			{
				SDL_FreeSurface(surf);
				SDL_FreeSurface(scaled);
				return 0;
			}
			SDL_FreeSurface(surf);
			surf = scaled;
			srcw = dstw;
			srch = dsth;
		}

		SDL_RWops* memops = SDL_RWFromMem(output, srcw * srch * sizeof(u32));
		if (!memops)
		{
			SDL_FreeSurface(surf);
			return 0;
		}
		memops->write = _sdl_wop_mem;
		ret = IMG_SavePNG_RW(surf, memops, 0);
		SDL_FreeSurface(surf);
		if (ret == 0)
		{
			written = memops->hidden.mem.here - memops->hidden.mem.base;
		}
		SDL_FreeRW(memops);
		return written;
	}

	void readImageFromMemory(SDL_Surface** output, size_t size, const u32* pixelData)
	{
		SDL_RWops* memops = SDL_RWFromConstMem(pixelData, (s32)size);
		if (!memops)
			return;

		SDL_Surface* sdlimg = IMG_Load_RW(memops, 1);
		if (output)
			*output = sdlimg;
	}
}
