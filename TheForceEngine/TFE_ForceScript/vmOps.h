#pragma once
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#include "vmConfig.h"
#ifdef VM_ENABLE
#include <TFE_System/types.h>
#include "value.h"

namespace TFE_ForceScript
{
	enum OpCode
	{
		FSF_OP_NOP = 0,
		FSF_OP_MOVE,
		FSF_OP_LOAD,
		FSF_OP_INC,
		FSF_OP_DEC,
		FSF_OP_IADD,	// All values are integer.
		FSF_OP_FADD,	// All values are float.

		FSF_OP_CAST_TO_FLOAT,	// Cast from any Value type to float.
		FSF_OP_CAST_TO_INT,     // Cast from any Value type to int.

		FSF_OP_ADD,		// Add arbitrary types (slow).
		
		FSF_OP_LT,		
		FSF_OP_JMP,
		FSF_OP_JL,	// Jump if less.
		FSF_OP_RET,
	};

	// Instruction = 
	// OpCode (10) | Arg0 (18) | Arg1 (18) | Arg2 (18)
	// OpCode (10) | Arg0 (18) | Arg1 (36)
	typedef u64 Instruction;

	#define I_OP_SIZE  (1ull << 10ull)
	#define I_OP_MASK  (I_OP_SIZE - 1)
	#define I_ARG_SIZE (1ull << 18ull)
	#define I_ARG_MASK (I_ARG_SIZE - 1ull)
	#define I_ARG2_MASK (I_ARG_SIZE*2 - 1ull)

	#define I_OP(i)   ((i) & I_OP_MASK)
	#define I_ARG0(i) (((i) >> 10ull) & I_ARG_MASK)
	#define I_ARG1(i) (((i) >> 28ull) & I_ARG_MASK)
	#define I_ARG2(i) (((i) >> 46ull) & I_ARG_MASK)
	#define I_ARG12(i) (((i) >> 28ull) & I_ARG2_MASK)

	#define I_OP_ARG0(i, arg0)					(u64(i) | (u64(arg0) << 10ull))
	#define I_OP_ARG0_1(i, arg0, arg1)			(u64(i) | (u64(arg0) << 10ull) | (u64(arg1) << 28ull))
	#define I_OP_ARG0_1_2(i, arg0, arg1, arg2)	(u64(i) | (u64(arg0) << 10ull) | (u64(arg1) << 28ull) | (u64(arg2) << 46ull))

	extern FsValue* s_stackPtr;
	extern Instruction* s_func;
	extern Instruction s_curInstr;
	extern s32 s_ip;
	extern s32 s_funcSize;
	extern FsValue s_retValue;

	typedef void(*vmCall)(Instruction);

	void opNop(Instruction curInstr);
	void opMove(Instruction curInstr);
	void opLoad(Instruction curInstr);
	void opInc(Instruction curInstr);
	void opDec(Instruction curInstr);
	void opIadd(Instruction curInstr);
	void opFadd(Instruction curInstr);

	void opCastToFloat(Instruction curInstr);
	void opCastToInt(Instruction curInstr);
	void opAdd(Instruction curInstr);

	void opLt(Instruction curInstr);
	void opJmp(Instruction curInstr);
	void opJl(Instruction curInstr);
	void opRet(Instruction curInstr);

	const vmCall s_vmOpCalls[] =
	{
		opNop,
		opMove,
		opLoad,
		opInc,
		opDec,
		opIadd,
		opFadd,
		opCastToFloat,
		opCastToInt,
		opAdd,
		opLt,
		opJmp,
		opJl,
		opRet,
	};

	inline void opNop(Instruction curInstr)
	{
	}

	inline void opMove(Instruction curInstr)
	{
	}

	inline void opLoad(Instruction curInstr)
	{
		s_stackPtr[I_ARG0(curInstr)] = fsValue_createInt(I_ARG1(curInstr));
	}

	// Assume that inc and dec are only emitted on pure int values.
	inline void opInc(Instruction curInstr)
	{
		s32* value = fsValue_getIntPtr(s_stackPtr[I_ARG0(curInstr)]);
		(*value)++;
	}

	inline void opDec(Instruction curInstr)
	{
		s32* value = fsValue_getIntPtr(s_stackPtr[I_ARG0(curInstr)]);
		(*value)--;
	}

	inline void opIadd(Instruction curInstr)
	{
		const FsValue* const v0 = &s_stackPtr[I_ARG1(curInstr)];
		const FsValue* const v1 = &s_stackPtr[I_ARG2(curInstr)];
		// For now pretent v0 and v1 are int.
		s_stackPtr[I_ARG0(curInstr)] = fsValue_createInt(*((s32*)v0) + *((s32*)v1));
	}

	inline void opFadd(Instruction curInstr)
	{
		const FsValue* const v0 = &s_stackPtr[I_ARG1(curInstr)];
		const FsValue* const v1 = &s_stackPtr[I_ARG2(curInstr)];
		s_stackPtr[I_ARG0(curInstr)] = fsValue_createFloat(*((f32*)v0) + *((f32*)v1));
	}

	inline void opCastToFloat(Instruction curInstr)
	{
		const FsValue* const v0 = &s_stackPtr[I_ARG1(curInstr)];
		f32 cast = fsValue_readAsFloat(v0);
		s_stackPtr[I_ARG0(curInstr)] = fsValue_createFloat(cast);
	}

	inline void opCastToInt(Instruction curInstr)
	{
		const FsValue* const v0 = &s_stackPtr[I_ARG1(curInstr)];
		s32 cast = fsValue_readAsInt(v0);
		s_stackPtr[I_ARG0(curInstr)] = fsValue_createInt(cast);
	}

	inline void opAdd(Instruction curInstr)
	{
		// TODO:
	}

	inline void opLt(Instruction curInstr)
	{
		const FsValue* const v0 = &s_stackPtr[I_ARG0(curInstr)];
		const FsValue* const v1 = &s_stackPtr[I_ARG1(curInstr)];
		if (!(fsValue_readAsInt(v0) < fsValue_readAsInt(v1)))
		{
			// Skip the next instruction if the condition is not true.
			s_ip++;
		}
	}

	inline void opJmp(Instruction curInstr)
	{
		s_ip = I_ARG0(curInstr);
	}

	inline void opJl(Instruction curInstr)
	{
		const FsValue* const v0 = &s_stackPtr[I_ARG0(curInstr)];
		const FsValue* const v1 = &s_stackPtr[I_ARG1(curInstr)];
		if (fsValue_readAsInt(v0) < fsValue_readAsInt(v1))
		{
			s_ip = I_ARG2(curInstr);
		}
	}

	inline void opRet(Instruction curInstr)
	{
		s_retValue = s_stackPtr[I_ARG0(curInstr)];
		// Set the IP to the end so we jump out.
		s_ip = s_funcSize;
	}
}
#endif