#include "jit.h"
#include "vm.h"
#include <assert.h>
#include <string>
#include <vector>
#include <map>

#ifdef VM_ENABLE
#include "asmjit/x86.h"

using namespace asmjit;
// Incomment to enable per-instruction validation at the assembler level.
#define JIT_DEBUG 0

namespace TFE_ForceScript
{
	typedef std::map<std::string, ScriptJitFunc> ScriptJitMap;
	typedef std::vector<ScriptJitFunc> ScriptJitList;

	static JitRuntime s_runtime;
	static ScriptJitMap s_jitFuncs;
	static ScriptJitList s_jitFuncList;
	
	bool jit_init()
	{
		return true;
	}

	void jit_destroy()
	{
		const size_t count = s_jitFuncList.size();
		ScriptJitFunc* func = s_jitFuncList.data();
		for (size_t i = 0; i < count; i++)
		{
			s_runtime.release(func[i]);
		}
		s_jitFuncs.clear();
		s_jitFuncList.clear();
	}

	ScriptJitFunc jit_getScriptFunc(const char* funcName)
	{
		ScriptJitMap::iterator it = s_jitFuncs.find(funcName);
		return it == s_jitFuncs.end() ? nullptr : it->second;
	}

	enum IR_Type
	{
		IRT_INSTR = 0,
		IRT_LABEL,
		IRT_COUNT
	};

	struct IR_Jit
	{
		IR_Type type;
		Instruction it;
		s32 index;
	};
	static std::vector<IR_Jit> s_jitIR;

	#ifdef JIT_DEBUG
		#define emit(x) ierr = as. ## x; assert(ierr == 0)
	#else
		#define emit(x) as. ## x
	#endif

	#define PTR0 8*I_ARG0(ir->it)
	#define PTR1 8*I_ARG1(ir->it)
	#define PTR2 8*I_ARG2(ir->it)

	Label* s_labels;
	s32 s_labelTarget[256];
	s32 s_labelCount = 0;

	void loadValueType(Error& ierr, x86::Assembler& as, x86::Gp dstReg, x86::Mem src)
	{
		// type = (src >> FS_TypeDataShift) & FS_TypeMask
		emit(mov(dstReg, src));
		emit(shr(dstReg,  FS_TypeDataShift));
		emit(and_(dstReg, FS_TypeMask));
	}

	void dynamicCastToFloat(Error& ierr, x86::Assembler& as, x86::Gp type, x86::Xmm dst, x86::Mem src)
	{
		// Create labels.
		s32 labelElse = s_labelCount;
		s32 labelEnd = s_labelCount + 1;
		s_labelCount +=2;
		s_labels[labelElse] = as.newLabel();
		s_labels[labelEnd] = as.newLabel();

		// Check to see if it is integer.
		// TODO: Support NxM types.
		// TODO: Default to 0 for invalid types.

		// Zero out the floating point register so the upper bits aren't undefined (which can cause problems later).
		emit(xorps(dst, dst));

		emit(cmp(type, FST_INT)); // if r0 == FST_INT {
		emit(jnz(s_labels[labelElse]));
			emit(cvtsi2ss(dst, src));
			emit(jmp(s_labels[labelEnd]));
		emit(bind(s_labels[labelElse])); // } else {
			emit(movss(dst, src));
		emit(bind(s_labels[labelEnd]));  // }
	}

