#include <cstring>

#include "rtexture.h"
#include <TFE_Game/igame.h>
#include <TFE_System/system.h>
#include <TFE_Archive/archive.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Jedi/Task/task.h>

using namespace TFE_DarkForces;
using namespace TFE_Memory;

namespace TFE_Jedi
{
	enum
	{
		DF_BM_VERSION = 30,
		DF_ANIM_ID = 2,
	};

	static std::vector<u8> s_buffer;
	static Allocator* s_textureAnimAlloc = nullptr;
	static Task* s_textureAnimTask = nullptr;
	static MemoryRegion* s_memoryRegion = nullptr;

	void decompressColumn_Type1(const u8* src, u8* dst, s32 pixelCount);
	void decompressColumn_Type2(const u8* src, u8* dst, s32 pixelCount);
	void textureAnimationTaskFunc(MessageType msg);

	u8 readByte(const u8*& data)
	{
		u8 res = *data;
		data++;
		return res;
	}

	s16 readShort(const u8*& data)
	{
		s16 res = *((s16*)data);
		data += 2;
		return res;
	}

	s32 readInt(const u8*& data)
	{
		s32 res = *((s32*)data);
		data += 4;
		return res;
	}

	void bitmap_setupAnimationTask()
	{
		s_textureAnimTask = createSubTask("texture animation", textureAnimationTaskFunc);
		s_textureAnimAlloc = allocator_create(sizeof(AnimatedTexture));
	}

	MemoryRegion* bitmap_getAllocator()
	{
		return s_memoryRegion;
	}

	void bitmap_setAllocator(MemoryRegion* allocator)
	{
		s_memoryRegion = allocator;
	}

	TextureData* bitmap_load(FilePath* filepath, u32 decompress)
	{
		FileStream file;
		if (!file.open(filepath, FileStream::MODE_READ))
		{
			return nullptr;
		}
		size_t size = file.getSize();
		s_buffer.resize(size);
		file.readBuffer(s_buffer.data(), (u32)size);
		file.close();

		TextureData* texture = (TextureData*)region_alloc(s_memoryRegion, sizeof(TextureData));
		const u8* data = s_buffer.data();
		const u8* fheader = data;
		data += 3;

		if (strncmp((char*)fheader, "BM ", 3))
		{
			TFE_System::logWrite(LOG_ERROR, "bitmap_load", "File '%s' is not a valid BM file.", filepath);
			return nullptr;
		}

		u8 version = readByte(data);
		if (version != DF_BM_VERSION)
		{
			TFE_System::logWrite(LOG_ERROR, "bitmap_load", "File '%s' has invalid BM version '%u'.", filepath, version);
			return nullptr;
		}

		texture->width = readShort(data);
		texture->height = readShort(data);
		texture->uvWidth = readShort(data);
		texture->uvHeight = readShort(data);
		texture->flags = readByte(data);
		texture->logSizeY = readByte(data);
		texture->compressed = readByte(data);
		// value is ignored.
		data++;

		if (texture->compressed)
		{
			s32 inSize = readInt(data);
			// values are ignored.
			data += 12;

			if (decompress & 1)
			{
				texture->dataSize = texture->width * texture->height;
				texture->image = (u8*)region_alloc(s_memoryRegion, texture->dataSize);

				const u8* inBuffer = data;
				data += inSize;

				const u32* columns = (u32*)data;
				data += sizeof(u32) * texture->width;

				if (texture->compressed == 1)
				{
					u8* dst = texture->image;
					for (s32 i = 0; i < texture->width; i++, dst += texture->height)
					{
						const u8* src = &inBuffer[columns[i]];
						decompressColumn_Type1(src, dst, texture->height);
					}
				}
				else if (texture->compressed == 2)
				{
					u8* dst = texture->image;
					for (s32 i = 0; i < texture->width; i++, dst += texture->height)
					{
						const u8* src = &inBuffer[columns[i]];
						decompressColumn_Type2(src, dst, texture->height);
					}
				}
				texture->compressed = 0;
				texture->columns = nullptr;
			}
			else
			{
				texture->dataSize = inSize;
				texture->image = (u8*)region_alloc(s_memoryRegion, texture->dataSize);
				memcpy(texture->image, data, texture->dataSize);
				data += texture->dataSize;

				texture->columns = (u32*)region_alloc(s_memoryRegion, texture->width * sizeof(u32));
				memcpy(texture->columns, data, texture->width * sizeof(u32));
				data += texture->width * sizeof(u32);
			}
		}
		else
		{
			texture->dataSize = texture->width * max(1, texture->height);
			// Datasize, ignored.
			data += 4;
			texture->columns = nullptr;

			// Padding, ignored.
			data += 12;

			// Allocate and read the BM image.
			texture->image = (u8*)region_alloc(s_memoryRegion, texture->dataSize);
			memcpy(texture->image, data, texture->dataSize);
			data += texture->dataSize;
		}

		return texture;
	}

