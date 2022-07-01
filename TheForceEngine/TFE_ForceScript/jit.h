#pragma once
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#include "vmConfig.h"

#ifdef VM_ENABLE
#include <TFE_System/types.h>
#include "value.h"
#include "vmOps.h"

namespace TFE_ForceScript
{
	typedef FsValue(*ScriptJitFunc)(FsValue*);

	bool jit_init();
	void jit_destroy();

	ScriptJitFunc jit_getScriptFunc(const char* funcName);
	ScriptJitFunc jit_compileScriptFunc(const char* funcName, Instruction* func, s32 funcSize);
}
#endif