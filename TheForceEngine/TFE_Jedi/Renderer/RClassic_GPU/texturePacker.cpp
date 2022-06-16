#include <cstring>

#include <TFE_System/profiler.h>
#include <TFE_System/math.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/shaderBuffer.h>

#include <TFE_Asset/imageAsset.h>

#include "texturePacker.h"
#include "../rcommon.h"

#include <map>

#define DEBUG_TEXTURE_ATLAS 0

#if DEBUG_TEXTURE_ATLAS
namespace TFE_DarkForces
{
	extern u8 s_levelPalette[];
}
#endif

namespace TFE_Jedi
{
	// TODO: Sort textures from largest to smallest before inserting.

	struct Node
	{
		Node*  child[2] = { 0 };	// Binary tree.
		Vec4ui rect = { 0 };
		void* tex = nullptr;
	};

	enum
	{
		TIGHT_FIT_PIXELS = 2,
		MAX_TEXTURE_COUNT = 2048,
	};

	static std::vector<Node*> s_nodes;
	static Node* s_root;
	static TexturePacker* s_texturePacker;
	static std::map<TextureData*, s32> s_textureDataMap;
	static std::map<WaxCell*, s32> s_waxDataMap;
	static std::vector<TextureInfo> s_texInfoPool;

	static s32 s_usedTexels = 0;
	static s32 s_totalTexels = 0;

#if DEBUG_TEXTURE_ATLAS
	void debug_writeOutAtlas();
#endif

