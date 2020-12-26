#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

struct JediModel;

namespace TFE_JediRenderer
{
	struct SecObject;
	namespace RClassic_Fixed
	{
		void robj3d_draw(SecObject* obj, JediModel* model);
	}
}
