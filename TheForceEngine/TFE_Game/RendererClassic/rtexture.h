#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/textureAsset.h>
#include "rmath.h"

namespace RClassicTexture
{
	TextureFrame* texture_getFrame(Texture* texture, u32 baseFrame = 0);
}