	// Initialize the texture packer once, it is persistent across levels.
	TexturePacker* texturepacker_init(const char* name, s32 width, s32 height)
	{
		TexturePacker* texturePacker = (TexturePacker*)malloc(sizeof(TexturePacker));
		if (!texturePacker) { return nullptr; }

		texturePacker->backingMemory = (u8*)malloc(width * height);
		if (!texturePacker->backingMemory)
		{
			texturepacker_destroy(texturePacker);
			return nullptr;
		}
		memset(texturePacker->backingMemory, 0, width * height);
		
		texturePacker->textureTable = (Vec4i*)malloc(sizeof(Vec4i) * MAX_TEXTURE_COUNT);
		if (!texturePacker->textureTable)
		{
			texturepacker_destroy(texturePacker);
			return nullptr;
		}

		texturePacker->width = width;
		texturePacker->height = height;
		texturePacker->texture = TFE_RenderBackend::createTexture(width, height, 1);
		if (!texturePacker->texture)
		{
			texturepacker_destroy(texturePacker);
			return nullptr;
		}

		ShaderBufferDef textureTableDef =
		{
			4,				// 1, 2, 4 channels (R, RG, RGBA)
			sizeof(s32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
			BUF_CHANNEL_INT
		};
		texturePacker->textureTableGPU.create(MAX_TEXTURE_COUNT, textureTableDef, true, nullptr);

		strncpy(texturePacker->name, name, 64);
		return texturePacker;
	}

	// Free memory and GPU buffers. Note: GPU textures need to be persistent, so the level allocator will not be used.
	void texturepacker_destroy(TexturePacker* texturePacker)
	{
		TFE_RenderBackend::freeTexture(texturePacker->texture);
		texturePacker->textureTableGPU.destroy();
		free(texturePacker->backingMemory);
		free(texturePacker->textureTable);
		free(texturePacker);
	}
		
	bool textureFitsInNode(Node* cur, u32 width, u32 height)
	{
		return width <= cur->rect.z && height <= cur->rect.w;
	}

	bool textureTightFit(Node* cur, u32 width, u32 height)
	{
		return (cur->rect.z - width <= TIGHT_FIT_PIXELS) && (cur->rect.w - height <= TIGHT_FIT_PIXELS);
	}

	Node* insertNode(Node* cur, void* tex, u32 width, u32 height)
	{
		Node* newNode = nullptr;
		if (!s_root)
		{
			newNode = new Node();
			newNode->child[0] = nullptr;
			newNode->child[1] = nullptr;
			newNode->rect = {0u, 0u, (u32)s_texturePacker->width, (u32)s_texturePacker->height};
			newNode->tex  = tex;
			s_root = newNode;

			s_nodes.push_back(newNode);
			return newNode;
		}

		// We are not a leaf...
		if (cur->child[0])
		{
			// Try inserting left.
			newNode = insertNode(cur->child[0], tex, width, height);
			if (newNode) { return newNode; }

			// Then right.
			return insertNode(cur->child[1], tex, width, height);
		}
		else // Leaf node.
		{
			// If the node is already full...
			if (cur->tex) { return nullptr; }

			// Check to see if the texture fits.
			if (!textureFitsInNode(cur, width, height)) { return nullptr; }

			// Check to see if it is a tight fit.
			if (textureTightFit(cur, width, height))
			{
				cur->tex = tex;
				return cur;
			}

			// Split the node.
			cur->child[0] = new Node();
			cur->child[1] = new Node();

			// Push nodes so they can be deleted later.
			s_nodes.push_back(cur->child[0]);
			s_nodes.push_back(cur->child[1]);
			
			// Binary split along the longest axis.
			const s32 dw = cur->rect.z - width;
			const s32 dh = cur->rect.w - height;
			if (dw > dh)
			{
				cur->child[0]->rect = { cur->rect.x, cur->rect.y, width, cur->rect.w };
				cur->child[1]->rect = { cur->rect.x + width, cur->rect.y, cur->rect.z - width, cur->rect.w };
			}
			else
			{
				cur->child[0]->rect = { cur->rect.x, cur->rect.y, cur->rect.z, height };
				cur->child[1]->rect = { cur->rect.x, cur->rect.y + height, cur->rect.z, cur->rect.w - height };
			}

			// Then insert into the first child.
			return insertNode(cur->child[0], tex, width, height);
		}
	}
		
	void packNode(const Node* node, const TextureData* texData, Vec4i* tableEntry)
	{
		// Copy the texture into place.
		const u8* srcImage = texData->image;
		u8* output = &s_texturePacker->backingMemory[node->rect.y * s_texturePacker->width + node->rect.x];
		for (s32 y = 0; y < texData->height; y++, output += s_texturePacker->width)
		{
			for (s32 x = 0; x < texData->width; x++)
			{
				output[x] = srcImage[x*texData->height + y];
			}
		}
		s_usedTexels += texData->width * texData->height;

		// Copy the mapping into the texture table.
		tableEntry->x = (s32)node->rect.x;
		tableEntry->y = (s32)node->rect.y;
		tableEntry->z = (s32)node->rect.z;
		tableEntry->w = (s32)node->rect.w;
	}

	void packNodeCell(const Node* node, const void* basePtr, const WaxCell* cell, Vec4i* tableEntry)
	{
		// Copy the texture into place.
		const s32 compressed = cell->compressed;
		u8* imageData = (u8*)cell + sizeof(WaxCell);
		u8* image = (compressed == 1) ? imageData + (cell->sizeX * sizeof(u32)) : imageData;
		u8* output = &s_texturePacker->backingMemory[node->rect.y*s_texturePacker->width + node->rect.x];

		u8 columnWorkBuffer[1024];
		const u32* columnOffset = (u32*)((u8*)basePtr + cell->columnOffset);
		for (s32 x = 0; x < cell->sizeX; x++)
		{
			u8* column = (u8*)image + columnOffset[x];
			if (compressed)
			{
				const u8* colPtr = (u8*)cell + columnOffset[x];
				sprite_decompressColumn(colPtr, columnWorkBuffer, cell->sizeY);
				column = columnWorkBuffer;
			}

			for (s32 y = 0; y < cell->sizeY; y++)
			{
				output[y*s_texturePacker->width + x] = column[y];
			}
		}
		s_usedTexels += cell->sizeX * cell->sizeY;

		// Copy the mapping into the texture table.
		tableEntry->x = (s32)node->rect.x;
		tableEntry->y = (s32)node->rect.y;
		tableEntry->z = (s32)node->rect.z;
		tableEntry->w = (s32)node->rect.w;
	}

	bool isTextureInMap(TextureData* tex)
	{
		return (s_textureDataMap.find(tex) != s_textureDataMap.end());
	}

	void insertTextureIntoMap(TextureData* tex, s32 id)
	{
		s_textureDataMap[tex] = id;
	}

	bool isWaxCellInMap(WaxCell* cell)
	{
		return (s_waxDataMap.find(cell) != s_waxDataMap.end());
	}

	void insertWaxCellIntoMap(WaxCell* cell, s32 id)
	{
		s_waxDataMap[cell] = id;
	}

	void insertTexture(TextureData* tex)
	{
		if (!tex || isTextureInMap(tex)) { return; }
		insertTextureIntoMap(tex, s_texturePacker->texturesPacked);
		s_totalTexels += tex->width * tex->height;

		Node* node = insertNode(s_root, tex, tex->width, tex->height);
		assert(!node || node->tex == tex);
		if (node)
		{
			assert(s_texturePacker->texturesPacked < MAX_TEXTURE_COUNT);
			tex->textureId = s_texturePacker->texturesPacked;
			packNode(node, tex, &s_texturePacker->textureTable[s_texturePacker->texturesPacked]);
			s_texturePacker->texturesPacked++;
		}
		else
		{
			tex->textureId = 0;
		}
	}

	void insertWaxFrame(void* basePtr, WaxFrame* frame)
	{
		if (!basePtr || !frame) { return; }
				
		WaxCell* cell = WAX_CellPtr(basePtr, frame);
		if (!cell || isWaxCellInMap(cell)) { return; }
		insertWaxCellIntoMap(cell, s_texturePacker->texturesPacked);
		s_totalTexels += cell->sizeX * cell->sizeY;

		Node* node = insertNode(s_root, cell, cell->sizeX, cell->sizeY);
		assert(node && node->tex == cell);
		if (node)
		{
			assert(s_texturePacker->texturesPacked < MAX_TEXTURE_COUNT);
			cell->textureId = s_texturePacker->texturesPacked;
			packNodeCell(node, basePtr, cell, &s_texturePacker->textureTable[s_texturePacker->texturesPacked]);
			s_texturePacker->texturesPacked++;
		}
	}

	void insertAnimatedTextureFrames(AnimatedTexture* animTex)
	{
		if (!animTex) { return; }
		for (s32 f = 0; f < animTex->count; f++)
		{
			insertTexture(animTex->frameList[f]);
		}
	}
		
	s32 textureSort(const void* a, const void* b)
	{
		const TextureInfo* texA = (TextureInfo*)a;
		const TextureInfo* texB = (TextureInfo*)b;
		if (texA->sortKey > texB->sortKey) { return -1; }
		if (texA->sortKey < texB->sortKey) { return  1; }
		return 0;
	}

	s32 texturepacker_pack(TexturePacker* texturePacker, TextureListCallback getList)
	{
		if (!texturePacker || !getList) { return 0; }

		// Pre-allocate the texture pool based on the maximum texture count.
		if (s_texInfoPool.capacity() == 0)
		{
			s_texInfoPool.reserve(MAX_TEXTURE_COUNT);
		}

		s_nodes.clear();
		s_root = nullptr;
		// Set the current packer.
		// Note: this will note work correctly as-is if multithreaded.
		s_texturePacker = texturePacker;
		s_texturePacker->texturesPacked = 0;
		s_textureDataMap.clear();
		s_waxDataMap.clear();

		// Insert the parent that covers all of the available space.
		insertNode(s_root, nullptr, 0, 0);

		// Get textures.
		s_texInfoPool.clear();
		if (getList(s_texInfoPool))
		{
			const s32 count = (s32)s_texInfoPool.size();
			TextureInfo* list = s_texInfoPool.data();
			// 1. Calculate the perimeter.
			for (s32 i = 0; i < count; i++)
			{
				switch (list[i].type)
				{
					case TEXINFO_DF_TEXTURE_DATA:
					{
						if (list[i].texData->uvWidth == BM_ANIMATED_TEXTURE)
						{
							AnimatedTexture* animTex = (AnimatedTexture*)list[i].texData->image;
							list[i].sortKey = animTex->frameList[0]->width + animTex->frameList[0]->height;
						}
						else
						{
							list[i].sortKey = list[i].texData->width * list[i].texData->height;
						}
					} break;
					case TEXINFO_DF_ANIM_TEX:
					{
						list[i].sortKey = list[i].animTex->frameList[0]->width * list[i].animTex->frameList[0]->height;
					} break;
					case TEXINFO_DF_WAX_CELL:
					{
						WaxCell* cell = WAX_CellPtr(list[i].basePtr, list[i].frame);
						list[i].sortKey = cell->sizeX * cell->sizeY;
					} break;
				}
			}

			// 2. Sort textures by perimeter from largest to smallest - simplified to w+h
			std::qsort(list, size_t(count), sizeof(TextureInfo), textureSort);

			// 3. Insert each texture into the tree.
			s_usedTexels = 0;
			s_totalTexels = 0;
			for (s32 i = 0; i < count; i++)
			{
				switch (list[i].type)
				{
					case TEXINFO_DF_TEXTURE_DATA:
					{
						if (list[i].texData->uvWidth == BM_ANIMATED_TEXTURE)
						{
							AnimatedTexture* animTex = (AnimatedTexture*)list[i].texData->image;
							insertAnimatedTextureFrames(animTex);
						}
						else
						{
							insertTexture(list[i].texData);
						}
					} break;
					case TEXINFO_DF_ANIM_TEX:
					{
						insertAnimatedTextureFrames(list[i].animTex);
					} break;
					case TEXINFO_DF_WAX_CELL:
					{
						insertWaxFrame(list[i].basePtr, list[i].frame);
					} break;
				}
			}

			#if DEBUG_TEXTURE_ATLAS
				debug_writeOutAtlas();
			#endif

			f64 utilization = 100.0 * f64(s_usedTexels) / f64(4096 * 4096);
			if (s_totalTexels > s_usedTexels)
			{
				// Cannot fit everything into the atlas.
				TFE_System::logWrite(LOG_WARNING, "TexturePacker", "Cannot pack all textures for '%s' into an atlas. Utilization: %f%%; Remaining texels: %d",
					s_texturePacker->name, utilization, s_totalTexels - s_usedTexels);
			}

			// Pack the GPU data.
			s_texturePacker->texture->update(s_texturePacker->backingMemory, s_texturePacker->width * s_texturePacker->height);
			s_texturePacker->textureTableGPU.update(s_texturePacker->textureTable, sizeof(Vec4i) * s_texturePacker->texturesPacked);
		}

		// Free nodes.
		const size_t count = s_nodes.size();
		Node** nodes = s_nodes.data();
		for (size_t i = 0; i < count; i++)
		{
			delete nodes[i];
		}
		s_nodes.clear();

		return s_texturePacker->texturesPacked;
	}

#if DEBUG_TEXTURE_ATLAS
	u32 s_debugPal[256];

#define CONV_6bitTo8bit_DBG(x) (((x)<<2) | ((x)>>4))
	void setupDebugPal(u8* pal)
	{
		// Update the palette.
		u32* outColor = s_debugPal;
		u8* srcColor = pal;
		for (s32 i = 0; i < 256; i++, outColor++, srcColor += 3)
		{
			*outColor = CONV_6bitTo8bit_DBG(srcColor[0]) | (CONV_6bitTo8bit_DBG(srcColor[1]) << 8u) | (CONV_6bitTo8bit_DBG(srcColor[2]) << 16u) | (0xffu << 24u);
		}
	}

	void debug_writeOutAtlas()
	{
		setupDebugPal(TFE_DarkForces::s_levelPalette);
		u32* image = (u32*)malloc(s_texturePacker->width * s_texturePacker->height * sizeof(u32));

		const u32 count = s_texturePacker->width * s_texturePacker->height;
		for (u32 i = 0; i < count; i++)
		{
			image[i] = s_debugPal[s_texturePacker->backingMemory[i]];
		}

		char atlasName[TFE_MAX_PATH];
		sprintf(atlasName, "Atlas_%s.png", s_texturePacker->name);
		TFE_Image::writeImage(atlasName, s_texturePacker->width, s_texturePacker->height, image);
		free(image);
	}
#endif
}