#include <climits>
#include <cstring>

#include "delt.h"
#include <TFE_System/endian.h>
#include <TFE_Game/igame.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <TFE_Jedi/Renderer/screenDraw.h>

namespace TFE_DarkForces
{
#pragma pack(push)
#pragma pack(1)
	struct DeltHeader
	{
		s16 offsetX;
		s16 offsetY;
		s16 sizeX;
		s16 sizeY;
	};

	struct DeltLine
	{
		s16 sizeAndType;
		s16 xStart;
		s16 yStart;
	};
#pragma pack(pop)

	static u8* s_buffer = nullptr;
	static size_t s_bufferSize = 0;

	void delt_resetState()
	{
		s_buffer = nullptr;
		s_bufferSize = 0;
	}

	u8* getTempBuffer(size_t size)
	{
		if (size > s_bufferSize)
		{
			s_bufferSize = size + 256;
			s_buffer = (u8*)game_realloc(s_buffer, s_bufferSize);
		}
		return s_buffer;
	}

	// This is here for now, until the proper LFD code is handled.
	JBool loadPaletteFromPltt(const char* name, u8* palette)
	{
		FilePath filePath;
		if (!TFE_Paths::getFilePath(name, &filePath))
		{
			return JFALSE;
		}
		FileStream file;
		file.open(&filePath, Stream::MODE_READ);
		size_t size = file.getSize();
		u8* buffer = getTempBuffer(size);
		file.readBuffer(buffer, (u32)size);
		file.close();

		s32 first = (s32)buffer[0];
		s32 last = (s32)buffer[1];
		s32 count = last - first + 1;
		memset(palette, 0, 768);

		const u8* color = buffer + 2;
		u8* outColor = palette;
		for (s32 i = first; i <= last; i++, color += 3, outColor += 3)
		{
			outColor[0] = color[0];
			outColor[1] = color[1];
			outColor[2] = color[2];
		}

		return JTRUE;
	}

	u32 getFramesFromAnim(const char* name, DeltFrame** outFrames)
	{
		FilePath filePath;
		if (!TFE_Paths::getFilePath(name, &filePath))
		{
			return JFALSE;
		}
		FileStream file;
		file.open(&filePath, Stream::MODE_READ);
		size_t size = file.getSize();
		u8* buffer = getTempBuffer(size);
		file.readBuffer(buffer, (u32)size);
		file.close();

		s16 frameCount = *((s16*)buffer);
		frameCount = TFE_Endian::swapLE16(frameCount);
		
		const u8* frames = buffer + 2;

		*outFrames = (DeltFrame*)game_alloc(sizeof(DeltFrame) * frameCount);
		DeltFrame* outFramePtr = *outFrames;

		for (s32 i = 0; i < frameCount; i++)
		{
			u32 size = *((u32*)frames);
			size = TFE_Endian::swapLE32(size);
			frames += 4;

			loadDeltIntoFrame(&outFramePtr[i], frames, size);
			frames += size;
		}

		return frameCount;
	}

	JBool getFrameFromDelt(const char* name, DeltFrame* outFrame)
	{
		FilePath filePath;
		if (!TFE_Paths::getFilePath(name, &filePath))
		{
			return JFALSE;
		}
		FileStream file;
		file.open(&filePath, Stream::MODE_READ);
		size_t size = file.getSize();
		u8* buffer = getTempBuffer(size);
		file.readBuffer(buffer, (u32)size);
		file.close();

		// Then read out the data.
		loadDeltIntoFrame(outFrame, buffer, (u32)size);
		return JTRUE;
	}

	void getDeltaFrameRect(DeltFrame* frame, ScreenRect* rect)
	{
		rect->left = INT_MAX;
		rect->top = INT_MAX;
		rect->right = -INT_MAX;
		rect->bot = -INT_MAX;

		u8* srcTexels = frame->texture.image;
		s32 w = frame->texture.width;
		s32 h = frame->texture.height;

		for (s32 y = 0; y < h; y++)
		{
			for (s32 x = 0; x < w; x++)
			{
				if (srcTexels[x])
				{
					rect->left = min(x, rect->left);
					rect->top  = min(y, rect->top);
					rect->right = max(x, rect->right);
					rect->bot   = max(y, rect->bot);
				}
			}
			srcTexels += w;
		}
	}

