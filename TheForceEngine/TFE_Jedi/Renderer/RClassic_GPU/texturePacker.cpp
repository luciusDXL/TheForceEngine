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
		TextureData* tex = nullptr;
	};

	enum
	{
		TIGHT_FIT_PIXELS = 2,
	};

	static std::vector<Node*> s_nodes;
	static Node* s_root;
	static ShaderBuffer s_textureTableGPU;
	static Vec4i* s_textureTable;
		
	static u8* s_backingMemory = nullptr;
	static TextureGpu* s_texture = nullptr;
	static s32 s_width  = 0;
	static s32 s_height = 0;
	static s32 s_texturesPacked = 0;

	static std::map<TextureData*, s32> s_textureDataMap;

#if DEBUG_TEXTURE_ATLAS
	void debug_writeOutAtlas();
#endif

	// Initialize the texture packer once, it is persistent across levels.
	bool texturepacker_init(s32 width, s32 height)
	{
		s_backingMemory = (u8*)malloc(width * height);
		memset(s_backingMemory, 0, width * height);
		if (!s_backingMemory) { return false; }

		s_textureTable = (Vec4i*)malloc(sizeof(Vec4i) * 1024);

		s_width = width;
		s_height = height;
		s_texture = TFE_RenderBackend::createTexture(width, height, 1);

		ShaderBufferDef textureTableDef =
		{
			4,				// 1, 2, 4 channels (R, RG, RGBA)
			sizeof(s32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
			BUF_CHANNEL_INT
		};
		s_textureTableGPU.create(1024, textureTableDef, true, nullptr);

		return s_texture!=nullptr;
	}

	// Free memory and GPU buffers. Note: GPU textures need to be persistent, so the level allocator will not be used.
	void texturepacker_destroy()
	{
		TFE_RenderBackend::freeTexture(s_texture);
		s_textureTableGPU.destroy();
		free(s_backingMemory);
		free(s_textureTable);

		s_texture = nullptr;
		s_backingMemory = nullptr;
	}
		
	bool textureFitsInNode(Node* cur, TextureData* tex)
	{
		return tex->width <= cur->rect.z && tex->height <= cur->rect.w;
	}

	bool textureTightFit(Node* cur, TextureData* tex)
	{
		return (cur->rect.z - tex->width <= TIGHT_FIT_PIXELS) && (cur->rect.w - tex->height <= TIGHT_FIT_PIXELS);
	}

	Node* insertNode(Node* cur, TextureData* tex)
	{
		Node* newNode = nullptr;
		if (!s_root)
		{
			newNode = new Node();
			newNode->child[0] = nullptr;
			newNode->child[1] = nullptr;
			newNode->rect = {0u, 0u, (u32)s_width, (u32)s_height};
			newNode->tex  = tex;
			s_root = newNode;

			s_nodes.push_back(newNode);
			return newNode;
		}

		// We are not a leaf...
		if (cur->child[0])
		{
			// Try inserting left.
			newNode = insertNode(cur->child[0], tex);
			if (newNode) { return newNode; }

			// Then right.
			return insertNode(cur->child[1], tex);
		}
		else // Leaf node.
		{
			// If the node is already full...
			if (cur->tex) { return nullptr; }

			// Check to see if the texture fits.
			if (!textureFitsInNode(cur, tex)) { return nullptr; }

			// Check to see if it is a tight fit.
			if (textureTightFit(cur, tex))
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
			const s32 dw = cur->rect.z - tex->width;
			const s32 dh = cur->rect.w - tex->height;
			if (dw > dh)
			{
				cur->child[0]->rect = { cur->rect.x, cur->rect.y, tex->width, cur->rect.w };
				cur->child[1]->rect = { cur->rect.x + tex->width, cur->rect.y, cur->rect.z - tex->width, cur->rect.w };
			}
			else
			{
				cur->child[0]->rect = { cur->rect.x, cur->rect.y, cur->rect.z, tex->height };
				cur->child[1]->rect = { cur->rect.x, cur->rect.y + tex->height, cur->rect.z, cur->rect.w - tex->height };
			}

			// Then insert into the first child.
			return insertNode(cur->child[0], tex);
		}
	}

	void packNode(const Node* node, const TextureData* texData, Vec4i* tableEntry)
	{
		// Copy the texture into place.
		const u8* srcImage = texData->image;
		u8* output = &s_backingMemory[node->rect.y * s_width + node->rect.x];
		for (s32 y = 0; y < texData->height; y++, output += s_width)
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

	bool isTextureInMap(TextureData* tex)
	{
		return (s_textureDataMap.find(tex) != s_textureDataMap.end());
	}

	void insertTextureIntoMap(TextureData* tex, s32 id)
	{
		s_textureDataMap[tex] = id;
	}

	void insertTexture(TextureData* tex)
	{
		if (!tex || isTextureInMap(tex)) { return; }
		insertTextureIntoMap(tex, s_texturesPacked);

		Node* node = insertNode(s_root, tex);
		assert(node && node->tex == tex);
		if (node)
		{
			tex->textureId = s_texturesPacked;
			packNode(node, tex, &s_textureTable[s_texturesPacked]);
			s_texturesPacked++;
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
	s32 texturepacker_packLevelTextures()
	{
		s_nodes.clear();
		s_root = nullptr;

		// Insert the parent that covers all of the available space.
		insertNode(s_root, nullptr);

		// Insert normal textures and signs.
		s_texturesPacked = 0;
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

		s_texture->update(s_backingMemory, s_width * s_height);
		s_textureTableGPU.update(s_textureTable, sizeof(Vec4i) * s_texturesPacked);

		// Free nodes.
		const size_t count = s_nodes.size();
		Node** nodes = s_nodes.data();
		for (size_t i = 0; i < count; i++)
		{
			delete nodes[i];
		}
		s_nodes.clear();
				
		return s_texturesPacked;
	}

	TextureGpu* texturepacker_getTexture()
	{
		return s_texture;
	}

	ShaderBuffer* texturepacker_getTable()
	{
		return &s_textureTableGPU;
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
		u32* image = (u32*)malloc(s_width * s_height * sizeof(u32));

		const u32 count = s_width * s_height;
		for (u32 i = 0; i < count; i++)
		{
			image[i] = s_debugPal[s_backingMemory[i]];
		}

		TFE_Image::writeImage("Atlas.png", s_width, s_height, image);
		free(image);
	}
#endif
}