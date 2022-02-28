#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/filestream.h>

namespace TFE_VM
{
	static const u32 c_version = 1;

	static const u32 c_opcodeArgCount[OP_COUNT] =
	{
		0, // OP_NOP
		2, // OP_MOV,		// mov dst, src - copy the value of src to dst.
		1, // OP_PUSH,		// push value - pushes 'value' onto the stack.
		1, // OP_POP,		// pop output - pops a 'value' off the stack and stores it into output.
		1, // OP_CALL,		// calls a script function.
		0, // OP_RET,		// returns from a script function.
		1, // OP_YIELD,		// returns control to the calling process.
		1, // OP_INC,		// inc value - value++
		1, // OP_DEC,		// dec value - value--
		3, // OP_ADD,		// add dst, arg0, arg1 : dst = arg0 + arg1
		3, // OP_SUB,		// sub dst, arg0, arg1 : dst = arg0 - arg1
		3, // OP_MUL,		// mul dst, arg0, arg1 : dst = arg0 * arg1
		3, // OP_DIV,		// div dst, arg0, arg1 : dst = arg0 / arg1
		3, // OP_MOD,		// mod dst, arg0, arg1 : dst = arg0 % arg1
		2, // OP_NOT,		// not dst, arg0 : dst = ~arg0
		3, // OP_XOR,		// xor dst, arg0, arg1 : dst = arg0 ^ arg1
		3, // OP_OR,		// or dst, arg0, arg1  : dst = arg0 | arg1
		3, // OP_AND,		// and dst, arg0, arg1 : dst = arg0 & arg1
		3, // OP_SHL,		// shl dst, arg0, arg1 : dst = arg0 << arg1
		3, // OP_SHR,		// shr dst, arg0, arg1 : dst = arg0 >> arg1
		2, // OP_CMP,		// cmp arg0, arg1 : (arg0 == arg1) ? 1 : (arg0 > arg1) ? 2 : 0; Sets flags register.
		1, // OP_JMP,		// jmp arg0 : jumps to label index 'arg0'
		1, // OP_JE,		// je arg0 : jumps to label if comparison ==
		1, // OP_JNE,		// !=
		1, // OP_JG,		// >
		1, // OP_JGE,		// >=
		1, // OP_JL,		// < 
		1, // OP_JLE,		// <=
		1, // OP_PRN,		// prn arg0 - prints out the value of arg0.
		0, // OP_END,		// End of program.
	};

	struct Name
	{
		char name[32];
	};

	static TFE_Module* s_curModule = nullptr;
	static Name s_labels[MAX_LABELS];
	static Name s_globals[MAX_GLOBALS];
			
	TFE_Module* startBuildModule(const char* name)
	{
		s_curModule = (TFE_Module*)malloc(sizeof(TFE_Module));
		if (!s_curModule) { return nullptr; }

		memset(s_curModule, 0, sizeof(TFE_Module));
		strcpy(s_curModule->name, name);

		s_curModule->reg[REG_IP].data.u32 = 0;
		return s_curModule;
	}

	void finishBuildModule()
	{
		if (!s_curModule) { return; }
		s_curModule->compiled = true;
		s_curModule->init = true;
	}

	bool serializeModule(TFE_Module* module, const char* filePath)
	{
		if (!module) { return false; }

		FileStream file;
		if (!file.open(filePath, FileStream::MODE_WRITE))
		{
			return false;
		}
		char hdr[] = { 'T', 'F', 'S', 0 };
		file.writeBuffer(hdr, 4);
		file.write(&c_version);

		file.writeBuffer(module->name, 32);
		file.write(&module->labelCount);
		file.write(&module->globalVarCount);
		file.write(&module->instrCount);
		file.write(&module->argCount);
		file.write(&module->startLabel);

		file.write(module->labelAddr, module->labelCount);
		file.write(module->instr, module->instrCount);
		file.writeBuffer(module->globalVar, sizeof(TFE_Value), module->globalVarCount);
		file.writeBuffer(module->args, sizeof(TFE_Value), module->argCount);

		file.close();
		return true;
	}