	ScriptJitFunc jit_compileScriptFunc(const char* funcName, Instruction* func, s32 funcSize)
	{
		s_jitIR.clear();

		Error ierr;
		CodeHolder code;
		code.init(s_runtime.environment());
		x86::Assembler as(&code);

		// Enable strict validation.
	#ifdef JIT_DEBUG
		as.addDiagnosticOptions(DiagnosticOptions::kRADebugAll);
	#endif

		// We currently only support x64.
	#if defined(_WIN32)
		x86::Gp stackPtr = x86::rcx;
	#else
		x86::Gp stackPtr = x86::rdi;
	#endif

		x86::Gp retValue = x86::rax;

		x86::Gp r0 = x86::rbx;
		x86::Gp r1 = x86::rdx;
		x86::Gp r0_32 = x86::ebx;
		x86::Gp r1_32 = x86::edx;
		x86::Xmm f0 = x86::xmm0;
		x86::Xmm f1 = x86::xmm1;

		// Save the temporary registers.
		emit(push(r0));
		emit(push(r1));

		Label exitLabel = as.newLabel();
		Label labels[256];
		s_labels = labels;
		s_labelCount = 0;

		// First go through and add instructions, noting any labels.
		s_jitIR.reserve(funcSize * 2);
		for (s32 i = 0; i < funcSize; i++)
		{
			Instruction it = func[i];
			IR_Jit ir = { IRT_INSTR, it, i };

			if (I_OP(it) == FSF_OP_JMP)
			{
				s32 offset = I_ARG0(it);
				// Re-write the instruction to point to the label index.
				ir.it = I_OP_ARG0(FSF_OP_JMP, s_labelCount);

				// insert a label.
				s_labels[s_labelCount] = as.newLabel();
				s_labelTarget[s_labelCount] = offset;
				s_labelCount++;
			}
			else if (I_OP(it) == FSF_OP_JL)
			{
				// TODO
			}

			s_jitIR.push_back(ir);
		}

		// Then go back and insert labels.
		for (s32 lidx = 0; lidx < s_labelCount; lidx++)
		{
			const s32 target = s_labelTarget[lidx];

			// Start the loop at the target, since it cannot be found any earlier.
			const size_t count = s_jitIR.size();
			const IR_Jit* ir = s_jitIR.data() + target;
			for (size_t i = target; i < count; i++, ir++)
			{
				if (ir->index == target)
				{
					// insert the label right *before* the target.
					IR_Jit label = { IRT_LABEL, Instruction(lidx), -1 };
					s_jitIR.insert(s_jitIR.begin() + i, label);
					break;
				}
			}
		}
				
		// Then insert each instruction and label.
		const size_t count = s_jitIR.size();
		const IR_Jit* ir = s_jitIR.data();
		for (size_t i = 0; i < count; i++, ir++)
		{
			if (ir->type == IRT_LABEL)
			{
				as.bind(s_labels[ir->it]);
			}
			else // IRT_INSTR
			{
				switch (I_OP(ir->it))
				{
					case FSF_OP_NOP:
					{
					} break;
					case FSF_OP_MOVE:
					{
					} break;
					case FSF_OP_LOAD:
					{
						// In x86_64 the only instruction that takes a 64-bit Immediate value is mov reg64, imm
						// This means that storing 64-bit immediate values into memory takes two instructions.
						uint64_t imm = fsValue_createInt(I_ARG1(ir->it));
						emit(mov(x86::dword_ptr(stackPtr, PTR0), imm & 0xffffffffull));
						emit(mov(x86::dword_ptr(stackPtr, PTR0 + 4), imm >> 32ull));
					} break;
					case FSF_OP_INC:
					{
						emit(add(x86::dword_ptr(stackPtr, PTR0), 1));
					} break;
					case FSF_OP_DEC:
					{
						emit(sub(x86::dword_ptr(stackPtr, PTR0), 1));
					} break;
					case FSF_OP_IADD:
					{
						x86::Mem arg1 = x86::dword_ptr(stackPtr, PTR1);
						x86::Mem arg2 = x86::dword_ptr(stackPtr, PTR2);

						// Load first argument and add memory value.
						emit(mov(r0_32, arg1));
						emit(add(r0_32, arg2));

						// Write the value and type to memory.
						emit(mov(x86::dword_ptr(stackPtr, PTR0), r0_32));
						emit(mov(x86::dword_ptr(stackPtr, PTR0 + 4), FST_INT << 16u));
					} break;
					case FSF_OP_FADD:
					{
						x86::Mem arg1 = x86::dword_ptr(stackPtr, PTR1);
						x86::Mem arg2 = x86::dword_ptr(stackPtr, PTR2);

						// Load first argument and add memory value.
						emit(xorps(f0, f0));  // Zero out the floating point register so the upper bits aren't undefined (which can cause problems later).
						emit(movss(f0, arg1));
						emit(addss(f0, arg2));

						// Write the value and type to memory.
						emit(movss(x86::dword_ptr(stackPtr, PTR0), f0));
						emit(mov(x86::dword_ptr(stackPtr, PTR0 + 4), FST_FLOAT << 16u));
					} break;
					case FSF_OP_CAST_TO_FLOAT:
					{
						x86::Mem arg1_qw = x86::qword_ptr(stackPtr, PTR1);
						x86::Mem arg1 = x86::dword_ptr(stackPtr, PTR1);

						loadValueType(ierr, as, r0, arg1_qw);		// r0 = type of arg1.
						dynamicCastToFloat(ierr, as, r0, f0, arg1); // f0 = float(r0)

						// Save the value and type.
						emit(movss(x86::dword_ptr(stackPtr, PTR0), f0));
						emit(mov(x86::dword_ptr(stackPtr, PTR0 + 4), FST_FLOAT << 16u));
					} break;
					case FSF_OP_CAST_TO_INT:
					{
						// TODO
					} break;
					case FSF_OP_ADD:
					{
						// TODO
					} break;
					case FSF_OP_LT:
					{
						// If the next instruction is a jump, combine them here.
						if ((ir + 1)->type == IRT_INSTR && I_OP((ir + 1)->it) == FSF_OP_JMP)
						{
							// Test here.
							emit(mov(r0_32, x86::dword_ptr(stackPtr, PTR0)));
							emit(cmp(r0_32, x86::dword_ptr(stackPtr, PTR1)));
							emit(jl(s_labels[I_ARG0((ir+1)->it)]));

							// Skip past the next instruction.
							i++;
							ir++;
						}
					} break;
					case FSF_OP_JMP:
					{
						emit(jmp(s_labels[I_ARG0(ir->it)]));
					} break;
					case FSF_OP_JL:
					{
					} break;
					case FSF_OP_RET:
					{
						x86::Mem arg0 = x86::qword_ptr(stackPtr, PTR0);
						emit(mov(retValue, arg0));
						emit(jmp(exitLabel));
					} break;
				}
			}
		}

		emit(bind(exitLabel));
		// Restore the temporary registers.
		emit(pop(r1));
		emit(pop(r0));
		// Return from our function.
		emit(ret());

		ScriptJitFunc fn;
		Error err = s_runtime.add(&fn, &code);
		if (err)
		{
			return nullptr;
		}
		s_jitFuncs[funcName] = fn;
		s_jitFuncList.push_back(fn);
		return fn;
	}
}
#endif