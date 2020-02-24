#include "fontAsset.h"
#include <DXL2_System/system.h>
#include <DXL2_Asset/assetSystem.h>
#include <DXL2_Archive/archive.h>
#include <assert.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>

namespace DXL2_Font
{
	#pragma pack(push)
	#pragma pack(1)
	struct FNT_Header
	{
		char magic[4];			// 'FNT' + 15h (21d)
		u8   height;			// Height of the font
		u8   u1;				// Unknown
		s16  DataSize;			// Data after header
		u8   First;				// First character in font
		u8   Last;				// Last character in font
		u8   pad1[22];			// 22 times 0x00
	};
	#pragma pack(pop)

	typedef std::map<std::string, Font*> FontMap;
	static FontMap s_fonts;
	static std::vector<u8> s_buffer;
	static const char* c_defaultGob = "DARK.GOB";

	struct FontChar
	{
		float width;
		float step_width;
		float u[2];
		float v[2];
	};

	Font* get(const char* name)
	{
		FontMap::iterator iFont = s_fonts.find(name);
		if (iFont != s_fonts.end())
		{
			return iFont->second;
		}

		// It doesn't exist yet, try to load the font.
		if (!DXL2_AssetSystem::readAssetFromGob(c_defaultGob, name, s_buffer))
		{
			return nullptr;
		}

		// Create the font.
		Font* font = new Font;
		const FNT_Header* header = (FNT_Header*)s_buffer.data();
		font->startChar = header->First;
		font->endChar = header->Last;
		font->charCount = (header->Last - header->First) + 1;
		font->height = header->height;

		const u8* data = s_buffer.data() + sizeof(FNT_Header);
		u32 totalImageSize = 0;
		for (s32 i = 0; i < (s32)font->charCount; i++)
		{
			s32 width = (s32)*data; data++;
			data += width * header->height;
			totalImageSize += width * header->height;
		}
		font->imageData = new u8[totalImageSize];

		data = s_buffer.data() + sizeof(FNT_Header);
		u32 offset = 0;
		font->maxWidth = 0;
		for (s32 i = 0; i < (s32)font->charCount; i++)
		{
			s32 width = (s32)*data; data++;

			font->maxWidth = std::max(font->maxWidth, (u8)width);
			font->width[i] = width;
			font->imageOffset[i] = offset;

			const u32 imageSize = width * header->height;
			memcpy(font->imageData + offset, data, imageSize);
			offset += imageSize;
			data += width * header->height;
		}

		s_fonts[name] = font;
		return font;
	}

	void freeAll()
	{
		FontMap::iterator iFont = s_fonts.begin();
		for (; iFont != s_fonts.end(); ++iFont)
		{
			Font* font = iFont->second;
			delete []font->imageData;
			delete font;
		}
		s_fonts.clear();
	}
}