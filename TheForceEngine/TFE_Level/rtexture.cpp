#include "rtexture.h"
#include <TFE_System/system.h>
#include <TFE_Archive/archive.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_FileSystem/paths.h>

using namespace InfAllocator;

namespace TFE_Level
{
	enum
	{
		DF_BM_VERSION = 30,
		DF_ANIM_ID = 2,
	};

	static const char* c_defaultGob = "TEXTURES.GOB";
	static std::vector<u8> s_buffer;
	static Allocator* s_textureAnimAlloc;

	u8 readByte(const u8*& data)
	{
		u8 res = *data;
		data++;
		return res;
	}

	s16 readShort(const u8*& data)
	{
		s16 res = *((s16*)data);
		data+=2;
		return res;
	}

	s32 readInt(const u8*& data)
	{
		s32 res = *((s32*)data);
		data += 4;
		return res;
	}

	void bitmap_createAnimatedTextureAllocator()
	{
		s_textureAnimAlloc = allocator_create(sizeof(AnimatedTexture));
	}

	TextureData* bitmap_load(const char* filepath)
	{
		TextureData* texture = (TextureData*)malloc(sizeof(TextureData));

		char gobPath[TFE_MAX_PATH];
		TFE_Paths::appendPath(PATH_SOURCE_DATA, c_defaultGob, gobPath);
		if (!TFE_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, filepath, s_buffer))
		{
			TFE_System::logWrite(LOG_ERROR, "bitmap_load", "Error loading '%s'.", filepath);
			return nullptr;
		}

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
			// TODO
		}
		else
		{
			texture->dataSize = texture->width * texture->height;
			// Datasize, ignored.
			data += 4;
			texture->u14 = 0;

			// Padding, ignored.
			data += 12;

			// Allocate and read the BM image.
			texture->image = (u8*)malloc(texture->dataSize);
			memcpy(texture->image, data, texture->dataSize);
			data += texture->dataSize;
		}

		if (texture->compressed)
		{
			// TODO
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
		anim->frameList = (TextureData**)malloc(sizeof(TextureData**) * anim->count);

		TextureData** texPtrs = anim->frameList;
		for (s32 i = 0; i < anim->count; i++, texPtrs++, textureOffsets++)	// i = eax = [ebp-0x14]
		{
			u32 offset = *textureOffsets;
			TextureData* frame = (TextureData*)(tex->image + 2 + offset);
			*texPtrs = frame;

			// Skip past the header directly to the pixel data.
			frame->image = (u8*)frame + sizeof(TextureData);
		}

		if (frameRate)
		{
			anim->delay = s32(145.5f / f32(frameRate));	// Delay is in "ticks."
			anim->nextTime = 0;
			*texture = anim->frameList[0];
		}
		else
		{
			// Hold indefinitely.
			anim->nextTime = 0xffffffff;
			// The "image" is really the animation.
			tex->image = (u8*)anim;
		}
	}
}