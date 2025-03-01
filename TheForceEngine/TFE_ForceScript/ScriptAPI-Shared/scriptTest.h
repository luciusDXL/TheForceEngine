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

namespace TFE_ForceScript
{
	class ScriptTest : public ScriptAPIClass
	{
	public:
		static void expectEq(asIScriptGeneric* gen);
		static void expectNotEq(asIScriptGeneric* gen);
		static void expectTrue(asIScriptGeneric* gen);
		static void expectFalse(asIScriptGeneric* gen);

		void setTestSystem(const std::string& name);
		void setTestName(const std::string& name);
		void report();
		
		static bool checkInTest();
		static bool compareValues(s32 typeId, void* ref0, void* ref1, bool equal, std::string& values);
		static bool checkBoolean(s32 typeId, void* ref, bool testForTrue, std::string& errorMsg);
		static bool validateExpect(asIScriptGeneric* gen);
		static bool validateBoolean(asIScriptGeneric* gen);

		// System
		bool scriptRegister(ScriptAPI api) override;

	private:
		struct Test
		{
			std::string name;
			s32 errorCount;
		};

		static std::string s_system;
		static std::vector<Test> s_tests;
	};
}
