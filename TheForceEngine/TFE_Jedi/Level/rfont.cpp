#include <cstring>

#include "rfont.h"
#include "rtexture.h"
#include <TFE_Game/igame.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/filestream.h>

namespace TFE_Jedi
{
	enum FontConstants
	{
		FNT_VERSION = 0x15,
	};

	Font* font_load(FilePath* filePath)
	{
		FileStream file;
		if (!file.open(filePath, FileStream::MODE_READ))
		{
			TFE_System::logWrite(LOG_ERROR, "Font", "Failed to open font.");
			return nullptr;
		}

		char hdr[4];
		file.readBuffer(hdr, 3);
		hdr[3] = 0;
		if (strncmp(hdr, "FNT", 3))
		{
			file.close();
			TFE_System::logWrite(LOG_ERROR, "Font", "Invalid font header - '%s'", hdr);
			return nullptr;
		}

		u8 version;
		file.read(&version);
		if (version != FNT_VERSION)
		{
			file.close();
			TFE_System::logWrite(LOG_ERROR, "Font", "Invalid font version: %d, should be %d", version, FNT_VERSION);
			return nullptr;
		}

		Font* font = (Font*)game_alloc(sizeof(Font));
		file.read(&font->vertSpacing);
		file.read(&font->horzSpacing);
		file.read(&font->width);
		file.read(&font->height);
		file.read(&font->minChar);
		file.read(&font->maxChar);

		// Unused space.
		u8 buffer[22];
		file.readBuffer(buffer, 22);

		s32 glyphCount = s32(font->maxChar) - s32(font->minChar) + 1;
		font->glyphs = (TextureData*)game_alloc(sizeof(TextureData) * glyphCount);
		memset(font->glyphs, 0, sizeof(TextureData) * glyphCount);

		TextureData* glyph = font->glyphs;
		for (s32 i = 0; i < glyphCount; i++, glyph++)
		{
			u8 width;
			file.read(&width);

			s32 pixelCount = width * font->vertSpacing;
			glyph->image = (u8*)game_alloc(pixelCount);
			file.readBuffer(glyph->image, pixelCount);

			glyph->width = width;
			glyph->height = font->vertSpacing;
			glyph->dataSize = pixelCount;
			glyph->flags = 9;
			glyph->compressed = 0;
		}
		file.close();

		return font;
	}
}