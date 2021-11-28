#include <climits>
#include <cstring>

#include "textureAsset.h"
#include <TFE_System/system.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Archive/archive.h>
#include <assert.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

namespace TFE_Texture
{
	#pragma pack(push)
	#pragma pack(1)
	struct BM_Header
	{
		char MAGIC[4];	// = 'BM ' + 0x1E
		s16 SizeX;		// if = 1 then multiple BM in the file
		s16 SizeY;		// EXCEPT if SizeY also = 1, in which case
						// it is a 1x1 BM
		s16 idemX;		// portion of texture actually used
		s16 idemY;		// portion of texture actually used

		u8 flags;
		u8 logSizeY;        // logSizeY = log2(SizeY)
                            // logSizeY = 0 for weapons

		s16 compressed;     // 0 = not compressed, 1 = compressed (RLE), 2 = compressed (RLE0)
		s32 dataSize;       // Data size for compressed BM
                            // excluding header and columns starts table
                            // If not compressed, DataSize is unused
		u8 pad1[12];		// 12 times 0x00
	};

	struct BM_SubHeader
	{
		s16 SizeX;		// if = 1 then multiple BM in the file
		s16 SizeY;		// EXCEPT if SizeY also = 1, in which case
						// it is a 1x1 BM
		s16 idemX;		// portion of texture actually used
		s16 idemY;		// portion of texture actually used

		s32 dataSize;	// Data size for compressed BM
						// excluding header and columns starts table
						// If not compressed, DataSize is unused

		u8 logSizeY;	// logSizeY = log2(SizeY)
						// logSizeY = 0 for weapons
		u8 pad1[3];
		u8 u1[3];
		u8 pad2[5];

		// 4 bytes
		u8  flags;
		s16 compressed; // 0 = not compressed, 1 = compressed (RLE), 2 = compressed (RLE0)
		u8  pad3;
	};

	#define PCX_MAGIC 0x0A
	struct PcxHeader
	{
		u8 magic;			// should be PCX_MAGIC
		u8 version;
		u8 encoding;		// 0 = uncompressed, 1 = rle
		u8 bitsPerPixel;	// bits per pixel per plane.
		u16 xMin;
		u16 yMin;
		u16 xMax;
		u16 yMax;
		u16 horizontalDPI;
		u16 verticalDPI;
		u8  egaPal[48];
		u8  reserved1;
		u8  colorPlaneCount;
		u16 colorPlaneStride;	// number of bytes per scanline for a single color plane.
		u16 palMode;			// 1 = monochrome/color, 2 = grayscale
		u16 srcScreenWidth;
		u16 srcScreenHeight;
		u8  reserved2[54];
	};
	#pragma pack(pop)

	typedef std::map<std::string, Texture*> TextureMap;
	static TextureMap s_textures;
	static std::vector<u8> s_buffer;
	static const char* c_defaultGob = "TEXTURES.GOB";
	// Used to store the most recent palette.
	static Palette256 s_tempPal = { 0 };

	void loadTextureFrame(s32 w, s32 h, s16 compressed, s32 dataSize, const u8* srcData, u8* dstImage)
	{
		assert(srcData && dstImage);
		const s32* col = (s32*)&srcData[dataSize];

		if (compressed == 0)
		{
			memcpy(dstImage, srcData, w * h);
			return;
		}
		else if (compressed == 1)
		{
			for (s32 x = 0; x < w; x++)
			{
				const u8 *colData = &srcData[col[x]];
				s32 y = 0, i = 0;
				while (y < h)
				{
					const s32 n = (s32)colData[i];
					i++;
					if (n <= 128)
					{
						for (s32 ii = 0; ii < n; ii++, y++)
						{
							dstImage[x*h + y] = colData[ii + i];
						}
						i += n;
					}
					else
					{
						const u8 c = colData[i];
						i++;
						for (s32 ii = 0; ii < n - 128; ii++, y++)
						{
							dstImage[x*h + y] = c;
						}
					}
				}
			}
			return;
		}
		// Compressed >= 2
		for (s32 x = 0; x < w; x++)
		{
			const u8* colData = &srcData[col[x]];
			s32 y = 0, i = 0;
			while (y < h)
			{
				s32 n = (s32)colData[i];
				if (n <= 128)
				{
					i++;
					for (s32 ii = 0; ii < n; ii++, y++)
					{
						dstImage[x*h + y] = colData[ii + i];
					}
					i += n;
				}
				else
				{
					i++;
					for (int ii = 0; ii < n - 128; ii++, y++)
					{
						dstImage[x*h + y] = 0;
					}
				}
			}
		}
	}

