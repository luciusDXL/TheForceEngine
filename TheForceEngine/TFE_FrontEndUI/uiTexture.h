#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/renderBackend.h>

struct UiTexture
{
	TextureGpu* texture = nullptr;
	u32  width = 0;
	u32  height = 0;
	f32  rect[4];
	Vec2f scale = { 1.0f, 1.0f };
};
struct TextureData;

namespace TFE_FrontEndUI
{
	bool convertPalette(const u8* srcPalette, u32* dstPalette);
	void convertDfTextureToTrueColor(const TextureData* src, const u32* palette, u32* image);
	bool createTexture(const TextureData* src, const u32* palette, UiTexture* tex, MagFilter filter = MAG_FILTER_NONE);
}