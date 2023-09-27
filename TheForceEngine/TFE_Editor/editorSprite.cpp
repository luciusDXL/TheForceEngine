#include "editorSprite.h"
#include "editor.h"
#include "editorColormap.h"
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_DarkForces/mission.h>
#include <TFE_System/system.h>
#include <TFE_Archive/archive.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Renderer/rcommon.h>
#include <algorithm>
#include <vector>
#include <map>

namespace TFE_Editor
{
	typedef std::map<std::string, s32> SpriteMap;
	typedef std::vector<EditorSprite> SpriteList;
		
	static SpriteMap s_spritesLoaded;
	static SpriteList s_spriteList;
	static std::map<WaxCell*, s32> s_waxDataMap;
	static std::vector<WaxCell*> s_waxDataList;
	
	void freeSprite(const char* name)
	{
		const size_t count = s_spriteList.size();
		const EditorSprite* sprite = s_spriteList.data();
		for (size_t i = 0; i < count; i++, sprite++)
		{
			if (strcasecmp(sprite->name, name) == 0)
			{
				TFE_RenderBackend::freeTexture(sprite->texGpu);
				s_spriteList.erase(s_spriteList.begin() + i);
				break;
			}
		}
		s_spritesLoaded.clear();
		const s32 newCount = (s32)s_spriteList.size();
		for (s32 i = 0; i < newCount; i++)
		{
			s_spritesLoaded[s_spriteList[i].name] = i;
		}
	}

	void freeCachedSprites()
	{
		const size_t count = s_spriteList.size();
		const EditorSprite* sprite = s_spriteList.data();
		for (size_t i = 0; i < count; i++, sprite++)
		{
			TFE_RenderBackend::freeTexture(sprite->texGpu);
		}
		s_spritesLoaded.clear();
		s_spriteList.clear();
	}

	s32 findSprite(const char* name)
	{
		SpriteMap::iterator iSprite = s_spritesLoaded.find(name);
		if (iSprite != s_spritesLoaded.end())
		{
			return iSprite->second;
		}
		return -1;
	}

	s32 allocateSprite(const char* name)
	{
		s32 index = (s32)s_spriteList.size();
		s_spriteList.push_back({});
		s_spritesLoaded[name] = index;
		return index;
	}

	bool findWaxCellInMap(WaxCell* cell, s32& id)
	{
		std::map<WaxCell*, s32>::iterator iCell = s_waxDataMap.find(cell);
		if (iCell != s_waxDataMap.end())
		{
			id = iCell->second;
			return true;
		}
		return false;
	}

	void insertWaxCellIntoMap(WaxCell* cell, s32 id)
	{
		s_waxDataMap[cell] = id;
		s_waxDataList.push_back(cell);
	}

	void writeCellToImage(s32 u, s32 v, s32 w, s32 h, s32 stride, const void* basePtr, const WaxCell* cell, const u32* palette, u32* imageBuffer)
	{
		u32* outImage = &imageBuffer[v * stride];

		const u8* imageData = (u8*)cell + sizeof(WaxCell);
		const u8* image = (cell->compressed == 1) ? imageData + (cell->sizeX * sizeof(u32)) : imageData;

		u8 columnWorkBuffer[WAX_DECOMPRESS_SIZE];
		const u32* columnOffset = (u32*)((u8*)basePtr + cell->columnOffset);

		for (s32 x = 0; x < cell->sizeX; x++)
		{
			u8* column = (u8*)image + columnOffset[x];
			if (cell->compressed)
			{
				const u8* colPtr = (u8*)cell + columnOffset[x];
				sprite_decompressColumn(colPtr, columnWorkBuffer, cell->sizeY);
				column = columnWorkBuffer;
			}

			for (s32 y = 0; y < cell->sizeY; y++)
			{
				outImage[y*stride + x + u] = column[y] ? palette[s_remapTable[column[y]]] : 0;
			}
		}
	}
				
