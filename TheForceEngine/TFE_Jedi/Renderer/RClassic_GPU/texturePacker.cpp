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
		s_textureTableGPU.create(s_sectorCount, textureTableDef, true, nullptr);

		return s_texture!=nullptr;
	}

	// Free memory and GPU buffers. Note: GPU textures need to be persistent, so the level allocator will not be used.
	void texturepacker_destroy()
	{
		TFE_RenderBackend::freeTexture(s_texture);
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
			
			// Binary split.
			s32 dw = cur->rect.z - tex->width;
			s32 dh = cur->rect.w - tex->height;

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

	// Returns the number of textures and fills in a shader buffer with the offsets and sizes.
	// Calling this will clear the existing atlas.
	s32 texturepacker_packLevelTextures()
	{
		s32 textureCount = 0;
		TextureData** textures = level_getTextures(&textureCount);
		if (!textureCount || !textures) { return 0; }

		s_nodes.clear();
		s_root = nullptr;

		const f32 uScale = 1.0f / f32(s_width);
		const f32 vScale = 1.0f / f32(s_height);

		// Insert the parent.
		insertNode(s_root, nullptr);
		// Insert textures.
		s32 texturesPacked = 0;
		s32 animTextureCount = 0;
		for (s32 i = 0; i < textureCount; i++)
		{
			// Animated texture
			if (textures[i]->uvWidth == -2)
			{
				// Skip for now.

				// Copy an empty rect.
				s_textureTable[i].x = 0;
				s_textureTable[i].y = 0;
				s_textureTable[i].z = 0;
				s_textureTable[i].w = 0;

				animTextureCount++;
				continue;
			}

			Node* node = insertNode(s_root, textures[i]);
			assert(node);

			if (node)
			{
				texturesPacked++;
				
				// Copy the texture into place.
				const u8* srcImage = textures[i]->image;
				u8* output = &s_backingMemory[node->rect.y * s_width + node->rect.x];
				for (s32 y = 0; y < textures[i]->height; y++, output += s_width)
				{
					for (s32 x = 0; x < textures[i]->width; x++)
					{
						output[x] = srcImage[x*textures[i]->height + y];
					}
				}

				// Copy the mapping into the texture table.
				s_textureTable[i].x = (s32)node->rect.x;
				s_textureTable[i].y = (s32)node->rect.y;
				s_textureTable[i].z = (s32)node->rect.z;
				s_textureTable[i].w = (s32)node->rect.w;
			}
		}
	#if DEBUG_TEXTURE_ATLAS
		debug_writeOutAtlas();
	#endif

		s_texture->update(s_backingMemory, s_width * s_height);
		s_textureTableGPU.update(s_textureTable, sizeof(Vec4i) * textureCount);

		// Free nodes.
		const size_t count = s_nodes.size();
		Node** nodes = s_nodes.data();
		for (size_t i = 0; i < count; i++)
		{
			delete nodes[i];
		}
		s_nodes.clear();
				
		return texturesPacked;
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