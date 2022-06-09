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

// TODO: Figure out incorrect texture in Ramsees Hed.

namespace TFE_Jedi
{
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
	};

	// Initialize the texture packer once, it is persistent across levels.
	TexturePacker* texturepacker_init(s32 width, s32 height);
	// Free memory and GPU buffers. Note: GPU textures need to be persistent, so the level allocator will not be used.
	void texturepacker_destroy(TexturePacker* texturePacker);

	// Returns the number of textures and fills in a shader buffer with the offsets and sizes.
	// Calling this will clear the existing atlas.
	s32 texturepacker_packLevelTextures(TexturePacker* texturePacker);
}  // TFE_Jedi
