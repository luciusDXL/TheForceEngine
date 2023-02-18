#pragma once
//////////////////////////////////////////////////////////////////////
// Renderer Debugging
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/robjData.h>
#include "../rsectorRender.h"

namespace TFE_Jedi
{
	void lighting_enable(bool enable, s32 sectorCount);	// enable dynamic lighting.
	void lighting_destroy();			// free buffers.

	void lighting_clear();
	void lighting_add(const Light& light, s32 objIndex, s32 sectorIndex);	// add a light to the scene.
	void lighting_submit();					// submit the light list to GPU data.

	void lighting_bind(s32 index);
	void lighting_unbind(s32 index);
}  // TFE_Jedi
