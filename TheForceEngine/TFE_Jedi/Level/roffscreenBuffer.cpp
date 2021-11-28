#include <cstring>

#include "roffscreenBuffer.h"
#include <TFE_System/system.h>
#include <TFE_Game/igame.h>

namespace TFE_Jedi
{
	static u8 s_workBuffer[256];

	OffScreenBuffer* createOffScreenBuffer(s32 width, s32 height, u32 flags)
	{
		OffScreenBuffer* buffer = (OffScreenBuffer*)game_alloc(sizeof(OffScreenBuffer));
		s32 size = width * height;

		buffer->width  = width;
		buffer->height = height;
		buffer->flags  = flags;
		buffer->size   = size;
		buffer->image  = (u8*)game_alloc(size);

		return buffer;
	}

	void freeOffScreenBuffer(OffScreenBuffer* buffer)
	{
		game_free(buffer->image);
		game_free(buffer);
	}

	void offscreenBuffer_clearImage(OffScreenBuffer* buffer, u8 clear_color)
	{
		memset(buffer->image, clear_color, buffer->size);
	}

	void offscreenBuffer_drawTexture(OffScreenBuffer* buffer, TextureData* tex, s32 x, s32 y)
	{
		s32 x0 = x, y0 = y;
		// Early out if the texture does not overlap with the buffer.
		if (x0 >= buffer->width || x0 + tex->width < 0 || y0 >= buffer->height || y0 + tex->height < 0)
		{
			return;
		}

		s32 x1 = x0 + tex->width - 1;
		s32 y1 = y0 + tex->height - 1;
		// Clip the texture rect to the offscreen buffer.
		s32 xDiff = 0;
		s32 yDiff = 0;
		if (y0 < 0)
		{
			y0 = 0;
		}
		if (y1 >= buffer->height)
		{
			yDiff = y1 - buffer->height + 1;
			y1 = buffer->height - 1;
		}

		if (x0 < 0)
		{
			x0 = 0;
			xDiff = -x0;
		}
		if (x1 >= buffer->width)
		{
			x1 = buffer->width - 1;
		}

		s32 height = y1 - y0 + 1;
		if (height <= 0)
		{
			return;
		}

		assert(x0 >= 0 && x1 < buffer->width  && x0 <= x1);
		assert(y0 >= 0 && y1 < buffer->height && y0 <= y1);
		if (tex->flags & OPACITY_TRANS)
		{
			if (tex->compressed)
			{
				// TODO - this doesn't seem to be used, so do it later...
				assert(0);
			}

			for (s32 xi = x0; xi <= x1; xi++)
			{
				s32 srcX = xi - x0 + xDiff;
				s32 xOffset = srcX * tex->height;
				u8* srcImage = tex->image + xOffset + yDiff;

				for (s32 yi = y1; yi >= y0; yi--, srcImage++)
				{
					if (*srcImage)
					{
						buffer->image[yi*buffer->width + xi] = *srcImage;
					}
				}
			}
		}
		else
		{
			if (tex->compressed)
			{
				// TODO - this doesn't seem to be used, so do it later...
				assert(0);
			}

			for (s32 xi = x0; xi <= x1; xi++)
			{
				s32 srcX = xi - x0 + xDiff;
				s32 xOffset = srcX * tex->height;
				u8* srcImage = tex->image + xOffset + yDiff;

				for (s32 yi = y1; yi >= y0; yi--, srcImage++)
				{
					buffer->image[yi*buffer->width + xi] = *srcImage;
				}
			}
		}
	}
}  // TFE_Jedi