	TFE_Module* loadModule(const char* filePath)
	{
		FileStream file;
		if (!file.open(filePath, FileStream::MODE_READ))
		{
			return nullptr;
		}
		char hdr[4];
		file.readBuffer(hdr, 4);
		if (hdr[0] != 'T' || hdr[1] != 'F' || hdr[2] != 'S' || hdr[3] != 0)
		{
			file.close();
			return nullptr;
		}
		u32 version;
		file.read(&version);
		if (version != c_version)
		{
			return nullptr;
		}

		TFE_Module* module = (TFE_Module*)malloc(sizeof(TFE_Module));
		if (!module)
		{
			return nullptr;
		}
		memset(module, 0, sizeof(TFE_Module));
		module->compiled = true;
		module->init = true;

		file.readBuffer(module->name, 32);
		file.read(&module->labelCount);
		file.read(&module->globalVarCount);
		file.read(&module->instrCount);
		file.read(&module->argCount);
		file.read(&module->startLabel);

		module->labelAddr = (u32*)malloc(sizeof(u32) * module->labelCount);
		module->instr     = (u32*)malloc(sizeof(u32) * module->instrCount);
		module->globalVar = (TFE_Value*)malloc(sizeof(TFE_Value) * module->globalVarCount);
		module->args      = (TFE_Value*)malloc(sizeof(TFE_Value) * module->argCount);

		if ((!module->labelAddr && module->labelCount) || (!module->instr && module->instrCount) || (!module->globalVar && module->globalVarCount) || 
			(!module->args && module->argCount))
		{
			file.close();
			freeModule(module);
			return nullptr;
		}

		file.read(module->labelAddr, module->labelCount);
		file.read(module->instr, module->instrCount);
		file.readBuffer(module->globalVar, sizeof(TFE_Value), module->globalVarCount);
		file.readBuffer(module->args, sizeof(TFE_Value), module->argCount);
		file.close();
		
		return module;
	}

	void freeModule(TFE_Module* module)
	{
		free(module->globalVar);
		free(module->labelAddr);
		free(module->instr);
		free(module->args);
		free(module);
	}
	   
	u32 getLabelIndex(const char* label)
	{
		u32 index = INVALID_LABEL;
		for (u32 i = 0; i < s_curModule->labelCount; i++)
		{
			if (s_labels[i].name[0] == label[0] && strcmp(s_labels[i].name, label) == 0)
			{
				index = i;
				break;
			}
		}
		return index;
	}

	u32 getGlobalIndex(const char* name)
	{
		u32 index = INVALID_LABEL;
		for (u32 i = 0; i < s_curModule->globalVarCount; i++)
		{
			if (s_globals[i].name[0] == name[0] && strcmp(s_globals[i].name, name) == 0)
			{
				index = i;
				break;
			}
		}
		return index;
	}

	void vmError(const char* error, ...)
	{
		static char _msgStr[1024];

		//Handle the variable input, "printf" style messages
		va_list arg;
		va_start(arg, error);
		vsprintf(_msgStr, error, arg);
		va_end(arg);

		TFE_System::logWrite(LOG_ERROR, s_curModule->name, "Script Runtime Error: %s", _msgStr);
		s_curModule->runtimeErrorCount++;
	}

	void addLabel(const char* label)
	{
		if (getLabelIndex(label) != INVALID_LABEL)
		{
			vmError("Duplicate label '%s'", label);
			return;
		}
		u32 index = s_curModule->labelCount;
		s_curModule->labelCount++;

		strcpy(s_labels[index].name, label);
		s_curModule->labelAddr = (u32*)realloc(s_curModule->labelAddr, sizeof(u32)*s_curModule->labelCount);
		s_curModule->labelAddr[index] = s_curModule->instrCount;

		if (strcasecmp(label, "start") == 0)
		{
			s_curModule->startLabel = index;
		}
	}
	
	void addGlobal(const char* name, TFE_Value value)
	{
		if (getGlobalIndex(name) != INVALID_GLOBAL)
		{
			vmError("Duplicate global name '%s'", name);
			return;
		}
		u32 index = s_curModule->globalVarCount;
		s_curModule->globalVarCount++;

		strcpy(s_globals[index].name, name);
		s_curModule->globalVar = (TFE_Value*)realloc(s_curModule->globalVar, sizeof(TFE_Value)*s_curModule->globalVarCount);
		s_curModule->globalVar[index] = value;
	}

