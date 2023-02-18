#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_Jedi/Math/core_math.h>
#include "robjData.h"

namespace TFE_Jedi
{
	void objOverrides_init();
	void objOverrides_destroy();

	s32 objOverrides_getIndex(const char* assetName);
	s32 objOverrides_addLight(const char* assetName, Light light);
	void objOverrides_getLight(s32 index, s32 animId, s32 frameId, Light* light);
}
