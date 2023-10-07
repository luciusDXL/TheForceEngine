#include "forceScript.h"
#include <TFE_System/system.h>
#include <TFE_FrontEndUI/frontEndUI.h>
#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <string>
#include <assert.h>

#ifdef ENABLE_FORCE_SCRIPT
#include <angelscript.h>

namespace TFE_ForceScript
{
	// Functions
	// Print the script string to the standard output stream
	void system_print(std::string& msg)
	{
		TFE_FrontEndUI::logToConsole(msg.c_str());
	}
	// TODO: Finish the "system" script API.

	void registerSystemFunctions(void* engine_)
	{
		asIScriptEngine* engine = (asIScriptEngine*)engine_;
		
		// Register functions.
		s32 res = engine->RegisterGlobalFunction("void system_print(const string &in)", asFUNCTION(system_print), asCALL_CDECL); assert(res >= 0);
	}
}  // TFE_ForceScript

#else

namespace TFE_ForceScript
{
	void registerSystemFunctions(void* engine_) {}
}  // TFE_ForceScript

#endif  // ENABLE_FORCE_SCRIPT