	void addInstruction(TFE_Opcode opcode, TFE_Value arg0, TFE_Value arg1, TFE_Value arg2)
	{
		u32 index = s_curModule->instrCount;
		s_curModule->instrCount++;

		s_curModule->instr = (u32*)realloc(s_curModule->instr, sizeof(u32)*s_curModule->instrCount);
		s_curModule->instr[index]  = opcode;
		s_curModule->instr[index] |= (s_curModule->argCount << 8);

		u32 argCount = c_opcodeArgCount[opcode];
		if (argCount)
		{
			u32 index = s_curModule->argCount;
			s_curModule->argCount += argCount;
			s_curModule->args = (TFE_Value*)realloc(s_curModule->args, sizeof(TFE_Value)*s_curModule->argCount);

			s_curModule->args[index] = arg0;
			if (argCount > 1)
			{
				s_curModule->args[index + 1] = arg1;
			}
			if (argCount > 2)
			{
				s_curModule->args[index + 2] = arg2;
			}
		}
	}

	bool isNumber(TFE_Value arg)
	{
		if (arg.type == VTYPE_INT || arg.type == VTYPE_FLOAT)
		{
			return true;
		}
		else if (arg.type == VTYPE_REG)
		{
			TFE_Value* reg = &s_curModule->reg[arg.data.u32];
			return reg->type == VTYPE_INT || reg->type == VTYPE_FLOAT;
		}
		else if (arg.type == VTYPE_GLOBAL)
		{
			TFE_Value* global = &s_curModule->globalVar[arg.data.u32];
			return global->type == VTYPE_INT || global->type == VTYPE_FLOAT;
		}
		return false;
	}

	s32 getInt(TFE_Value arg)
	{
		if (arg.type == VTYPE_INT) { return arg.data.i32; }
		else if (arg.type == VTYPE_FLOAT) { return (s32)arg.data.f32; }
		else if (arg.type == VTYPE_REG)
		{ 
			TFE_Value* reg = &s_curModule->reg[arg.data.u32];
			if (reg->type == VTYPE_INT) { return reg->data.i32; }
			if (reg->type == VTYPE_FLOAT) { return (s32)reg->data.f32; }
		}
		else if (arg.type == VTYPE_GLOBAL)
		{
			TFE_Value* global = &s_curModule->globalVar[arg.data.u32];
			if (global->type == VTYPE_INT) { return global->data.i32; }
			if (global->type == VTYPE_FLOAT) { return (s32)global->data.f32; }
		}
		return 0;
	}

	f32 getFloat(TFE_Value arg)
	{
		if (arg.type == VTYPE_INT) { return (f32)arg.data.i32; }
		else if (arg.type == VTYPE_FLOAT) { return arg.data.f32; }
		else if (arg.type == VTYPE_REG)
		{
			TFE_Value* reg = &s_curModule->reg[arg.data.u32];
			if (reg->type == VTYPE_INT) { return (f32)reg->data.i32; }
			if (reg->type == VTYPE_FLOAT) { return reg->data.f32; }
		}
		else if (arg.type == VTYPE_GLOBAL)
		{
			TFE_Value* global = &s_curModule->globalVar[arg.data.u32];
			if (global->type == VTYPE_INT) { return (f32)global->data.i32; }
			if (global->type == VTYPE_FLOAT) { return global->data.f32; }
		}
		return 0.0f;
	}

	f32 readArgAsFloat(TFE_Value arg)
	{
		if (isNumber(arg))
		{
			return getFloat(arg);
		}
		vmError("Cannot read argument as float.");
		return 0.0f;
	}

	s32 readArgAsInt(TFE_Value arg)
	{
		if (isNumber(arg))
		{
			return getInt(arg);
		}
		vmError("Cannot read argument as int.");
		return 0;
	}

	TFE_Value getGlobalValue(TFE_Module* module, s32 index)
	{
		return module->globalVar[index];
	}
	
	#define CMP_CLEAR_FLAGS() reg[REG_FL].data.u32 = 0;
	#define CMP_SET_FLAGS(v0, v1) reg[REG_FL].data.u32 = u32(v0 == v1) | (u32(v0 > v1) << 1)

