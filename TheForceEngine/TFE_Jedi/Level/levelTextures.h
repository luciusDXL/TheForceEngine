#pragma once
//////////////////////////////////////////////////////////////////////
// Level
// Functionality to get level geometry and object textures for use by
// the GPU renderer(s).
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_DarkForces/sound.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_RenderShared/texturePacker.h>
#include "rsector.h"

namespace TFE_Jedi
{
	bool level_getLevelTextures(TextureInfoList& textures, AssetPool pool);
	bool level_getObjectTextures(TextureInfoList& textures, AssetPool pool);
}
