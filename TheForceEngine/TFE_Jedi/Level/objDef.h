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
	void objDef_init();
	void objDef_destroy();

	s32  objDef_getIndex(const char* assetName);
	s32  objDef_addLight(const char* assetName, Light light);
	bool objDef_getLight(s32 index, s32 animId, s32 frameId, Light* light);
}