	TextureData* bitmap_loadFromMemory(const u8* data, size_t size, u32 decompress)
	{
		TextureData* texture = (TextureData*)malloc(sizeof(TextureData));
		const u8* fheader = data;
		data += 3;

		if (strncmp((char*)fheader, "BM ", 3))
		{
			TFE_System::logWrite(LOG_ERROR, "bitmap_load", "Load From Memory - invalid data.");
			return nullptr;
		}

		u8 version = readByte(data);
		if (version != DF_BM_VERSION)
		{
			TFE_System::logWrite(LOG_ERROR, "bitmap_load", "Load From Memory - invalid BM version '%u'.", version);
			return nullptr;
		}

		texture->width = readShort(data);
		texture->height = readShort(data);
		texture->uvWidth = readShort(data);
		texture->uvHeight = readShort(data);
		texture->flags = readByte(data);
		texture->logSizeY = readByte(data);
		texture->compressed = readByte(data);
		// value is ignored.
		data++;

		if (texture->compressed)
		{
			s32 inSize = readInt(data);
			// values are ignored.
			data += 12;

			if (decompress & 1)
			{
				texture->dataSize = texture->width * texture->height;
				texture->image = (u8*)malloc(texture->dataSize);

				const u8* inBuffer = data;
				data += inSize;

				const u32* columns = (u32*)data;
				data += sizeof(u32) * texture->width;

				if (texture->compressed == 1)
				{
					u8* dst = texture->image;
					for (s32 i = 0; i < texture->width; i++, dst += texture->height)
					{
						const u8* src = &inBuffer[columns[i]];
						decompressColumn_Type1(src, dst, texture->height);
					}
				}
				else if (texture->compressed == 2)
				{
					u8* dst = texture->image;
					for (s32 i = 0; i < texture->width; i++, dst += texture->height)
					{
						const u8* src = &inBuffer[columns[i]];
						decompressColumn_Type2(src, dst, texture->height);
					}
				}
				texture->compressed = 0;
				texture->columns = nullptr;
			}
			else
			{
				texture->dataSize = inSize;
				texture->image = (u8*)malloc(texture->dataSize);
				memcpy(texture->image, data, texture->dataSize);
				data += texture->dataSize;

				texture->columns = (u32*)malloc(texture->width * sizeof(u32));
				memcpy(texture->columns, data, texture->width * sizeof(u32));
				data += texture->width * sizeof(u32);
			}
		}
		else
		{
			texture->dataSize = texture->width * max(1, texture->height);
			// Datasize, ignored.
			data += 4;
			texture->columns = nullptr;

			// Padding, ignored.
			data += 12;

			// Allocate and read the BM image.
			texture->image = (u8*)malloc(texture->dataSize);
			memcpy(texture->image, data, texture->dataSize);
			data += texture->dataSize;
		}

		return texture;
	}

