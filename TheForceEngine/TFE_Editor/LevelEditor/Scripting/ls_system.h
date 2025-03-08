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

namespace LevelEditor
{
	class LS_System : public ScriptAPIClass
	{
	public:
		// Properties
		s32 version = 1;
		// Functions
		static void error(asIScriptGeneric* gen);
		static void warning(asIScriptGeneric* gen);
		static void print(asIScriptGeneric* gen);
		void clearOutput();
		void runScript(std::string& scriptName);
		void showScript(std::string& scriptName);
		// System
		bool scriptRegister(ScriptAPI api) override;
	};
}
