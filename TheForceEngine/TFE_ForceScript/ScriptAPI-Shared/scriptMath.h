#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/system.h>
#include <TFE_System/types.h>
#include <TFE_ForceScript/scriptInterface.h>
#include <string>

#define Vec2f_to_float2(v) TFE_ForceScript::float2(v.x, v.z)
#define float2_to_Vec2f(v) Vec2f{v.x, v.y};

namespace TFE_ForceScript
{
	class ScriptMath : public ScriptAPIClass
	{
	public:
		// System
		bool scriptRegister(ScriptAPI api) override;
	};
}
