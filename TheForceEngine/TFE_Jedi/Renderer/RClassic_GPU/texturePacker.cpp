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
	struct Node
	{
		Node*  child[2] = { 0 };	// Binary tree.
		Vec4ui rect = { 0 };
		void* tex = nullptr;
	};

	enum
	{
		TIGHT_FIT_PIXELS = 2,
	};

	static std::vector<Node*> s_nodes;
	static Node* s_root;
	static TexturePacker* s_texturePacker;
	static std::map<TextureData*, s32> s_textureDataMap;
	static std::map<WaxCell*, s32> s_waxDataMap;

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
		
		texturePacker->textureTable = (Vec4i*)malloc(sizeof(Vec4i) * 1024);
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
		texturePacker->textureTableGPU.create(1024, textureTableDef, true, nullptr);

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

		Node* node = insertNode(s_root, tex, tex->width, tex->height);
		assert(node && node->tex == tex);
		if (node)
		{
			tex->textureId = s_texturePacker->texturesPacked;
			packNode(node, tex, &s_texturePacker->textureTable[s_texturePacker->texturesPacked]);
			s_texturePacker->texturesPacked++;
		}
	}

	void insertWaxFrame(void* basePtr, WaxFrame* frame)
	{
		if (!basePtr || !frame) { return; }

		WaxCell* cell = WAX_CellPtr(basePtr, frame);
		if (!cell || isWaxCellInMap(cell)) { return; }
		insertWaxCellIntoMap(cell, s_texturePacker->texturesPacked);

		Node* node = insertNode(s_root, cell, cell->sizeX, cell->sizeY);
		assert(node && node->tex == cell);
		if (node)
		{
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
		
	// Returns the number of textures and fills in a shader buffer with the offsets and sizes.
	// Calling this will clear the existing atlas.
	s32 texturepacker_packLevelTextures(TexturePacker* texturePacker)
	{
		s_nodes.clear();
		s_root = nullptr;
		// Set the current packer.
		// Note: this will note work correctly as-is if multithreaded.
		s_texturePacker = texturePacker;
		s_textureDataMap.clear();

		// Insert the parent that covers all of the available space.
		insertNode(s_root, nullptr, 0, 0);

		// Insert normal textures and signs.
		s_texturePacker->texturesPacked = 0;
		s32 textureCount = 0;
		TextureData** textures = level_getTextures(&textureCount);
		for (s32 i = 0; i < textureCount; i++)
		{
			// Animated texture
			if (textures[i]->uvWidth == BM_ANIMATED_TEXTURE)
			{
				AnimatedTexture* animTex = (AnimatedTexture*)textures[i]->image;
				insertAnimatedTextureFrames(animTex);
			}
			else
			{
				insertTexture(textures[i]);
			}
		}
	
		// Insert animated textures.
		Allocator* animTextures = bitmap_getAnimatedTextures();
		AnimatedTexture* animTex = (AnimatedTexture*)allocator_getHead(animTextures);
		while (animTex)
		{
			insertAnimatedTextureFrames(animTex);
			animTex = (AnimatedTexture*)allocator_getNext(animTextures);
		}

	#if DEBUG_TEXTURE_ATLAS
		debug_writeOutAtlas();
	#endif

		s_texturePacker->texture->update(s_texturePacker->backingMemory, s_texturePacker->width * s_texturePacker->height);
		s_texturePacker->textureTableGPU.update(s_texturePacker->textureTable, sizeof(Vec4i) * s_texturePacker->texturesPacked);

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

	// Returns the number of textures and fills in a shader buffer with the offsets and sizes.
	// Calling this will clear the existing atlas.
	s32 texturepacker_packObjectTextures(TexturePacker* texturePacker)
	{
		s_nodes.clear();
		s_root = nullptr;
		// Set the current packer.
		// Note: this will note work correctly as-is if multithreaded.
		s_texturePacker = texturePacker;
		s_waxDataMap.clear();

		// Insert the parent that covers all of the available space.
		insertNode(s_root, nullptr, 0, 0);

		// Insert sprite frames.
		s_texturePacker->texturesPacked = 0;
		std::vector<JediWax*> waxList;
		TFE_Sprite_Jedi::getWaxList(waxList);
		const size_t waxCount = waxList.size();
		JediWax** wax = waxList.data();
		for (size_t i = 0; i < waxCount; i++)
		{
			for (s32 animId = 0; animId < wax[i]->animCount; animId++)
			{
				WaxAnim* anim = WAX_AnimPtr(wax[i], animId);
				if (!anim) { continue; }
				for (s32 v = 0; v < 32; v++)
				{
					WaxView* view = WAX_ViewPtr(wax[i], anim, v);
					if (!view) { continue; }
					for (s32 f = 0; f < anim->frameCount; f++)
					{
						WaxFrame* frame = WAX_FramePtr(wax[i], view, f);
						insertWaxFrame(wax[i], frame);
					}
				}
			}
		}
		
		// Insert frames.
		std::vector<JediFrame*> frameList;
		TFE_Sprite_Jedi::getFrameList(frameList);
		const size_t frameCount = frameList.size();
		JediFrame** frame = frameList.data();
		for (size_t i = 0; i < frameCount; i++)
		{
			insertWaxFrame(frame[i], frame[i]);
		}

#if DEBUG_TEXTURE_ATLAS
		debug_writeOutAtlas();
#endif

		s_texturePacker->texture->update(s_texturePacker->backingMemory, s_texturePacker->width * s_texturePacker->height);
		s_texturePacker->textureTableGPU.update(s_texturePacker->textureTable, sizeof(Vec4i) * s_texturePacker->texturesPacked);

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