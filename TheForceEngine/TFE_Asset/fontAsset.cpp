#include <cstring>

#include "fontAsset.h"
#include <TFE_System/system.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Archive/archive.h>
#include <assert.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>

namespace TFE_Font
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

	typedef std::map<std::string, FontTFE*> FontMap;
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

	FontTFE* get(const char* name)
	{
		FontMap::iterator iFont = s_fonts.find(name);
		if (iFont != s_fonts.end())
		{
			return iFont->second;
		}

		// It doesn't exist yet, try to load the font.
		if (!TFE_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, name, s_buffer))
		{
			return nullptr;
		}

		// Create the font.
		FontTFE* font = new FontTFE;
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
			font->step[i] = width + 1;
			font->imageOffset[i] = offset;

			const u32 imageSize = width * header->height;
			memcpy(font->imageData + offset, data, imageSize);
			offset += imageSize;
			data += width * header->height;
		}

		s_fonts[name] = font;
		return font;
	}

#pragma pack(push)
#pragma pack(1)
	struct FontHeader
	{
		s16 first;
		s16 last;
		s16 bitsPerLine;
		s16 height;
		s16 minWidth;
		s16 pad16;
	};
#pragma pack(pop)

	FontTFE* getFromFont(const char* name, const char* archivePath)
	{
		FontMap::iterator iFont = s_fonts.find(name);
		if (iFont != s_fonts.end())
		{
			return iFont->second;
		}

		// It doesn't exist yet, try to load the palette.
		if (!TFE_AssetSystem::readAssetFromArchive(archivePath, name, s_buffer))
		{
			return nullptr;
		}

		// Create the font.
		FontTFE* font = new FontTFE;
		memset(font, 0, sizeof(FontTFE));
		FontHeader* header = (FontHeader*)s_buffer.data();
		
		font->startChar = (u8)header->first;
		font->endChar = (u8)header->last;
		font->height = (u8)header->height;
		font->charCount = u8(header->last - header->first + 1);

		//character widths.
		const s32 count = font->charCount;
		const u8* data = s_buffer.data() + sizeof(FontHeader);
		u32 pixelCount = 0u;
		// Why is the data invalid? - only the font height seems to work.
		for (s32 i = 0; i < count; i++, data++)
		{
			font->width[i] = (u8)header->bitsPerLine;
			font->step[i] = (*data) + 2;
			font->maxWidth = std::max(font->maxWidth, font->step[i]);
			pixelCount += font->width[i] * font->height;
		}

		font->imageData = new u8[pixelCount];
		u32 imageOffset = 0u;
		const u8* srcBitmap = s_buffer.data() + sizeof(FontHeader) + header->last;
		for (s32 i = 0; i < count; i++)
		{
			const u8 width = font->width[i];
			u8* dstImage = font->imageData + imageOffset;
			font->imageOffset[i] = imageOffset;
			imageOffset += width * font->height;

			for (u32 y = 0; y < font->height; y++)
			{
				u8 bitmap = *srcBitmap;
				for (u32 x = 0; x < width; x++)
				{
					dstImage[(width - x - 1) * font->height + font->height - y - 1] = (bitmap & (1u << x)) ? 31 : 0;
					// Handle wide fronts.
					if (x > 0 && (x & 7) == 0)
					{
						srcBitmap++;
						bitmap = *srcBitmap;
					}
				}
				srcBitmap++;
			}
		}

		s_fonts[name] = font;
		return font;
	}

	void freeAll()
	{
		FontMap::iterator iFont = s_fonts.begin();
		for (; iFont != s_fonts.end(); ++iFont)
		{
			FontTFE* font = iFont->second;
			delete []font->imageData;
			delete font;
		}
		s_fonts.clear();
	}

	/////////////////////////////////////////////
	// Dark Forces Embedded System Font
	/////////////////////////////////////////////
	#define BINtoU8(b0,b1,b2,b3,b4,b5,b6,b7) (b0|(b1<<1)|(b2<<2)|(b3<<3)|(b4<<4)|(b5<<5)|(b6<<6)|(b7<<7))

	static const s16 c_sysStart = 0x20, c_sysEnd = 0x7f;
	static const s16 c_sysSpace = 0x05, c_sysFontWidth = 6, c_sysFontHeight = 7;
	// actual size 6x7, font reports height 8.
	static const u8 c_systemFont[] =
	{
		//space.
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x5,
		//!
		0x5f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2,
		//"
		0x0, 0x3, 0x0, 0x3, 0x0, 0x0, 0x5,
		//#
		0x24, 0x7e, 0x24, 0x24, 0x7e, 0x24, 0x7,
		//$
		0x26, 0x49, 0x7f, 0x49, 0x32, 0x0, 0x6,
		//????
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x5,
		//???
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x5,
		//'
		0x03, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2,
		//(
		0x1c, 0x22, 0x41, 0x0, 0x0, 0x0, 0x4,
		//)
		0x41, 0x22, 0x1c, 0x0, 0x0, 0x0, 0x4,
		//*
		0x24, 0x18, 0x7e, 0x18, 0x24, 0x0, 0x6,
		//+
		0x08, 0x08, 0x3e, 0x08, 0x08, 0x0, 0x6,
		//,
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//-
		BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), 0x00, 0x6,
		//?
		BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), 0x00, 0x2,
		// /
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//0
		BINtoU8(0,1,1,1,1,1,0,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(0,1,1,1,1,1,0,0), 0x00, 0x6,
		//1
		BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,0,0), BINtoU8(0,0,0,1,1,1,1,0), 0x00, 0x4,
		//2
		BINtoU8(1,0,0,0,1,1,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(0,1,1,0,0,0,1,0), 0x00, 0x6,
		//3
		BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(0,1,1,0,1,1,0,0), 0x00, 0x6,
		//4
		BINtoU8(1,1,1,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(1,1,1,1,1,1,1,0), 0x00, 0x6,
		//5
		BINtoU8(1,1,1,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,0,1,1,0,0), 0x00, 0x6,
		//6
		BINtoU8(1,1,1,1,1,1,0,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,0,1,1,0,0), 0x00, 0x6,
		//7
		BINtoU8(1,0,0,0,0,0,0,0), BINtoU8(1,0,0,0,0,0,0,0), BINtoU8(1,0,0,0,0,0,0,0), BINtoU8(1,0,0,0,0,0,0,0), BINtoU8(1,1,1,1,1,1,1,0), 0x00, 0x6,
		//8
		BINtoU8(0,1,1,0,1,1,0,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(0,1,1,0,1,1,0,0), 0x00, 0x6,
		//9
		BINtoU8(0,1,1,0,0,0,0,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(0,1,1,1,1,1,0,0), 0x00, 0x6,
		//:
		BINtoU8(0,0,0,0,0,0,0,0), BINtoU8(0,0,1,0,0,1,0,0), BINtoU8(0,0,0,0,0,0,0,0), BINtoU8(0,0,0,0,0,0,0,0), BINtoU8(0,0,0,0,0,0,0,0), 0x00, 0x3,
		//;
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//<
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//=
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//>
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//?
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//@
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//A
		BINtoU8(0,1,1,1,1,1,1,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(0,1,1,1,1,1,1,0), 0x00, 0x6,
		//B
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(0,1,1,0,1,1,0,0), 0x00, 0x6,
		//C
		BINtoU8(0,1,1,1,1,1,0,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), 0x00, 0x6,
		//D
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(0,1,1,1,1,1,0,0), 0x00, 0x6,
		//E
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), 0x00, 0x6,
		//F
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(1,0,0,0,0,0,0,0), 0x00, 0x6,
		//G
		BINtoU8(0,1,1,1,1,1,0,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,1,1,1,1,0), 0x00, 0x6,
		//H
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(1,1,1,1,1,1,1,0), 0x00, 0x6,
		//I
		BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,0,0), BINtoU8(1,1,1,1,1,1,1,0), 0x00, 0x4,
		//J
		BINtoU8(0,0,0,0,0,1,0,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(1,1,1,1,1,1,0,0), 0x00, 0x6,
		//K
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,1,0,1,0,0,0), BINtoU8(1,1,0,0,0,1,1,0), 0x00, 0x6,
		//L
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,1,0), 0x00, 0x6,
		//M
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(0,1,0,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,1,0,0,0,0,0,0), BINtoU8(1,1,1,1,1,1,1,0), 0x00, 0x6,
		//N
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(0,1,0,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(1,1,1,1,1,1,1,0), 0x00, 0x6,
		//O
		BINtoU8(0,1,1,1,1,1,0,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(0,1,1,1,1,1,0,0), 0x00, 0x6,
		//P
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(0,1,1,0,0,0,0,0), 0x00, 0x6,
		//Q
		BINtoU8(0,1,1,1,1,1,0,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,0,1,0), BINtoU8(1,0,0,0,0,1,1,0), BINtoU8(0,1,1,1,1,1,1,0), 0x00, 0x6,
		//R
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(0,1,1,0,1,1,1,0), 0x00, 0x6,
		//S
		BINtoU8(0,1,1,0,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,0,0,1,1,0,0), 0x00, 0x6,
		//T
		BINtoU8(1,0,0,0,0,0,0,0), BINtoU8(1,0,0,0,0,0,0,0), BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(1,0,0,0,0,0,0,0), BINtoU8(1,0,0,0,0,0,0,0), 0x00, 0x6,
		//U
		BINtoU8(1,1,1,1,1,1,0,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(1,1,1,1,1,1,0,0), 0x00, 0x6,
		//V
		BINtoU8(1,1,1,1,1,0,0,0), BINtoU8(0,0,0,0,0,1,0,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,1,0,0), BINtoU8(1,1,1,1,1,0,0,0), 0x00, 0x6,
		//W
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(0,0,0,0,0,1,0,0), BINtoU8(0,0,0,0,1,0,0,0), BINtoU8(0,0,0,0,0,1,0,0), BINtoU8(1,1,1,1,1,1,1,0), 0x00, 0x6,
		//X
		BINtoU8(1,1,0,0,0,1,1,0), BINtoU8(0,0,1,0,1,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,1,0,1,0,0,0), BINtoU8(1,1,0,0,0,1,1,0), 0x00, 0x6,
		//Y
		BINtoU8(1,1,0,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,0,1,1,1,1,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(1,1,0,0,0,0,0,0), 0x00, 0x6,
		//Z
		BINtoU8(1,0,0,0,0,1,1,0), BINtoU8(1,0,0,0,1,0,1,0), BINtoU8(1,0,0,1,0,0,1,0), BINtoU8(1,0,1,0,0,0,1,0), BINtoU8(1,1,0,0,0,0,1,0), 0x00, 0x6,
		//[
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		/* \ */
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//]
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//^
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//_
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//`
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//a
		BINtoU8(0,0,0,1,1,1,0,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,1,1,1,1,0), 0x00, 0x6,
		//b
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,0,1,1,1,0,0), 0x00, 0x6,
		//c
		BINtoU8(0,0,0,1,1,1,0,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,0,0,0,1,0), 0x00, 0x6,
		//d
		BINtoU8(0,0,0,1,1,1,0,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(1,1,1,1,1,1,1,0), 0x00, 0x6,
		//e
		BINtoU8(0,0,0,1,1,1,0,0), BINtoU8(0,0,1,0,1,0,1,0), BINtoU8(0,0,1,0,1,0,1,0), BINtoU8(0,0,1,0,1,0,1,0), BINtoU8(0,0,0,1,1,0,1,0), 0x00, 0x6,
		//f
		BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,1,1,1,1,1,1,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(1,0,0,1,0,0,0,0), BINtoU8(0,0,0,0,0,0,0,0), 0x00, 0x5,
		//g
		BINtoU8(0,0,0,1,1,0,0,0), BINtoU8(0,0,1,0,0,1,0,1), BINtoU8(0,0,1,0,0,1,0,1), BINtoU8(0,0,1,0,0,1,0,1), BINtoU8(0,0,0,1,1,1,1,0), 0x00, 0x6,
		//h
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,0,1,1,1,1,0), 0x00, 0x6,
		//i
		BINtoU8(1,0,1,1,1,1,1,0), BINtoU8(0,0,0,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,0,1,1,1,1,0), 0x00, 0x2,
		//j
		BINtoU8(0,0,0,0,0,0,0,1), BINtoU8(0,0,0,0,0,0,0,1), BINtoU8(1,0,1,1,1,1,1,0), BINtoU8(0,0,0,0,0,0,0,0), BINtoU8(0,0,0,1,1,1,1,0), 0x00, 0x4,
		//k
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(0,0,0,0,1,0,0,0), BINtoU8(0,0,0,1,0,1,0,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,0,0), 0x00, 0x5,
		//l
		BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(0,0,0,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,0,1,1,1,1,0), 0x00, 0x2,
		//m
		BINtoU8(0,0,1,1,1,1,1,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,1,1,1,1,1,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,0,1,1,1,1,0), 0x00, 0x6,
		//n
		BINtoU8(0,0,1,1,1,1,1,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,0,1,1,1,1,0), 0x00, 0x6,
		//o
		BINtoU8(0,0,0,1,1,1,0,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,0,1,1,1,0,0), 0x00, 0x6,
		//p
		BINtoU8(0,0,1,1,1,1,1,1), BINtoU8(0,0,1,0,0,1,0,0), BINtoU8(0,0,1,0,0,1,0,0), BINtoU8(0,0,1,0,0,1,0,0), BINtoU8(0,0,0,1,1,0,0,0), 0x00, 0x6,
		//q
		BINtoU8(0,0,0,1,1,0,0,0), BINtoU8(0,0,1,0,0,1,0,0), BINtoU8(0,0,1,0,0,1,0,0), BINtoU8(0,0,1,0,0,1,0,0), BINtoU8(0,0,1,1,1,1,1,1), 0x00, 0x6,
		//r
		BINtoU8(0,0,1,1,1,1,1,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,0,0,0,0,0,0), 0x00, 0x5,
		//s
		BINtoU8(0,0,0,1,0,0,1,0), BINtoU8(0,0,1,0,1,0,1,0), BINtoU8(0,0,1,0,1,0,1,0), BINtoU8(0,0,1,0,1,0,1,0), BINtoU8(0,0,1,0,0,1,0,0), 0x00, 0x6,
		//t
		BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(1,1,1,1,1,1,1,0), BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), 0x00, 0x5,
		//u
		BINtoU8(0,0,1,1,1,1,0,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,1,1,1,1,0,0), 0x00, 0x6,
		//v
		BINtoU8(0,0,1,1,1,0,0,0), BINtoU8(0,0,0,0,0,1,0,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,1,0,0), BINtoU8(0,0,1,1,1,0,0,0), 0x00, 0x6,
		//w
		BINtoU8(0,0,1,1,1,1,1,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,1,1,1,1,1,0), BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,1,1,1,1,0,0), 0x00, 0x6,
		//x
		BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,0,1,0,1,0,0), BINtoU8(0,0,0,0,1,0,0,0), BINtoU8(0,0,0,1,0,1,0,0), BINtoU8(0,0,1,0,0,0,1,0), 0x00, 0x6,
		//y
		BINtoU8(0,0,1,0,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,0,1,1,1,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,1,0,0,0,0,0), 0x00, 0x6,
		//z
		BINtoU8(0,0,1,0,0,0,1,0), BINtoU8(0,0,1,0,0,1,1,0), BINtoU8(0,0,1,0,1,0,1,0), BINtoU8(0,0,1,1,0,0,1,0), BINtoU8(0,0,1,0,0,0,1,0), 0x00, 0x6,
		//{
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//|
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//}
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//~
		0x28, 0x7E, 0x28, 0x28, 0x7E, 0x28, 0x7,
		//.
		BINtoU8(0,0,0,0,0,0,1,0), BINtoU8(0,0,0,0,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), BINtoU8(0,0,0,1,0,0,0,0), 0x00, 0x2,
	};

	FontTFE* createSystemFont6x8()
	{
		const char* name = "SystemFont";
		FontMap::iterator iFont = s_fonts.find(name);
		if (iFont != s_fonts.end())
		{
			return iFont->second;
		}

		// Create the font.
		FontTFE* font = new FontTFE;
		memset(font, 0, sizeof(FontTFE));

		font->startChar = c_sysStart;
		font->endChar   = c_sysEnd;
		font->height    = c_sysFontHeight;
		font->charCount = font->endChar - font->startChar + 1;

		//character widths.
		const s32 count = font->charCount;
		u32 pixelCount = 0u;
		// Why is the data invalid? - only the font height seems to work.
		font->maxWidth = c_sysFontWidth;
		for (s32 i = 0; i < count; i++)
		{
			font->step[i]  = c_systemFont[i * 7 + 6];
			font->width[i] = (font->step[i] < c_sysFontWidth) ? font->step[i] : c_sysFontWidth;
			pixelCount += font->width[i] * font->height;
		}

		font->imageData = new u8[pixelCount];
		memset(font->imageData, 0, pixelCount);

		u32 imageOffset = 0u;
		const u8* srcBitmap = c_systemFont;
		for (s32 i = 0; i < count; i++)
		{
			const u8 width = font->width[i];
			u8* dstImage = font->imageData + imageOffset;
			font->imageOffset[i] = imageOffset;
			imageOffset += width * font->height;

			for (u32 x = 0; x < width; x++)
			{
				u8 bitmap = srcBitmap[x];
				for (u32 y = 0; y < font->height; y++)
				{
					dstImage[x * font->height + font->height - y - 1] = (bitmap & (1u << y)) ? 255 : 0;
				}
			}
			srcBitmap += 7;
		}

		s_fonts[name] = font;
		return font;
	}
}