#pragma once
//////////////////////////////////////////////////////////////////////
// Renderer Debugging
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/robjData.h>
#include "frustum.h"
#include "../rsectorRender.h"

struct SceneLight;

namespace TFE_Jedi
{
	void lighting_destroy();			// free buffers.

	// Scene
	void lighting_initScene(s32 sectorCount);
	SceneLight* lighting_addToScene(const Light& light, s32 sectorId);
	void lighting_updateLight(SceneLight* sceneLight, const Light& light, s32 newSector);
	void lighting_freeLight(SceneLight* sceneLight);

	// Per-frame scene traversal
	void lighting_addSectorLights(s32 sectorId, Frustum* viewFrustum);

	// Per-Frame
	void lighting_clear();
	void lighting_submit();					// submit the light list to GPU data.

	void lighting_bind(s32 index);
	void lighting_unbind(s32 index);
}  // TFE_Jedi
