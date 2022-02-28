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
 * Yield as first class citizen.
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

enum TFE_Register
{
	// Specific registers.
	REG_IP = 0,	// Instruction pointer.
	REG_SP,		// Stack pointer.
	REG_CS,		// Callstack pointer.
	REG_FL,		// Flags register.

	// 16 General purpose integer registers, R0 - R15
	REG_R0,
	REG_R1,
	REG_R2,
	REG_R3,
	REG_R4,
	REG_R5,
	REG_R6,
	REG_R7,
	REG_R8,
	REG_R9,
	REG_R10,
	REG_R11,
	REG_R12,
	REG_R13,
	REG_R14,
	REG_R15,

	REG_COUNT,
};
