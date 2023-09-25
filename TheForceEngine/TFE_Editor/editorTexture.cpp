#include "editorTexture.h"
#include "editor.h"
#include <TFE_DarkForces/mission.h>
#include <TFE_System/system.h>
#include <TFE_Archive/archive.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <algorithm>
#include <vector>
#include <map>

namespace TFE_Editor
{
	typedef std::map<std::string, s32> TextureMap;
	typedef std::vector<EditorTexture> TextureList;
		
	static TextureMap s_texturesLoaded;
	static TextureList s_textureList;
	static u32 s_palette[256];
	static u8 s_identityTable[256];
	static bool s_rebuildIdentityTable = true;
	static const u8* s_remapTable = s_identityTable;

	void freeTexture(const char* name)
	{
		const size_t count = s_textureList.size();
		const EditorTexture* texture = s_textureList.data();
		for (size_t i = 0; i < count; i++, texture++)
		{
			if (strcasecmp(texture->name, name) == 0)
			{
				for (s32 f = 0; f < texture->frameCount; f++)
				{
					TFE_RenderBackend::freeTexture(texture->texGpu[f]);
				}
				s_textureList.erase(s_textureList.begin() + i);
				break;
			}
		}
		s_texturesLoaded.clear();
		const size_t newCount = s_textureList.size();
		for (size_t i = 0; i < newCount; i++, texture++)
		{
			s_texturesLoaded[s_textureList[i].name] = i;
		}
	}

	void freeCachedTextures()
	{
		const size_t count = s_textureList.size();
		const EditorTexture* texture = s_textureList.data();
		for (size_t i = 0; i < count; i++, texture++)
		{
			for (s32 f = 0; f < texture->frameCount; f++)
			{
				TFE_RenderBackend::freeTexture(texture->texGpu[f]);
			}
		}
		s_texturesLoaded.clear();
		s_textureList.clear();
	}

	s32 findTexture(const char* name)
	{
		TextureMap::iterator iTex = s_texturesLoaded.find(name);
		if (iTex != s_texturesLoaded.end())
		{
			return iTex->second;
		}
		return -1;
	}

	s32 allocateTexture(const char* name)
	{
		s32 index = (s32)s_textureList.size();
		s_textureList.push_back({});
		s_texturesLoaded[name] = index;
		return index;
	}

	void buildIdentityTable()
	{
		for (s32 i = 0; i < 256; i++) { s_identityTable[i] = i; }
	}

	TextureGpu* loadBmFrame(u32 width, u32 height, const u8* image, const u32* palette)
	{
		if (s_rebuildIdentityTable)
		{
			s_rebuildIdentityTable = false;
			buildIdentityTable();
		}

		WorkBuffer& buffer = getWorkBuffer();
		buffer.resize(width * height * 4);
		u32* imageBuffer = (u32*)buffer.data();
		const u8* srcImage = image;

		for (s32 x = 0; x < width; x++, srcImage += height)
		{
			const u8* column = srcImage;
			for (s32 y = 0; y < height; y++)
			{
				u8 palIndex = column[y];
				imageBuffer[y*width + x] = palette[s_remapTable[palIndex]];
			}
		}
		return TFE_RenderBackend::createTexture(width, height, imageBuffer);
	}
		
	bool loadEditorTexture(SourceType type, Archive* archive, const char* filename, const u32* palette, s32 palIndex, EditorTexture* texture)
	{
		if (!archive && !filename) { return false; }
		if (!texture) { return false; }

		s32 id = findTexture(filename);
		if (id >= 0)
		{
			*texture = s_textureList[id];
			return true;
		}

		if (!archive->openFile(filename))
		{
			return false;
		}
		size_t len = archive->getFileLength();
		if (!len)
		{
			archive->closeFile();
			return false;
		}
		WorkBuffer& buffer = getWorkBuffer();
		buffer.resize(len);
		archive->readFile(buffer.data(), len);
		archive->closeFile();

		if (type == TEX_BM)
		{
			bool result = false;
			TextureData* texData = bitmap_loadFromMemory(buffer.data(), len, 1);
			if (texData)
			{
				// What about animated textures?
				if (texData->uvWidth == BM_ANIMATED_TEXTURE)
				{
					u8 frameRate  = texData->image[0];
					u8 animatedId = texData->image[1];
					u8 frameCount = texData->uvHeight;
					if (animatedId != 2)
					{
						free(texData);
						return false;
					}

					id = allocateTexture(filename);
					s_textureList[id].frameCount = frameCount;
					
					const u32* textureOffsets = (u32*)(texData->image + 2);
					const u8* base = texData->image + 2;
					for (s32 i = 0; i < frameCount; i++)
					{
						const TextureData* frame = (TextureData*)(base + textureOffsets[i]);
						TextureData outFrame;

						// Allocate an image buffer since everything no longer fits nicely.
						u8* image = (u8*)frame + 0x1c;

						// We have to make sure the structure offsets line up with DOS...
						outFrame.flags = *((u8*)frame + 0x18);
						outFrame.compressed = *((u8*)frame + 0x19);
						outFrame.animIndex = 0;
						outFrame.frameIdx = i;
						outFrame.animPtr = nullptr;
						outFrame.animSetup = 1;
						outFrame.palIndex = 1;
						outFrame.columns = nullptr;
						s_textureList[id].texGpu[i] = loadBmFrame(frame->width, frame->height, image, palette);

						if (i == 0)
						{
							s_textureList[id].width  = frame->width;
							s_textureList[id].height = frame->height;
						}
					}
				}
				else
				{
					id = allocateTexture(filename);
					s_textureList[id].width  = texData->width;
					s_textureList[id].height = texData->height;
					s_textureList[id].frameCount = 1;
					s_textureList[id].texGpu[0] = loadBmFrame(texData->width, texData->height, texData->image, palette);
				}
				strcpy(s_textureList[id].name, filename);
				s_textureList[id].paletteIndex = palIndex;
				s_textureList[id].lightLevel = 32;

				*texture = s_textureList[id];
				result = true;
			}
			if (texData) { free(texData->image); }
			free(texData);

			return result;
		}

		return false;
	}

	bool loadEditorTextureLit(SourceType type, Archive* archive, const char* filename, const u32* palette, s32 palIndex, const u8* colormap, s32 lightLevel, EditorTexture* texture)
	{
		s_remapTable = &colormap[lightLevel * 256];
		bool result = loadEditorTexture(type, archive, filename, palette, palIndex, texture);
		if (result)
		{
			texture->lightLevel = lightLevel;
		}
		s_remapTable = s_identityTable;

		return result;
	}
}