	#define MATH_OP(op)	\
	if (isNumber(args[1]) && isNumber(args[2])) \
	{ \
		TFE_Value* output = nullptr; \
		if (args[0].type == VTYPE_REG) \
		{ \
			output = &reg[args[0].data.u32]; \
		} \
		else if (args[0].type == VTYPE_GLOBAL) \
		{ \
			output = &s_curModule->globalVar[args[0].data.u32]; \
		} \
		if (output) \
		{ \
			if (args[1].type == VTYPE_FLOAT || args[2].type == VTYPE_FLOAT) /*Promote to float if either input is float.*/ \
			{ \
				f32 value = readArgAsFloat(args[1]) op readArgAsFloat(args[2]); \
				*output = argImmFloat(value); \
			} \
			else /*Otherwise it is an int.*/ \
			{ \
				s32 value = readArgAsInt(args[1]) op readArgAsInt(args[2]); \
				*output = argImmInt(value); \
			} \
		} \
		else \
		{ \
			vmError("Invalid binary math operation output, it needs to write to a register or global."); \
		} \
	} \
	else \
	{ \
		vmError("Invalid binary math operation, both input values need to be numbers."); \
	}

	#define BINARY_OP(op) \
	if (isNumber(args[1]) && isNumber(args[2])) \
	{ \
		TFE_Value* output = nullptr; \
		if (args[0].type == VTYPE_REG) \
		{ \
			output = &reg[args[0].data.u32]; \
		} \
		else if (args[0].type == VTYPE_GLOBAL) \
		{ \
			output = &s_curModule->globalVar[args[0].data.u32]; \
		} \
		if (output) \
		{ \
			s32 value = readArgAsInt(args[1]) op readArgAsInt(args[2]); \
			*output = argImmInt(value); \
		} \
		else \
		{ \
			vmError("Invalid binary operation output, it needs to write to a register or global."); \
		} \
	} \
	else \
	{ \
		vmError("Invalid binary operation, both input values need to be numbers."); \
	}

	void vm_push(TFE_Value value)
	{
		u32 sp = s_curModule->reg[REG_SP].data.u32;
		s_curModule->reg[REG_SP].data.u32++;

		if (value.type == VTYPE_REG)
		{
			TFE_Value* reg = &s_curModule->reg[value.data.u32];
			s_curModule->stack[sp] = *reg;
		}
		else if (value.type == VTYPE_GLOBAL)
		{
			TFE_Value* global = &s_curModule->globalVar[value.data.u32];
			s_curModule->stack[sp] = *global;
		}
		else
		{
			s_curModule->stack[sp] = value;
		}
	}

	void vm_pop(TFE_Value* output)
	{
		u32* sp = &s_curModule->reg[REG_SP].data.u32;
		if (*sp > 0)
		{
			(*sp)--;
			TFE_Value* value = &s_curModule->stack[*sp];

			if (output->type == VTYPE_REG)
			{
				TFE_Value* reg = &s_curModule->reg[output->data.u32];
				*reg = s_curModule->stack[*sp];
			}
			else if (output->type == VTYPE_GLOBAL)
			{
				TFE_Value* global = &s_curModule->globalVar[output->data.u32];
				*global = s_curModule->stack[*sp];
			}
			else
			{
				*output = s_curModule->stack[*sp];
			}
		}
	}