	Texture* get(const char* name)
	{
		TextureMap::iterator iTex = s_textures.find(name);
		if (iTex != s_textures.end())
		{
			return iTex->second;
		}

		// It doesn't exist yet, try to load the texture.
		if (!TFE_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, name, s_buffer))
		{
			return nullptr;
		}

		// Then read out the data.
		const BM_Header* header = (BM_Header*)s_buffer.data();
		const u8* data = s_buffer.data() + sizeof(BM_Header);

		Texture* texture = new Texture();
		strcpy(texture->name, name);
		size_t allocSize = 0u;

		// First determine the amount of memory to allocate.
		s32 frameCount = 1;
		s32 frameRate = 0;
		if (header->SizeX == 1 && header->SizeY != 1)
		{
			// This is multiple textures packed together for animation.
			s32 subBM_FileSizem32 = header->SizeY;
			frameCount = header->idemY;
			frameRate = data[0];
			s32* offsets = (s32*)&data[2];

			allocSize += sizeof(TextureFrame) * frameCount;
			for (s32 f = 0; f < frameCount; f++)
			{
				BM_SubHeader* frame = (BM_SubHeader*)&data[offsets[f] + 2];
				allocSize += frame->SizeX * frame->SizeY;
			}
		}
		else
		{
			allocSize += sizeof(TextureFrame);
			allocSize += header->SizeX * header->SizeY;
		}
				
		// Allocate memory.
		texture->memory = new u8[allocSize];
		texture->frames = (TextureFrame*)texture->memory;
		texture->frameCount = frameCount;
		texture->frameRate = frameRate;
		texture->layout = TEX_LAYOUT_VERT;
		size_t offset = sizeof(TextureFrame) * frameCount;

		// Then pass through again to decompress.
		if (header->SizeX == 1 && header->SizeY != 1)
		{
			// This is multiple textures packed together for animation.
			s32 subBM_FileSizem32 = header->SizeY;
			s32 frameCount = header->idemY;
			s32 frameRate = data[0];
			s32* offsets = (s32*)&data[2];

			for (s32 f = 0; f < frameCount; f++)
			{
				BM_SubHeader* frame = (BM_SubHeader*)&data[offsets[f] + 2];
				u8* imageData = (u8*)frame + sizeof(BM_SubHeader);
				
				texture->frames[f].width = frame->SizeX;
				texture->frames[f].height = frame->SizeY;
				texture->frames[f].logSizeY = frame->logSizeY;

				// I'm not sure yet what the other flags do, but 8 refers to a transparent texture as seen in the DOS code.
				texture->frames[f].opacity = (frame->flags&8) ? TEX_OPACITY_TRANS : TEX_OPACITY_NORMAL;

				texture->frames[f].uvWidth = frame->idemX;
				texture->frames[f].uvHeight = frame->idemY;
				texture->frames[f].offsetX = 0;
				texture->frames[f].offsetY = 0;
				
				texture->frames[f].image = texture->memory + offset;
				offset += frame->SizeX * frame->SizeY;

				loadTextureFrame(frame->SizeX, frame->SizeY, frame->compressed, frame->dataSize, imageData, texture->frames[f].image);
			}
		}
		else
		{
			texture->frames[0].image = texture->memory + offset;
			offset += header->SizeX * header->SizeY;

			texture->frames[0].width  = header->SizeX;
			texture->frames[0].height = header->SizeY;
			texture->frames[0].logSizeY = header->logSizeY;
			texture->frames[0].offsetX = 0;
			texture->frames[0].offsetY = 0;

			// I'm not sure yet what the other flags do, but 8 refers to a transparent texture as seen in the DOS code.
			texture->frames[0].opacity = (header->flags & 8) ? TEX_OPACITY_TRANS : TEX_OPACITY_NORMAL;

			texture->frames[0].uvWidth = header->idemX;
			texture->frames[0].uvHeight = header->idemY;

			loadTextureFrame(header->SizeX, header->SizeY, header->compressed, header->dataSize, data, texture->frames[0].image);
		}

