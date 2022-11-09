#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Enemy Generator
// This is a logic that handles spawning enemies based on various
// parameters.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include "logic.h"
#include "time.h"

namespace TFE_DarkForces
{
	Logic* obj_createGenerator(SecObject* obj, LogicSetupFunc* setupFunc, KEYWORD genType);

	// Serialization
	void generatorLogic_serialize(Logic*& logic, Stream* stream);
}  // namespace TFE_DarkForces