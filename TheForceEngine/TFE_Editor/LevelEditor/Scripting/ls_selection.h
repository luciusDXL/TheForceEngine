#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/system.h>
#ifdef ENABLE_FORCE_SCRIPT
#include <TFE_System/types.h>
#include <TFE_ForceScript/float2.h>
#include "ls_api.h"
#include <angelscript.h>
#include <string>

namespace LevelEditor
{
	class LS_Selection : public LS_API
	{
	public:
		TFE_ForceScript::float2 getPositionXZ(s32 index = 0);
		// System
		bool scriptRegister(asIScriptEngine* engine) override;
	};
}
#endif