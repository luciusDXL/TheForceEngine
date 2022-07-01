#include "vmConfig.h"

#ifdef VM_ENABLE
#include <TFE_System/system.h>
#include "vm.h"
#include "vmOps.h"
#include "value.h"
#include "jit.h"
#include <assert.h>

namespace TFE_ForceScript
{
	struct IRCode
	{
		vmCall func;
		Instruction instr;
	};
	IRCode s_funcOpt[256];

	FsValue* s_stackPtr;
	Instruction* s_func;
	Instruction s_curInstr;
	s32 s_ip;
	s32 s_funcSize;
	FsValue s_retValue;

	bool init()
	{
		return false;
	}

	void destroy()
	{
	}
		
	void optimizeFunc(Instruction* func, s32 funcSize)
	{
		for (s32 i = 0; i < funcSize; i++)
		{
			s_funcOpt[i].func  = s_vmOpCalls[I_OP(func[i])];
			s_funcOpt[i].instr = func[i];
		}
	}

	// stackPtr = current position on stack, the arguments are the initial values.
	// Note: it is an error to jump outside of a function.
	FsValue callFunc(const IRCode* const func, const s32 funcSize, FsValue* stackPtr)
	{
		s_stackPtr = stackPtr;

		s_retValue = fsValue_createNull();	// Default return value is null.
		s_funcSize = funcSize;
		for (s_ip = 0; s_ip < funcSize; )
		{
			const IRCode* const code = &func[s_ip];
			s_ip++;

			code->func(code->instr);
		}
		return s_retValue;
	}

	f32 cversion(s32 arg0, s32 arg1, f32 arg2)
	{
		s32 r3i = arg0 + arg1;
		f32 r3f = f32(r3i);
		for (s32 i = 0; i < 100; i++)
		{
			r3f += arg2;
		}
		return r3f;
	}

	f32 cversion2(Instruction* func, FsValue* stackPtr)
	{
		s_stackPtr = stackPtr;

		s_vmOpCalls[FSF_OP_LOAD](func[0]);
		s_vmOpCalls[FSF_OP_LOAD](func[1]);
		s_vmOpCalls[FSF_OP_IADD](func[2]);

		for (s32 i = 0; i < 100; i++)
		{
			s_vmOpCalls[FSF_OP_FADD](func[3]);
		}

		s_vmOpCalls[FSF_OP_RET](func[7]);
		return *fsValue_getFloatPtr(s_retValue);
	}

	s32 test()
	{
		FsValue v0 = fsValue_createInt(-237);
		FsValue v1 = fsValue_createFloat(3.141516f);

		char outStr[256];
		char outStr2[256];
		fsValue_toString(v0, outStr);
		TFE_System::debugWrite("Test", "v0 = %s", outStr);

		fsValue_toString(v1, outStr);
		TFE_System::debugWrite("Test", "v1 = %s", outStr);

		s32* testCast0 = (s32*)&v0;
		f32* testCast1 = (f32*)&v1;

		/* -- TODO: Map rN to registers and have seperate load/store.
		load r4, 0		-- load integer value into r4
		load r5, 100	-- load integer value into r5
		iadd r3, r0, r1 -- add function arguments and store in r3
		toFloat r3, r3  -- cast dynamic value as float.
		
		loop:
			fadd r3, r3, r2 -- r3 = r3 + r2
			inc r4			-- increment loop counter in r4
			lt r4, r5		-- skip next instruction unless r4 < r5
			jmp loop		-- goto loop

		ret r3		-- return r3

		-----------------------

		load r4, 0		-- load integer value into r4
		load r5, 100	-- load integer value into r5
		load f1, stack[2]
		load r0, stack[0]
		load r1, stack[1]
		iadd r3, r0, r1 -- add function arguments and store in r3
		toFloat f0, r3  -- cast dynamic value as float.

		-- for (..100) {}; for (1..100) {}; for (i = ..100) {}; for (i = 1..100) {}
		loop 100        -- loop opcodes: OP_LOOP_CNT cnt; OP_LOOP_RNG start, end; 
			fadd f3, f3, f1  -- r3 = r3 + r2
			endLoop

		ret r3
		*/

		Instruction testFunc[] =
		{
			/*0*/ I_OP_ARG0_1(FSF_OP_LOAD, 4, 0),		// load r4, 0
			/*1*/ I_OP_ARG0_1(FSF_OP_LOAD, 5, 100),		// load r5, 100
			/*2*/ I_OP_ARG0_1_2(FSF_OP_IADD, 3, 0, 1),	// iadd r3, r0, r1
			/*3*/ I_OP_ARG0_1(FSF_OP_CAST_TO_FLOAT, 3, 3),	// mov r3, float(r3)
			// Loop -> addr = 4
			/*4*/ I_OP_ARG0_1_2(FSF_OP_FADD, 3, 3, 2),	// fadd r3, r3, r2
			/*5*/ I_OP_ARG0(FSF_OP_INC, 4),				// inc r4
			/*6*/ I_OP_ARG0_1(FSF_OP_LT, 4, 5),			// r4 < r5 ? ip++
			/*7*/ I_OP_ARG0(FSF_OP_JMP, 4),				// goto 4
			// End Loop
			/*8*/ I_OP_ARG0(FSF_OP_RET, 3),				// return r3
		};

		jit_init();
		ScriptJitFunc jitFunc = jit_compileScriptFunc("Test", testFunc, TFE_ARRAYSIZE(testFunc));
		
		f64 dtSec = 0.0;
		f64 dtSecJit = 0.0;
		f64 dtSecC = 0.0;

		optimizeFunc(testFunc, TFE_ARRAYSIZE(testFunc));

		FsValue retValue;
		FsValue retValueJit;
		f32 retValueC;

		FsValue stack[256];
		for (s32 i = 0; i < 100000; i++)
		{
			// "push" values onto the stack.
			u64 start = TFE_System::getCurrentTimeInTicks();
				stack[0] = fsValue_createInt(-237);
				stack[1] = fsValue_createInt(101);
				stack[2] = fsValue_createFloat(37.2f);
				retValue = callFunc(s_funcOpt, TFE_ARRAYSIZE(testFunc), stack);
			u64 dt = TFE_System::getCurrentTimeInTicks() - start;
			dtSec += TFE_System::convertFromTicksToSeconds(dt);

			// "push" values onto the stack.
			start = TFE_System::getCurrentTimeInTicks();
				stack[0] = fsValue_createInt(-237);
				stack[1] = fsValue_createInt(101);
				stack[2] = fsValue_createFloat(37.2f);
				retValueJit = jitFunc(stack);
			dt = TFE_System::getCurrentTimeInTicks() - start;
			dtSecJit += TFE_System::convertFromTicksToSeconds(dt);

			start = TFE_System::getCurrentTimeInTicks();
				retValueC = cversion(-237, 101, 37.2f);
				//retValueC = cversion2(testFunc, stack);
			dt = TFE_System::getCurrentTimeInTicks() - start;
			dtSecC += TFE_System::convertFromTicksToSeconds(dt);
		}
		
		fsValue_toString(retValue, outStr);
		fsValue_toString(retValueJit, outStr2);
		TFE_System::debugWrite("Test", "func(-237, 101, 37.2f) = [Interp] %s, [JIT] %s, [C] %f", outStr, outStr2, retValueC);
		TFE_System::debugWrite("Test", "Execution time = [Interp] %fms, [JIT] %fms, [C] %fms", dtSec * 1000.0 / 100000.0, dtSecJit * 1000.0 / 100000.0, dtSecC * 1000.0 / 100000.0);

		jit_destroy();
				
		return 1;
	}
}
#endif