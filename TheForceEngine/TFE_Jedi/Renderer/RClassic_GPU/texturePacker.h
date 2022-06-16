#pragma once
//////////////////////////////////////////////////////////////////////
// Pack level textures into an atlas texture or array of textures.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <TFE_RenderBackend/shaderBuffer.h>

struct TextureData;
struct AnimatedTexture;
struct WaxFrame;

namespace TFE_Jedi
{
	enum TextureInfoType
	{
		TEXINFO_DF_TEXTURE_DATA = 0,
		TEXINFO_DF_ANIM_TEX,
		TEXINFO_DF_WAX_CELL,
		TEXINFO_COUNT
	};

	struct TextureInfo
	{
		TextureInfoType type;
		union
		{
			TextureData* texData;
			AnimatedTexture* animTex;
			WaxFrame* frame;
		};
		void* basePtr = nullptr;	// used for WAX/Frame.
		s32 sortKey = 0;			// Calculated by Texture Packer.
	};
	typedef std::vector<TextureInfo> TextureInfoList;
	typedef bool(*TextureListCallback)(TextureInfoList& texList);

	struct TexturePacker
	{
		// GPU resources
		ShaderBuffer textureTableGPU;
		TextureGpu* texture = nullptr;

		// CPU memory, this is kept around so it can be used on multiple levels.
		Vec4i* textureTable = nullptr;
		u8* backingMemory = nullptr;

		// General Data
		s32 width = 0;
		s32 height = 0;
		s32 texturesPacked = 0;

		// For debugging.
		char name[64];
	};

	// Initialize the texture packer once, it is persistent across levels.
	TexturePacker* texturepacker_init(const char* name, s32 width, s32 height);
	// Free memory and GPU buffers. Note: GPU textures need to be persistent, so the level allocator will not be used.
	void texturepacker_destroy(TexturePacker* texturePacker);

	// Pack textures of various types into a single texture atlas.
	// The client must provide a 'getList' function to get a list of 'TextureInfo' (see above).
	s32 texturepacker_pack(TexturePacker* texturePacker, TextureListCallback getList);
}  // TFE_Jedi