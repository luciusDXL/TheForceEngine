#include "imageAsset.h"
#include <TFE_System/system.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

namespace TFE_Image
{
	typedef std::map<std::string, Image*> ImageMap;
	static ImageMap s_images;
	static std::vector<u8> s_buffer;

	void init()
	{
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_Image::init");
	}

	void shutdown()
	{
		freeAll();
	}

	Image* loadFromMemory(const u8* buffer, size_t size)
	{
		Image* image = new Image;

		int n;
		u8 *buf = stbi_load_from_memory(buffer, size, (int *) (&image->width),
										(int *) (&image->height), &n, STBI_rgb_alpha);
		if (buf == nullptr) {
			// TODO: handle error.
			return nullptr;
		}

		image->data = new u32[image->width * image->height];
		memcpy(image->data, buf, image->width * image->height * sizeof(u32));
		stbi_image_free(buf);

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

		int n;
		// Always require all components in 4th parameter
		u8 *buf = stbi_load(imagePath, (int *) (&image->width), (int *) (&image->height), &n,
							STBI_rgb_alpha);
		if (buf == nullptr)
		{
			// TODO: handle error.
			return nullptr;
		}

		image->data = new u32[image->width * image->height];
		memcpy(image->data, buf, image->width * image->height * sizeof(u32));
		stbi_image_free(buf);

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
		stbi_write_png(path, width, height, STBI_rgb_alpha, pixelData, 0);
	}
}
