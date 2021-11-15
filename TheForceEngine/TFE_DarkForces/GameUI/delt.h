#pragma once
//////////////////////////////////////////////////////////////////////
// Note this is still the original TFE code, reverse-engineered game
// UI code will not be available until future releases.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Level/rtexture.h>

namespace TFE_DarkForces
{
	struct DeltFrame
	{
		TextureData texture;
		s16 offsetX;
		s16 offsetY;
	};

	u8* getTempBuffer(size_t size);
	JBool loadPaletteFromPltt(const char* name, u8* palette);
	u32 getFramesFromAnim(const char* name, DeltFrame** outFrames);
	JBool getFrameFromDelt(const char* name, DeltFrame* outFrame);

	void blitDeltaFrame(DeltFrame* frame, s32 x, s32 y, u8* framebuffer);
	void blitDeltaFrameScaled(DeltFrame* frame, s32 x0, s32 y0, fixed16_16 xScale, fixed16_16 yScale, u8* framebuffer);
}
