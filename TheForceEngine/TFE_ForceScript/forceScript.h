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

	// Initialize and destroy script system.
	void init();
	void destroy();
	// Run any active script functions.
	void update();
	// Stop all running script functions.
	void stopAllFunc();

	// Compile module.
	ModuleHandle createModule(const char* moduleName, const char* sectionName, const char* srcCode);
	ModuleHandle createModule(const char* moduleName, const char* filePath);
	// Find a specific script function in a module.
	FunctionHandle findScriptFunc(ModuleHandle modHandle, const char* funcName);

	// Execute a script function.
	// It won't be executed immediately, it will happen on the next TFE_ForceScript update.
	// ---------------------------------------
	// Returns -1 on failure or id on success.
	// Note id is only valid as long as the function is running.
	s32 execFunc(FunctionHandle funcHandle);
	// Resume a suspended script function given by id.
	void resume(s32 id);
}  // TFE_ForceScript