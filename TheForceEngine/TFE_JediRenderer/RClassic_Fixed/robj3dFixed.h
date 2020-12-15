#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/modelAsset_Jedi.h>
#include "../rmath.h"
#include "../rlimits.h"
#include "../robject.h"

struct RSector;

namespace TFE_JediRenderer
{
	namespace RClassic_Fixed
	{
		void model_draw(SecObject* obj, JediModel* model);
	}
}
