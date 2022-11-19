#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Animation Logic-
// Handles cyclic "fire and forget" sprite animations.
// This includes animated sprite objects, explosions, and hit effects.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include "logic.h"
#include "time.h"

namespace TFE_DarkForces
{
	Logic* obj_createVueLogic(SecObject* obj, LogicSetupFunc* setupFunc);

	void vue_resetState();

	// Serialization
	void vueLogic_serialize(Logic*& logic, SecObject* obj, Stream* stream);
}  // namespace TFE_DarkForces