#pragma once
//////////////////////////////////////////////////////////////////////
// Renderer - Classic
// Classic Dark Forces (DOS) derived renderer.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/colormapAsset.h>
#include "../renderCommon.h"

namespace RendererClassic
{
	void setCamera(s32 cosYaw, s32 sinYaw, s32 x, s32 y, s32 z, s32 sectorId);
	void draw(u8* display, const ColorMap* colormap);

	void setupLevel();
}