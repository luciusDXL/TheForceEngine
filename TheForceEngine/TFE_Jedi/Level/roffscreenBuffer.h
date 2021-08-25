#pragma once
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Memory/allocator.h>
#include <TFE_FileSystem/paths.h>
#include "rtexture.h"

struct OffScreenBuffer
{
	u8* image;
	s32 width;
	s32 height;
	s32 size;
	u32 flags;
};

enum OffScreenBufferFlags
{
	OBF_NONE = 0,
	OBF_TRANS = FLAG_BIT(0),	// transparent element.
};

namespace TFE_Jedi
{
	OffScreenBuffer* createOffScreenBuffer(s32 width, s32 height, u32 flags);
	void offscreenBuffer_clearImage(OffScreenBuffer* buffer, u8 clear_color);

	// Draw texture 'tex' to the offscreen buffer 'buffer' at position (x, y).
	void offscreenBuffer_drawTexture(OffScreenBuffer* buffer, TextureData* tex, s32 x, s32 y);
}  // TFE_Jedi