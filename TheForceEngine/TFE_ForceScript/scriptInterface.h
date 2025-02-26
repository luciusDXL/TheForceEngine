#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "forceScript.h"
#include "float2.h"
#include "float3.h"
#include "float4.h"
#include "float2x2.h"
#include "float3x3.h"
#include "float4x4.h"
#include "scriptAPI.h"

class ScriptAPIClass
{
public:
	virtual bool scriptRegister(ScriptAPI api) = 0;
};

namespace TFE_ScriptInterface
{
	// API interface.
	void registerScriptInterface(ScriptAPI api);
	void setAPI(ScriptAPI api, const char* searchPath);
	void reset();

	// Pre/re-compile a script.
	bool recompileScript(const char* scriptName);

	// Run Scripts, compile on demand.
	// If a script has already been compiled, it will just use it by default.
	template<typename... Args>
	void runScript(const char* scriptName, const char* scriptFunc, Args... inputArgs);
	void runScript(const char* scriptName, const char* scriptFunc);

	// Update - run queued scripts.
	void update();
}
