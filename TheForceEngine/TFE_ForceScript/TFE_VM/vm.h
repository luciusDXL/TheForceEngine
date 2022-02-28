#pragma once
/***********************************************************
 TFE VM
 -----------------------------------------------------------
 The core idea is to create the VM and assembly language
 first with the idea that the JIT can be implemented
 against the VM and performance tested before layering on
 the high level language.
 -----------------------------------------------------------
 Core concepts (VM)
 -----------------------------------------------------------
 * Register based.
 * Simple structure to call into native code.
 * Simple to call into script functions.
 * Compiled as modules, where each module has its own memory
   and state. So there can be many instances of the same
   module running at the same time.
 * Structures can be created, but they are POD types.
 * Members must be predefined (i.e. no Lua style runtime 
   structure changes), so member accesses are indices in the
   VM.
 * Global variables can exist but are local to the current
   running script (i.e. each instance has its own memory).
 * Global variables are predefined and so can be indexed at
   runtime.
 * Local variables are added and removed based on scope.
************************************************************/
#include <TFE_System/types.h>
#include "value.h"
#include "opcodes.h"
#include "registers.h"

enum VM_Constants
{
	INVALID_LABEL  = 0xffffffffu,
	INVALID_GLOBAL = 0xffffffffu,
	STACK_SIZE = 1024,
	CALLSTACK_SIZE = 16,
	MAX_LABELS = 256,
	MAX_GLOBALS = 256,
};

// This is pretty inefficient, but good enough for now.
struct TFE_Instruction
{
	u32 opCode;
	TFE_Value args[3];	// up to 3 arguments.
};

struct TFE_Module
{
	char name[32];
	bool compiled;
	bool init;

	u32 labelCount;
	u32 globalVarCount;
	u32 instrCount;
	u32 argCount;
	u32 startLabel;
	u32 runtimeErrorCount;

	u32* labelAddr;
	// Registers change to stack:
	//  On function call, allocate enough stack space for:
	//    [TFE_Value] arguments (0, ...).
	//    [TFE_Value] local variables.
	// Then these values are referenced by index in the instructions.
	// Function calls also preallocate and store constants.
	// Return value saved in source function local.
	// 8 bits = up to 250 arguments + locals.
	// 9 bits can represent all "registers" + constants.
	// Change instruction: 6 bits = opcode, 8 bits = dstReg, [9 bits = arg0, 9 bits = arg1], [18-bits = arg0 (signed)], [18-bits = arg0 (unsigned)].
	u32* instr;			// 8 bits = opcode, 24 bits = argument offset.
	TFE_Value* globalVar;
	TFE_Value* args;	// full list of arguments used by instructions.

	// Each module has its own memory so they can run independently.
	TFE_Value reg[REG_COUNT];
	TFE_Value stack[STACK_SIZE];
	u32 callStack[CALLSTACK_SIZE];
};

#define ARG_IMM_INT(x)   TFE_VM::argImmInt(x)
#define ARG_IMM_FLOAT(x) TFE_VM::argImmFloat(x)
#define ARG_REG(x)       TFE_VM::argRegister(x)
#define ARG_GLOBAL(name) TFE_VM::argGlobal(name)
#define ARG_LABEL(name)  TFE_VM::argLabel(name)
#define ARG_NONE()       {0, VTYPE_INT, VFLAG_NONE, 0}

namespace TFE_VM
{
	TFE_Module* startBuildModule(const char* name);
	void finishBuildModule();
	void freeModule(TFE_Module* module);

	bool serializeModule(TFE_Module* module, const char* filePath);
	TFE_Module* loadModule(const char* filePath);

	void addLabel(const char* label);
	void addGlobal(const char* name, TFE_Value value);
	u32  getLabelIndex(const char* label);
	u32  getGlobalIndex(const char* name);
	void addInstruction(TFE_Opcode opcode, TFE_Value arg0=ARG_NONE(), TFE_Value arg1=ARG_NONE(), TFE_Value arg2=ARG_NONE());

	// Returns true if the module has finished running.
	// If a yield is encountered, it will return false instead.
	bool runModule(TFE_Module* module);

	TFE_Value getGlobalValue(TFE_Module* module, s32 index);

	static inline TFE_Value argImmInt(s32 x)
	{
		TFE_Value arg;
		arg.data.i32 = x;
		arg.type = VTYPE_INT;
		arg.size = 1;
		arg.flags = VFLAG_NONE;
		return arg;
	}

	static inline TFE_Value argImmFloat(f32 x)
	{
		TFE_Value arg;
		arg.data.f32 = x;
		arg.type = VTYPE_FLOAT;
		arg.size = 1;
		arg.flags = VFLAG_NONE;
		return arg;
	}

	static inline TFE_Value argRegister(TFE_Register reg)
	{
		TFE_Value arg;
		arg.data.u32 = reg;
		arg.type = VTYPE_REG;
		arg.size = 1;
		arg.flags = VFLAG_NONE;
		return arg;
	}

	static inline TFE_Value argGlobal(const char* name)
	{
		TFE_Value arg;
		arg.data.u32 = getGlobalIndex(name);
		arg.type = VTYPE_GLOBAL;
		arg.size = 1;
		arg.flags = VFLAG_NONE;
		return arg;
	}

	static inline TFE_Value argLabel(const char* name)
	{
		TFE_Value arg;
		arg.data.u32 = getLabelIndex(name);
		arg.type = VTYPE_INT;
		arg.size = 1;
		arg.flags = VFLAG_NONE;
		return arg;
	}
};