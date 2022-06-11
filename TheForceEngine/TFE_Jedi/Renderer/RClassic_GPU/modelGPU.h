#pragma once
//////////////////////////////////////////////////////////////////////
// Manage and render 3DO models.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include "sbuffer.h"

namespace TFE_Jedi
{
	bool model_init();
	void model_destroy();
	// This needs to be called *after* texture packing is complete so that textureIds are already set.
	void model_loadLevelModels();

	void model_drawListClear();
	void model_drawListFinish();

	void model_add(JediModel* model, Vec3f posWS, fixed16_16* transform);
	void model_drawList();

	s32  model_getDrawListSize();
}  // TFE_Jedi