	void bitmap_setupAnimatedTexture(TextureData** texture)
	{
		TextureData* tex = *texture;
		u8 frameRate = tex->image[0];
		u8 animatedId = tex->image[1];

		// if animatedId != DF_ANIM_ID, then this is not a properly setup animated texture.
		if (animatedId != DF_ANIM_ID)
		{
			TFE_System::logWrite(LOG_WARNING, "bitmap_setupAnimatedTexture", "Invalid animatedId %u, should be %u.", animatedId, DF_ANIM_ID);
			tex->uvWidth = 0;
			return;
		}
		
		// In the original DOS code, this is directly set to pointers. But since TFE is compiled as 64-bit, pointers are not the correct size.
		u32* textureOffsets = (u32*)(tex->image + 2);
		AnimatedTexture* anim = (AnimatedTexture*)allocator_newItem(s_textureAnimAlloc);

		// 64 bit pointers are larger than the offsets, so we have to allocate more space (for now).
		anim->frame = 0;
		anim->count = tex->uvHeight;	// frame count is packed into uvHeight.
		anim->texPtr = texture;			// pointer to the texture pointer, allowing us to update that pointer later.
		anim->baseFrame = tex;
		anim->baseData = tex->image;
		anim->frameList = (TextureData**)res_alloc(sizeof(TextureData**) * anim->count);
		assert(anim->frameList);

		for (s32 i = 0; i < anim->count; i++, textureOffsets++)
		{
			u32 offset = *textureOffsets;
			TextureData* frame = (TextureData*)(tex->image + 2 + offset);
			anim->frameList[i] = frame;

			// Offset to account for differences in pointer size.
			size_t pointerOffset = 2 * (sizeof(size_t) - sizeof(s32));

			// Skip past the header directly to the pixel data.
			frame->image = (u8*)frame + sizeof(TextureData) - pointerOffset;
		}

		if (frameRate)
		{
			anim->delay = time_frameRateToDelay(frameRate);	// Delay is in "ticks."
			anim->nextTick = 0;
			*texture = anim->frameList[0];
		}
		else
		{
			// Hold indefinitely.
			anim->nextTick = 0xffffffff;
			// The "image" is really the animation.
			tex->image = (u8*)anim;
		}
	}
		
	// Per frame animated texture update.
	void textureAnimationTaskFunc(MessageType msg)
	{
		task_begin;
		while (msg != MSG_FREE_TASK)
		{
			// No persistent state is required.
			{
				AnimatedTexture* animTex = (AnimatedTexture*)allocator_getHead(s_textureAnimAlloc);
				while (animTex)
				{
					if (animTex->nextTick < s_curTick)
					{
						if (animTex->nextTick == 0)
						{
							animTex->nextTick = s_curTick;
						}

						animTex->frame++;
						if (animTex->frame >= animTex->count)
						{
							animTex->frame = 0;
						}

						*animTex->texPtr = animTex->frameList[animTex->frame];
						animTex->nextTick += animTex->delay;
					}
					animTex = (AnimatedTexture*)allocator_getNext(s_textureAnimAlloc);
				}
			}
			task_yield(TASK_NO_DELAY);
		}
		task_end;
	}
		
	// Type 1: RLE with runs of solid colors. Costs 2 bytes per solid-color run.
	void decompressColumn_Type1(const u8* src, u8* dst, s32 pixelCount)
	{
		while (pixelCount)
		{
			const u8 value = *src;
			src++;

			if (value & 0x80)
			{
				const u8 color = *src;
				src++;

				const u32 count = value & 0x7f;
				// store packed at dst
				for (u32 i = 0; i < count; i++, dst++)
				{
					*dst = color;
				}
				pixelCount -= count;
			}
			else
			{
				const u32 count = value;
				// store packed at dst
				for (u32 i = 0; i < count; i++, dst++, src++)
				{
					*dst = *src;
				}
				pixelCount -= count;
			}
		}
	}

	// Type 2: RLE with runs of transparent texels (useful for sprites). Costs 1 byte per transparent run.
	void decompressColumn_Type2(const u8* src, u8* dst, s32 pixelCount)
	{
		while (pixelCount)
		{
			const u8 value = *src;
			src++;

			if (value & 0x80)
			{
				const u32 count = value & 0x7f;
				// store 0 at dst
				memset(dst, 0, count);
				pixelCount -= count;
			}
			else
			{
				const u32 count = value;
				// store packed at dst
				for (u32 i = 0; i < count; i++, dst++, src++)
				{
					*dst = *src;
				}
				pixelCount -= count;
			}
		}
	}
}