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

namespace TFE_Jedi
{
	// Initialize the texture packer once, it is persistent across levels.
	bool texturepacker_init(s32 width, s32 height);
	// Free memory and GPU buffers. Note: GPU textures need to be persistent, so the level allocator will not be used.
	void texturepacker_destroy();

	// Returns the number of textures and fills in a shader buffer with the offsets and sizes.
	// Calling this will clear the existing atlas.
	s32 texturepacker_packLevelTextures();

	// Get the texture and table for use in shaders.
	TextureGpu*   texturepacker_getTexture();
	ShaderBuffer* texturepacker_getTable();
}  // TFE_Jedi