		s_textures[name] = texture;
		return texture;
	}

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

	void loadDeltIntoFrame(TextureFrame* frame, u8** imageDst, const u8* buffer, u32 size)
	{
		DeltHeader header = *((DeltHeader*)buffer);
		header.sizeX++;
		header.sizeY++;

		frame->width    = header.sizeX;
		frame->height   = header.sizeY;
		frame->logSizeY = 0;
		frame->opacity  = TEX_OPACITY_TRANS;
		frame->uvWidth  = header.sizeX;
		frame->uvHeight = header.sizeY;
		frame->image    = *imageDst;

		frame->offsetX = header.offsetX;
		frame->offsetY = header.offsetY;

		memset(frame->image, 0, header.sizeX * header.sizeY);
		(*imageDst) += header.sizeX * header.sizeY;

		const u8* data = buffer + sizeof(DeltHeader);
		const u8* end = buffer + size;
		while (data < end)
		{
			const DeltLine* line = (DeltLine*)data;
			data += sizeof(DeltLine);

			const s32 startX = line->xStart;
			const s32 startY = line->yStart;//header.sizeY - (line->yStart) - 1;
			const bool rle = (line->sizeAndType & 1) ? true : false;
			s32 pixelCount = (line->sizeAndType >> 1) & 0x3FFF;

			u8* image = frame->image + startX + startY * header.sizeX;
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

	Texture* getFromDelt(const char* name, const char* archivePath)
	{
		TextureMap::iterator iTex = s_textures.find(name);
		if (iTex != s_textures.end())
		{
			return iTex->second;
		}

		// It doesn't exist yet, try to load the palette.
		if (!TFE_AssetSystem::readAssetFromArchive(archivePath, name, s_buffer))
		{
			return nullptr;
		}

		// Then read out the data.
		DeltHeader header = *((DeltHeader*)s_buffer.data());
		header.sizeX++;
		header.sizeY++;

		Texture* texture = new Texture();
		strcpy(texture->name, name);
		
		u32 allocSize = 0u;
		allocSize += sizeof(TextureFrame);
		allocSize += header.sizeX * header.sizeY;
		// Allocate memory.
		texture->memory = new u8[allocSize];
		texture->frames = (TextureFrame*)texture->memory;
		texture->frameCount = 1;
		texture->frameRate = 0;
		texture->layout = TEX_LAYOUT_HORZ;

		size_t offset = sizeof(TextureFrame);
		texture->frames[0].image = texture->memory + offset;
		memset(texture->frames[0].image, 0, header.sizeX * header.sizeY);

		u8* imageDst = texture->memory + offset;
		loadDeltIntoFrame(&texture->frames[0], &imageDst, s_buffer.data(), (u32)s_buffer.size());

		s_textures[name] = texture;
		return texture;
	}

	Texture* getFromAnim(const char* name, const char* archivePath)
	{
		TextureMap::iterator iTex = s_textures.find(name);
		if (iTex != s_textures.end())
		{
			return iTex->second;
		}

		// It doesn't exist yet, try to load the anim.
		if (!TFE_AssetSystem::readAssetFromArchive(archivePath, name, s_buffer))
		{
			return nullptr;
		}

		const s16 frameCount = *((s16*)s_buffer.data());
		const u8* frames = s_buffer.data() + 2;

		// First compute the total size.
		u32 allocSize = sizeof(TextureFrame) * frameCount;
		for (s32 i = 0; i < frameCount; i++)
		{
			u32 size = *((u32*)frames); frames += 4;
			DeltHeader header = *((DeltHeader*)frames);
			header.sizeX++;
			header.sizeY++;
			allocSize += header.sizeX*header.sizeY;

			frames += size;
		}

		Texture* texture = new Texture();
		strcpy(texture->name, name);

		// Allocate memory.
		texture->memory = new u8[allocSize];
		texture->frames = (TextureFrame*)texture->memory;
		texture->frameCount = frameCount;
		texture->frameRate = 15;	// arbitrary.
		texture->layout = TEX_LAYOUT_HORZ;

		// Then load the DELTs.
		frames = s_buffer.data() + 2;
		u8* imageDst = texture->memory + sizeof(TextureFrame) * frameCount;
		for (s32 i = 0; i < frameCount; i++)
		{
			u32 size = *((u32*)frames); frames += 4;
			loadDeltIntoFrame(&texture->frames[i], &imageDst, frames, size);
			frames += size;
		}

		s_textures[name] = texture;
		return texture;
	}

	Texture* getFromPCX(const char* name, const char* archivePath)
	{
		TextureMap::iterator iTex = s_textures.find(name);
		if (iTex != s_textures.end())
		{
			return iTex->second;
		}

		// It doesn't exist yet, try to load the anim.
		if (!TFE_AssetSystem::readAssetFromArchive(archivePath, name, s_buffer))
		{
			return nullptr;
		}

		const u8* buffer = s_buffer.data();
		const PcxHeader* header = (PcxHeader*)buffer;
		const u32 width  = header->xMax - header->xMin + 1;
		const u32 height = header->yMax - header->yMin + 1;
		// Currently only 8bit PCX files are supported.
		if (header->bitsPerPixel != 8 || header->colorPlaneCount != 1)
		{
			return nullptr;
		}

		const u8* pixelData = buffer + sizeof(PcxHeader);
		const u32 imageSize = u32(s_buffer.size() - 769 - sizeof(PcxHeader));

		Texture* texture = new Texture();
		strcpy(texture->name, name);

		// Allocate memory.
		texture->memory = new u8[sizeof(TextureFrame) + width * height];
		texture->frames = (TextureFrame*)texture->memory;
		texture->frameCount = 1;
		texture->frameRate = 0;	// arbitrary.
		texture->layout = TEX_LAYOUT_HORZ;
		texture->frames[0].image = texture->memory + sizeof(TextureFrame);
		u8* imageOut = texture->frames[0].image;

		texture->frames[0].width = width;
		texture->frames[0].height = height;
		texture->frames[0].logSizeY = 0;
		texture->frames[0].offsetX = 0;
		texture->frames[0].offsetY = 0;
		texture->frames[0].opacity = TEX_OPACITY_NORMAL;
		texture->frames[0].uvWidth = width;
		texture->frames[0].uvHeight = height;
		
		if (header->encoding == 1)
		{
			for (u32 y = 0; y < height; y++)
			{
				u8* dst = &imageOut[y * width];
				for (u32 x = 0; x < width;)
				{
					u8 c = *pixelData;
					pixelData++;
					if (c < 192)
					{
						dst[x++] = c;
					}
					else
					{
						u8 count = c & 63;
						c = *pixelData;
						pixelData++;
						for (u32 i = 0; i < count; i++)
						{
							dst[x++] = c;
						}
					}
				}
			}
		}
				
		// Verify that the palette is valid.
		assert(buffer[s_buffer.size() - 769] == 0x0C);
		const u8* pal = buffer + s_buffer.size() - 768;

		// Load the palette which can be retrieved later.
		for (u32 i = 0; i < 256; i++, pal += 3)
		{
			s_tempPal.colors[i] = pal[0] | (pal[1] << 8) | (pal[2] << 16) | (0xff << 24);
		}

		s_textures[name] = texture;
		return texture;
	}

	u8 findClosestPaletteMatch(u8 r, u8 g, u8 b, const u32* colors)
	{
		// Slow method.
		s32 minDistManhattan = INT_MAX;
		u8 index = 0;
		for (s32 i = 1; i < 256; i++)
		{
			const u8 palR = colors[i] & 0xffu;
			const u8 palG = (colors[i] >> 8u) & 0xff;
			const u8 palB = (colors[i] >> 16u) & 0xff;

			const s32 dR = abs(s32(r) - s32(palR));
			const s32 dG = abs(s32(g) - s32(palG));
			const s32 dB = abs(s32(b) - s32(palB));

			const s32 distM = dR + dG + dB;
			if (distM < minDistManhattan)
			{
				minDistManhattan = distM;
				index = i;
			}
		}

		return index;
	}

	Texture* convertImageToTexture_8bit(const char* name, const Image* image, const char* paletteName)
	{
		TextureMap::iterator iTex = s_textures.find(name);
		if (iTex != s_textures.end())
		{
			return iTex->second;
		}

		const Palette256* pal = TFE_Palette::get256(paletteName);
		if (!pal)
		{
			pal = TFE_Palette::getDefault256();
			if (!pal) { return nullptr; }
		}

		Texture* texture = new Texture();
		strcpy(texture->name, name);

		// Allocate memory.
		texture->memory = new u8[sizeof(TextureFrame) + image->width * image->height];
		texture->frames = (TextureFrame*)texture->memory;
		texture->frameCount = 1;
		texture->frameRate = 0;	// arbitrary.
		texture->layout = TEX_LAYOUT_HORZ;
		texture->frames[0].image = texture->memory + sizeof(TextureFrame);
		u8* imageOut = texture->frames[0].image;

		texture->frames[0].width = image->width;
		texture->frames[0].height = image->height;
		texture->frames[0].logSizeY = 0;
		texture->frames[0].offsetX = 0;
		texture->frames[0].offsetY = 0;
		texture->frames[0].opacity = TEX_OPACITY_TRANS;
		texture->frames[0].uvWidth = image->width;
		texture->frames[0].uvHeight = image->height;

		// For now look for the closest "manhattan" match.
		s32 pixelCount = image->width * image->height;
		for (s32 i = 0; i < pixelCount; i++)
		{
			u8 srcR = image->data[i] & 0xffu;
			u8 srcG = (image->data[i] >> 8u) & 0xff;
			u8 srcB = (image->data[i] >> 16u) & 0xff;
			u8 srcA = (image->data[i] >> 24u) & 0xff;

			if (srcA < 128u)
			{
				imageOut[i] = 0;
			}
			else
			{
				imageOut[i] = findClosestPaletteMatch(srcR, srcG, srcB, pal->colors);
			}
		}

		s_textures[name] = texture;
		return texture;
	}

	Palette256* getPreviousPalette()
	{
		return &s_tempPal;
	}

	void free(Texture* texture)
	{
		if (!texture) { return; }
		delete[] texture->memory;

		TextureMap::iterator iTex = s_textures.begin();
		for (; iTex != s_textures.end(); ++iTex)
		{
			if (iTex->second == texture)
			{
				s_textures.erase(iTex);
				break;
			}
		}
	}

	void freeAll()
	{
		TextureMap::iterator iTex = s_textures.begin();
		for (; iTex != s_textures.end(); ++iTex)
		{
			Texture* texture = iTex->second;
			if (texture)
			{
				delete[] texture->memory;
			}
		}
		s_textures.clear();
	}
}
