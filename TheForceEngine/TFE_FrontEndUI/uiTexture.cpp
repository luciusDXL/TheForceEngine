#include "uiTexture.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <TFE_DarkForces/mission.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>

namespace TFE_FrontEndUI
{
	static std::vector<u32> s_imageBuffer;

	// Convert the source palette to 32-bit color.
	bool convertPalette(const u8* srcPalette, u32* dstPalette)
	{
		const u8* srcColor = srcPalette;
		for (s32 i = 0; i < 256; i++, dstPalette++, srcColor += 3)
		{
			*dstPalette = CONV_6bitTo8bit(srcColor[0]) | (CONV_6bitTo8bit(srcColor[1]) << 8u) | (CONV_6bitTo8bit(srcColor[2]) << 16u) | (0xffu << 24u);
		}
		return true;
	}
		
	void convertDfTextureToTrueColor(const TextureData* src, const u32* palette, u32* image)
	{
		for (s32 y = 0; y < src->height; y++)
		{
			for (s32 x = 0; x < src->width; x++)
			{
				image[y*src->width + x] = palette[src->image[x*src->height + y]];
			}
		}
	}

	bool createTexture(const TextureData* src, const u32* palette, UiTexture* tex, MagFilter filter)
	{
		if (!src || !tex) { return false; }
		tex->scale = { 1.0f, 1.0f };

		s_imageBuffer.resize(src->width * src->height);
		u32* image = s_imageBuffer.data();
		convertDfTextureToTrueColor(src, palette, image);

		tex->texture = TFE_RenderBackend::createTexture(src->width, src->height, image, filter);

		tex->scale.x = 1.0f;
		tex->scale.z = 1.0f;
		tex->rect[0] = 0.0f;
		tex->rect[1] = 0.0f;
		tex->rect[2] = 0.0f;
		tex->rect[3] = 0.0f;
		return true;
	}
}
