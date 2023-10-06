#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Config
// Replacement for the Jedi Config for TFE.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

namespace TFE_ForceScript
{
	// Opaque Handles.
	typedef void* ModuleHandle;
	typedef void* FunctionHandle;

	void init();
	void destroy();

	ModuleHandle createModule(const char* moduleName, const char* sectionName, const char* srcCode);
	FunctionHandle findScriptFunc(ModuleHandle modHandle, const char* funcName);
	void execFunc(FunctionHandle funcHandle);
}  // TFE_ForceScript