	bool loadEditorSprite(SpriteSourceType type, Archive* archive, const char* filename, const u32* palette, s32 palIndex, EditorSprite* sprite)
	{
		if (!archive && !filename) { return false; }
		if (!sprite) { return false; }
		buildIdentityTable();

		s32 id = findSprite(filename);
		if (id >= 0)
		{
			*sprite = s_spriteList[id];
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

		if (type == SPRITE_WAX)
		{
			JediWax* wax = TFE_Sprite_Jedi::loadWaxFromMemory(buffer.data(), len);
			if (!wax)
			{
				return false;
			}

			// Allocate
			id = allocateSprite(filename);
			EditorSprite* outSprite = &s_spriteList[id];
			outSprite->anim.clear();
			outSprite->frame.clear();
			outSprite->cell.clear();
			outSprite->texGpu = nullptr;

			// First gather all of the cells.
			s_waxDataMap.clear();
			s_waxDataList.clear();
			for (s32 animId = 0; animId < wax->animCount; animId++)
			{
				WaxAnim* anim = WAX_AnimPtr(wax, animId);
				if (!anim) { continue; }

				SpriteAnim outAnim;
				outAnim.frameRate   = anim->frameRate;
				outAnim.frameCount  = anim->frameCount;
				outAnim.worldWidth  = fixed16ToFloat(anim->worldWidth);
				outAnim.worldHeight = fixed16ToFloat(anim->worldHeight);
				for (s32 v = 0; v < WAX_MAX_VIEWS; v++)
				{
					WaxView* view = WAX_ViewPtr(wax, anim, v);
					memset(outAnim.views[v].frameIndex, 0, sizeof(s32) * 32);
					if (!view) { continue; }

					for (s32 f = 0; f < anim->frameCount; f++)
					{
						outAnim.views[v].frameIndex[f] = (s32)outSprite->frame.size();

						WaxFrame* frame = WAX_FramePtr(wax, view, f);
						WaxCell* cell = WAX_CellPtr(wax, frame);

						s32 id = -1;
						if (!findWaxCellInMap(cell, id))
						{
							id = (s32)s_waxDataList.size();
							insertWaxCellIntoMap(cell, id);
						}

						SpriteFrame outFrame;
						outFrame.cellIndex = id;
						outFrame.flip = frame->flip;
						outFrame.offsetX  = frame->offsetX;
						outFrame.offsetY  = frame->offsetY;
						outFrame.widthWS  = fixed16ToFloat(frame->widthWS);
						outFrame.heightWS = fixed16ToFloat(frame->heightWS);
						outSprite->frame.push_back(outFrame);
					}
				}
				outSprite->anim.push_back(outAnim);
			}

			// Now pack the cells into the texture...
			const size_t cellCount = s_waxDataList.size();
			WaxCell** waxCell = s_waxDataList.data();
			s32 totalWidth = 0, maxHeight = 0;
			for (size_t c = 0; c < cellCount; c++)
			{
				totalWidth += waxCell[c]->sizeX + 2;
				maxHeight = max(maxHeight, waxCell[c]->sizeY + 2);
			}

			// Super basic packing.
			s32 rows = (totalWidth + 4095) / 4096;
			s32 texW = totalWidth <= 4096 ? totalWidth : 4096;
			s32 texH = rows * maxHeight;

			// make sure the texture size is divisible by 4.
			texW = ((texW + 3) >> 2) << 2;
			texH = ((texH + 3) >> 2) << 2;

			WorkBuffer& buffer = getWorkBuffer();
			buffer.resize(texW * texH * 4);
			u32* imageBuffer = (u32*)buffer.data();
			memset(imageBuffer, 0, texW * texH * 4);

			s32 u = 0, v = 0;
			for (size_t c = 0; c < cellCount; c++)
			{
				if (u + waxCell[c]->sizeX >= 4096)
				{
					u = 0;
					v += maxHeight;
				}

				SpriteCell outCell;
				outCell.u = u;
				outCell.v = v;
				outCell.w = waxCell[c]->sizeX;
				outCell.h = waxCell[c]->sizeY;
				outSprite->cell.push_back(outCell);

				writeCellToImage(u, v, waxCell[c]->sizeX, waxCell[c]->sizeY, texW, wax, waxCell[c], palette, imageBuffer);
				u += waxCell[c]->sizeX + 2;
			}

			outSprite->texGpu = TFE_RenderBackend::createTexture(texW, texH, imageBuffer);
			outSprite->paletteIndex = palIndex;
			outSprite->lightLevel = 32;
			strcpy(outSprite->name, filename);
					
			free(wax);
			*sprite = *outSprite;
			return true;
		}

		return false;
	}

	bool loadEditorSpriteLit(SpriteSourceType type, Archive* archive, const char* filename, const u32* palette, s32 palIndex, const u8* colormap, s32 lightLevel, EditorSprite* sprite)
	{
		s_remapTable = &colormap[lightLevel * 256];
		bool result = loadEditorSprite(type, archive, filename, palette, palIndex, sprite);
		if (result)
		{
			sprite->lightLevel = lightLevel;
		}
		s_remapTable = s_identityTable;

		return result;
	}
}