	bool runModule(TFE_Module* module)
	{
		s_curModule = module;
		TFE_Value* reg = module->reg;
		TFE_Value* stack = module->stack;
		u32* callStack = module->callStack;

		u32* ip = &reg[REG_IP].data.u32;
		u32* cs = &reg[REG_CS].data.u32;
		u32* instr = module->instr;
		if (module->init)
		{
			module->init = false;
			module->runtimeErrorCount = 0;
			*ip = module->labelAddr[module->startLabel];
			*cs = 0;
		}
		
		bool runModule = true;
		bool finished = true;
		while (runModule)
		{
			u32* pc = &instr[*ip];
			u32 opcode    = (*pc) & 0xff;
			u32 argOffset = (*pc) >> 8u;
			TFE_Value* args = &module->args[argOffset];
			(*ip)++;

			switch (opcode)
			{
				case OP_NOP:
				{
				} break;
				case OP_MOV:
				{
					TFE_Value* output = nullptr;
					if (args[0].type == VTYPE_REG)
					{
						output = &reg[args[0].data.u32];
					}
					else if (args[0].type == VTYPE_GLOBAL)
					{
						output = &s_curModule->globalVar[args[0].data.u32];
					}

					if (output)
					{
						TFE_Value* src = &args[1];
						if (args[1].type == VTYPE_REG)
						{
							src = &reg[args[1].data.u32];
						}
						else if (args[1].type == VTYPE_GLOBAL)
						{
							src = &s_curModule->globalVar[args[1].data.u32];
						}
						*output = *src;
					}
					else
					{
						vmError("Cannot copy value, wrong type - %d, it must be a register or variable", args[0].type);
					}
				} break;
				case OP_PUSH:
				{
					vm_push(args[0]);
				} break;
				case OP_POP:
				{
					vm_pop(&args[0]);
				} break;
				case OP_CALL:
				{
					// Push the next instruction as the return value.
					callStack[*cs] = *ip;
					(*cs)++;

					// Then set the instruction pointer.
					*ip = args[0].data.u32;
				} break;
				case OP_RET:
				{
					if (*cs > 0)
					{
						(*cs)--;
						*ip = callStack[*cs];
					}
					else
					{
						vmError("Invalid return - the call stack is empty.");
					}
				} break;
				case OP_YIELD:
				{
					u32 delayInTicks = args[0].data.u32;

					// Leave the IP as-is and just return control.
					runModule = false;
					finished = false;
				} break;
				case OP_INC:
				{
					TFE_Value* output = nullptr;
					if (args[0].type == VTYPE_REG)
					{
						output = &reg[args[0].data.u32];
					}
					else if (args[0].type == VTYPE_GLOBAL)
					{
						output = &s_curModule->globalVar[args[0].data.u32];
					}

					if (output && isNumber(*output))
					{
						if (output->type == VTYPE_FLOAT)
						{
							output->data.f32 += 1.0f;
						}
						else
						{
							output->data.i32++;
						}
					}
					else
					{
						vmError("Cannot increment non-numerical value, wrong type - %d.", args[0].type);
					}
				} break;
				case OP_DEC:
				{
					TFE_Value* output = nullptr;
					if (args[0].type == VTYPE_REG)
					{
						output = &reg[args[0].data.u32];
					}
					else if (args[0].type == VTYPE_GLOBAL)
					{
						output = &s_curModule->globalVar[args[0].data.u32];
					}

					if (output && isNumber(*output))
					{
						if (output->type == VTYPE_FLOAT)
						{
							output->data.f32 -= 1.0f;
						}
						else
						{
							output->data.i32--;
						}
					}
					else
					{
						vmError("Cannot decrement non-numerical value, wrong type - %d.", args[0].type);
					}
				} break;
				case OP_ADD:
				{
					MATH_OP(+);
				} break;
				case OP_SUB:
				{
					MATH_OP(-);
				} break;
				case OP_MUL:
				{
					MATH_OP(*);
				} break;
				case OP_DIV:
				{
					MATH_OP(/);
				} break;
				case OP_MOD:
				{
					if (isNumber(args[1]) && isNumber(args[2]))
					{
						TFE_Value* output = nullptr;
						if (args[0].type == VTYPE_REG)
						{
							output = &reg[args[0].data.u32];
						}
						else if (args[0].type == VTYPE_GLOBAL)
						{
							output = &s_curModule->globalVar[args[0].data.u32];
						}
						if (output)
						{
							if (args[1].type == VTYPE_FLOAT || args[2].type == VTYPE_FLOAT)	// Promote to float if either argument is a float.
							{
								f32 value = fmodf(readArgAsFloat(args[1]), readArgAsFloat(args[2]));
								*output = argImmFloat(value);
							}
							else
							{
								s32 value = readArgAsInt(args[1]) % readArgAsInt(args[2]);
								*output = argImmInt(value);
							}
						}
						else
						{
							vmError("Invalid binary math operation output, it needs to write to a register or global."); \
						}
					}
					else
					{
						vmError("Invalid binary math operation, both input values need to be numbers.");
					}
				} break;
				case OP_NOT:
				{
					if (isNumber(args[1]))
					{
						TFE_Value* output = nullptr;
						if (args[0].type == VTYPE_REG)
						{
							output = &reg[args[0].data.u32];
						}
						else if (args[0].type == VTYPE_GLOBAL)
						{
							output = &s_curModule->globalVar[args[0].data.u32];
						}
						if (output)
						{
							*output = argImmInt(~readArgAsInt(args[1]));
						}
						else
						{
							vmError("Cannot apply binary NOT (~) to value, wrong type - %d, it must be an integer type.", args[0].type);
						}
					}
					else
					{
						vmError("The NOT (~) operation can only be applied to an integer.");
					}
				} break;
				case OP_XOR:
				{
					BINARY_OP(^);
				} break;
				case OP_OR:
				{
					BINARY_OP(|);
				} break;
				case OP_AND:
				{
					BINARY_OP(&);
				} break;
				case OP_SHL:
				{
					BINARY_OP(<<);
				} break;
				case OP_SHR:
				{
					BINARY_OP(>>);
				} break;
				case OP_CMP:
				{
					TFE_Value* arg0 = &args[0];
					TFE_Value* arg1 = &args[1];
					if (arg0->type == VTYPE_REG)
					{
						arg0 = &reg[arg0->data.u32];
					}
					else if (arg0->type == VTYPE_GLOBAL)
					{
						arg0 = &module->globalVar[arg0->data.u32];
					}
					if (arg1->type == VTYPE_REG)
					{
						arg1 = &reg[arg1->data.u32];
					}
					else if (arg1->type == VTYPE_GLOBAL)
					{
						arg1 = &module->globalVar[arg1->data.u32];
					}

					// For now we only support numerical comparisons.
					if (isNumber(*arg0) && isNumber(*arg1))
					{
						if (arg0->type == VTYPE_FLOAT || arg1->type == VTYPE_FLOAT)
						{
							f32 value0 = readArgAsFloat(*arg0);
							f32 value1 = readArgAsFloat(*arg1);
							CMP_SET_FLAGS(value0, value1);
						}
						else
						{
							s32 value0 = readArgAsInt(*arg0);
							s32 value1 = readArgAsInt(*arg1);
							CMP_SET_FLAGS(value0, value1);
						}
					}
					else
					{
						CMP_CLEAR_FLAGS();
					}
				} break;
				case OP_JMP:
				{
					*ip = args[0].data.u32;
				} break;
				case OP_JE:
				{
					*ip = (reg[REG_FL].data.u32 & 0x01) ? args[0].data.u32 : *ip;
				} break;
				case OP_JNE:
				{
					*ip = (!(reg[REG_FL].data.u32 & 0x01)) ? args[0].data.u32 : *ip;
				} break;
				case OP_JG:
				{
					*ip = (reg[REG_FL].data.u32 & 0x02) ? args[0].data.u32 : *ip;
				} break;
				case OP_JGE:
				{
					*ip = (reg[REG_FL].data.u32 & 0x03) ? args[0].data.u32 : *ip;
				} break;
				case OP_JL:
				{
					*ip = (!(reg[REG_FL].data.u32 & 0x03)) ? args[0].data.u32 : *ip;
				} break;
				case OP_JLE:
				{
					*ip = (!(reg[REG_FL].data.u32 & 0x02)) ? args[0].data.u32 : *ip;
				} break;
				case OP_PRN:
				{
					TFE_Value* output = &args[0];
					if (args[0].type == VTYPE_REG)
					{
						output = &reg[args[0].data.u32];
					}
					else if (args[0].type == VTYPE_GLOBAL)
					{
						output = &s_curModule->globalVar[args[0].data.u32];
					}

					if (output->type == VTYPE_FLOAT)
					{
						TFE_System::logWrite(LOG_MSG, module->name, "%f", output->data.f32);
					}
					else if (output->type == VTYPE_INT)
					{
						TFE_System::logWrite(LOG_MSG, module->name, "%d", output->data.i32);
					}
					else
					{
						TFE_System::logWrite(LOG_MSG, module->name, "%u", output->data.u32);
					}
				} break;
				case OP_END:
				{
					runModule = false;
					finished = true;
				} break;
			}

			if (module->runtimeErrorCount)
			{
				TFE_System::logWrite(LOG_ERROR, module->name, "Encountered %d runtime errors in script '%s', aborting", module->runtimeErrorCount, module->name);
				module->init = true;
				return true;
			}
		}
		return finished;
	}
};