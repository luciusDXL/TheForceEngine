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

// TODO:
// * Add boolean operations (&&, ||)
// * Add native call.
// * Add string type and remove 'prn' function (replace with native version).
// * Add arrays.
// * Add function environment in addition to labels (setup with 'call').
// * Add local variables.
// * Add "assembler".
// * Setup proper "script" system that can manage modules and yield delays.
// * Add "structures" (since they are PODS, I think it can be done at this level).

enum TFE_Opcode
{
	OP_NOP = 0,
	// Memory.
	OP_MOV,		// mov dst, src - copy the value of src to dst.
	// Stack.
	OP_PUSH,	// push value - pushes 'value' onto the stack.
	OP_POP,		// pop output - pops a 'value' off the stack and stores it into output.
	// Functions.
	OP_CALL,	// calls a script function.
	OP_RET,		// returns from a script function.
	OP_YIELD,	// yield delay - 0 = immediate (next frame), 0xffffffff = sleep, otherwise delay in ticks.
	// Arithmetic - works for both integers and floats.
	OP_INC,		// inc value - value++
	OP_DEC,		// dec value - value--
	OP_ADD,		// add dst, arg0, arg1 : dst = arg0 + arg1
	OP_SUB,		// sub dst, arg0, arg1 : dst = arg0 - arg1
	OP_MUL,		// mul dst, arg0, arg1 : dst = arg0 * arg1
	OP_DIV,		// div dst, arg0, arg1 : dst = arg0 / arg1
	OP_MOD,		// mod dst, arg0, arg1 : dst = arg0 % arg1
	// Unary operators - Int only.
	OP_NOT,		// not dst, arg0 : dst = ~arg0
	// Binary operators - Int only.
	OP_XOR,		// xor dst, arg0, arg1 : dst = arg0 ^ arg1
	OP_OR,		// or dst, arg0, arg1  : dst = arg0 | arg1
	OP_AND,		// and dst, arg0, arg1 : dst = arg0 & arg1
	OP_SHL,		// shl dst, arg0, arg1 : dst = arg0 << arg1
	OP_SHR,		// shr dst, arg0, arg1 : dst = arg0 >> arg1
	// Comparison.
	OP_CMP,		// cmp arg0, arg1 : (arg0 == arg1) ? 1 : (arg0 > arg1) ? 2 : 0; Sets flags register.
	// Control Flow.
	OP_JMP,		// jmp arg0 : jumps to label index 'arg0'
	OP_JE,		// je arg0 : jumps to label if comparison ==
	OP_JNE,		// !=
	OP_JG,		// >
	OP_JGE,		// >=
	OP_JL,		// < 
	OP_JLE,		// <=

	// Debug.
	OP_PRN,		// prn arg0 - prints out the value of arg0.

	// Program termination.
	OP_END,		// End of program.
	OP_COUNT
};
