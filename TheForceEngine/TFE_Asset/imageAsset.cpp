#include "imageAsset.h"
#include <TFE_System/system.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Archive/archive.h>
#include <assert.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

#define IL_USE_PRAGMA_LIBS
#include <IL/il.h>
#include <IL/ilu.h>

namespace TFE_Image
{
	typedef std::map<std::string, Image*> ImageMap;
	static ImageMap s_images;
	static std::vector<u8> s_buffer;

	void init()
	{
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_Image::init");

		// Initialize IL
		ilInit();
		iluInit();

		// We want all images to be loaded in a consistent manner
		ilEnable(IL_ORIGIN_SET);
		ilOriginFunc(IL_ORIGIN_UPPER_LEFT);
	}

	void shutdown()
	{
		freeAll();
		ilShutDown();
	}

	Image* loadFromMemory(const u8* buffer, size_t size)
	{
		Image* image = new Image;

		// Now let's switch over to using devIL...
		ILuint handle;

		// In the next section, we load one image
		ilGenImages(1, &handle);
		ilBindImage(handle);
		if (ilLoadL(IL_JPG, buffer, (ILuint)size) == IL_FALSE)
		{
			return nullptr;
		}
		
		// Let’s spy on it a little bit
		image->width  = (u32)ilGetInteger(IL_IMAGE_WIDTH);  // getting image width
		image->height = (u32)ilGetInteger(IL_IMAGE_HEIGHT); // and height

		image->data = new u32[image->width * image->height];
		// get the image data
		ilCopyPixels(0, 0, 0, image->width, image->height, 1, IL_RGBA, IL_UNSIGNED_BYTE, image->data);

		// Finally, clean the mess!
		ilDeleteImages(1, &handle);

		return image;
	}

	Image* get(const char* imagePath)
	{
		ImageMap::iterator iImage = s_images.find(imagePath);
		if (iImage != s_images.end())
		{
			return iImage->second;
		}

		Image* image = new Image;

		// Now let's switch over to using devIL...
		ILuint handle;

		// In the next section, we load one image
		ilGenImages(1, &handle);
		ilBindImage(handle);
		if (ilLoadImage(imagePath) == IL_FALSE)
		{
			// TODO: handle error.
			// ILenum error = ilGetError();
			return nullptr;
		}

		// Let’s spy on it a little bit
		image->width  = (u32)ilGetInteger(IL_IMAGE_WIDTH);  // getting image width
		image->height = (u32)ilGetInteger(IL_IMAGE_HEIGHT); // and height

		image->data = new u32[image->width * image->height];
		// get the image data
		ilCopyPixels(0, 0, 0, image->width, image->height, 1, IL_RGBA, IL_UNSIGNED_BYTE, image->data);

		// Finally, clean the mess!
		ilDeleteImages(1, &handle);

		s_images[imagePath] = image;
		return image;
	}

	void free(Image* image)
	{
		if (!image) { return; }
		delete[] image->data;

		ImageMap::iterator iImage = s_images.begin();
		for (; iImage != s_images.end(); ++iImage)
		{
			if (iImage->second == image)
			{
				delete iImage->second;
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
			Image* image = iImage->second;
			if (image)
			{
				delete[] image->data;
			}
			delete image;
		}
		s_images.clear();
	}

	void writeImage(const char* path, u32 width, u32 height, u32* pixelData)
	{
		ILuint handle;
		ilGenImages(1, &handle);
		ilBindImage(handle);

		ilTexImage(width, height, 1, 4, IL_RGBA, IL_UNSIGNED_BYTE, (void*)pixelData);
		ilSetData((void*)pixelData);
		ilSaveImage(path);

		ilBindImage(0);
		ilDeleteImage(handle);
	}
}