	void blitDeltaFrame(DeltFrame* frame, s32 x0, s32 y0, u8* framebuffer)
	{
		u8* output = &framebuffer[y0 * 320];
		u8* srcTexels = frame->texture.image;
		s32 w = frame->texture.width;
		s32 h = frame->texture.height;
		s32 x1 = x0 + w - 1;
		s32 y1 = y0 + h - 1;

		// Skip if out of bounds.
		if (x0 >= 320 || x1 < 0 || y0 >= 200 || y1 < 0)
		{
			return;
		}
		// Clip
		if (x0 < 0)
		{
			x0 = 0;
		}
		if (x1 >= 320)
		{
			x1 = 319;
		}
		if (y0 < 0)
		{
			srcTexels -= y0 * w;
			output -= y0 * 320;
			y0 = 0;
		}
		if (y1 >= 200)
		{
			y1 = 199;
		}

		for (s32 y = y0; y <= y1; y++)
		{
			for (s32 x = x0; x <= x1; x++)
			{
				if (srcTexels[x - x0])
				{
					output[x] = srcTexels[x - x0];
				}
			}
			srcTexels += w;
			output += 320;
		}
	}

	void blitDeltaFrameScaled(DeltFrame* frame, s32 x0, s32 y0, fixed16_16 xScale, fixed16_16 yScale, u8* framebuffer)
	{
		s32 x1 = x0 + floor16(mul16(intToFixed16(frame->texture.width - 1), xScale));
		s32 y1 = y0 + floor16(mul16(intToFixed16(frame->texture.height- 1), yScale));

		ScreenImage scrImage=
		{
			frame->texture.width,
			frame->texture.height,
			frame->texture.image,
			JTRUE,
			JFALSE
		};

		ScreenRect* rect = vfb_getScreenRect(VFB_RECT_UI);
		blitTextureToScreenScaled(&scrImage, (DrawRect*)rect, x0, y0, xScale, yScale, framebuffer);
	}

	void loadDeltIntoFrame(DeltFrame* frame, const u8* buffer, u32 size)
	{
		DeltHeader header = *((DeltHeader*)buffer);
		header.offsetX = TFE_Endian::swapLE16(header.offsetX);
		header.offsetY = TFE_Endian::swapLE16(header.offsetY);
		header.sizeX = TFE_Endian::swapLE16(header.sizeX);
		header.sizeY = TFE_Endian::swapLE16(header.sizeY);
		header.sizeX++;
		header.sizeY++;

		frame->texture.width = header.sizeX;
		frame->texture.height = header.sizeY;
		frame->texture.uvWidth = header.sizeX;
		frame->texture.uvHeight = header.sizeY;
		frame->texture.dataSize = frame->texture.width * frame->texture.height;
		frame->texture.logSizeY = 0;
		frame->texture.flags = OPACITY_TRANS;
		frame->texture.textureId = 0;
		
		frame->offsetX = header.offsetX;
		frame->offsetY = header.offsetY;
		frame->texture.image = (u8*)game_alloc(frame->texture.dataSize);
		memset(frame->texture.image, 0, frame->texture.dataSize);

		const u8* data = buffer + sizeof(DeltHeader);
		const u8* end = size ? buffer + size : nullptr;
		while ((data && data < end) || !data)
		{
			const DeltLine* line = (DeltLine*)data;
			if (line->sizeAndType == 0)
			{
				break;
			}

			data += sizeof(DeltLine);

			const s32 startX = TFE_Endian::swapLE16(line->xStart);
			const s32 startY = TFE_Endian::swapLE16(line->yStart);
			const bool rle = (TFE_Endian::swapLE16(line->sizeAndType) & 1) ? true : false;
			s32 pixelCount = (TFE_Endian::swapLE16(line->sizeAndType) >> 1) & 0x3FFF;

			u8* image = frame->texture.image + startX + startY * header.sizeX;
			while (pixelCount > 0)
			{
				if (rle)
				{
					//read count byte...
					u8 count = *data; data++;
					if (!(count & 1)) // direct
					{
						count >>= 1;
						for (s32 p = 0; p < count; p++, image++, data++)
						{
							*image = *data;
						}
						pixelCount -= count;
					}
					else	//rle
					{
						count >>= 1;
						const u8 pixel = *data; data++;

						memset(image, pixel, count);
						image += count;

						pixelCount -= count;
					}
				}
				else
				{
					for (s32 p = 0; p < pixelCount; p++, image++, data++)
					{
						*image = *data;
					}
					pixelCount = 0;
				}
			}
		}
	}
}