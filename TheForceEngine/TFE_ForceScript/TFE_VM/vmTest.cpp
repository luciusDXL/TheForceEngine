#include "vmTest.h"
#include <assert.h>

using namespace TFE_VM;

TFE_Module* module_testGlobalArithmetic()
{
	TFE_Module* module = TFE_VM::startBuildModule("testGlobalAdd");
	/*
	.global output0, 10
	.global output1, 10
	.global output2, 2
	.global output3, 0
	.global output4, 0.5
	.global output5, 0

	start:
	mov r0, 0
	loop:
		add output0, output0, 2
		sub output1, output1, 2
		mul output2, output2, 2
		add output4, output4, 0.1

		inc r0
		cmp r0, 10
	jl loop
	div output3, output2, 16
	mod output5, output2, 7
	prn output0
	prn output1
	prn output2
	prn output3
	prn output4
	prn output5

	output should = 30, -10, 2048, 128, 1.5, 4
	*/
	TFE_VM::addGlobal("output0", ARG_IMM_INT(10));
	TFE_VM::addGlobal("output1", ARG_IMM_INT(10));
	TFE_VM::addGlobal("output2", ARG_IMM_INT(2));
	TFE_VM::addGlobal("output3", ARG_IMM_INT(0));
	TFE_VM::addGlobal("output4", ARG_IMM_FLOAT(0.5f));
	TFE_VM::addGlobal("output5", ARG_IMM_INT(0));

	TFE_VM::addLabel("start");
	TFE_VM::addInstruction(OP_MOV, ARG_REG(REG_R0), ARG_IMM_INT(0));

	TFE_VM::addLabel("loop");
	TFE_VM::addInstruction(OP_ADD, ARG_GLOBAL("output0"), ARG_GLOBAL("output0"), ARG_IMM_INT(2));
	TFE_VM::addInstruction(OP_SUB, ARG_GLOBAL("output1"), ARG_GLOBAL("output1"), ARG_IMM_INT(2));
	TFE_VM::addInstruction(OP_MUL, ARG_GLOBAL("output2"), ARG_GLOBAL("output2"), ARG_IMM_INT(2));
	TFE_VM::addInstruction(OP_ADD, ARG_GLOBAL("output4"), ARG_GLOBAL("output4"), ARG_IMM_FLOAT(0.1f));
	TFE_VM::addInstruction(OP_INC, ARG_REG(REG_R0));
	TFE_VM::addInstruction(OP_CMP, ARG_REG(REG_R0), ARG_IMM_INT(10));
	TFE_VM::addInstruction(OP_JL, ARG_LABEL("loop"));

	TFE_VM::addInstruction(OP_DIV, ARG_GLOBAL("output3"), ARG_GLOBAL("output2"), ARG_IMM_INT(16));
	TFE_VM::addInstruction(OP_MOD, ARG_GLOBAL("output5"), ARG_GLOBAL("output2"), ARG_IMM_INT(7));

	TFE_VM::addInstruction(OP_PRN, ARG_GLOBAL("output0"));
	TFE_VM::addInstruction(OP_PRN, ARG_GLOBAL("output1"));
	TFE_VM::addInstruction(OP_PRN, ARG_GLOBAL("output2"));
	TFE_VM::addInstruction(OP_PRN, ARG_GLOBAL("output3"));
	TFE_VM::addInstruction(OP_PRN, ARG_GLOBAL("output4"));
	TFE_VM::addInstruction(OP_PRN, ARG_GLOBAL("output5"));
	TFE_VM::addInstruction(OP_END);

	TFE_VM::finishBuildModule();
	return module;
}

TFE_Module* module_testBinaryOps()
{
	TFE_Module* module = TFE_VM::startBuildModule("testBinaryOps");
	/*
	.global output, 0

	start:
		mov r0, 5
		xor r0, r0
		or r0, 7
		and r0, 3
		not output, r0

		output = -4
	*/
	TFE_VM::addGlobal("output", ARG_IMM_INT(0));
	TFE_VM::addLabel("start");
	TFE_VM::addInstruction(OP_MOV, ARG_REG(REG_R0), ARG_IMM_INT(5));
	TFE_VM::addInstruction(OP_XOR, ARG_REG(REG_R0), ARG_REG(REG_R0), ARG_REG(REG_R0));
	TFE_VM::addInstruction(OP_OR, ARG_REG(REG_R0), ARG_REG(REG_R0), ARG_IMM_INT(7));
	TFE_VM::addInstruction(OP_AND, ARG_REG(REG_R0), ARG_REG(REG_R0), ARG_IMM_INT(3));
	TFE_VM::addInstruction(OP_NOT, ARG_GLOBAL("output"), ARG_REG(REG_R0));

	TFE_VM::addInstruction(OP_PRN, ARG_GLOBAL("output"));
	TFE_VM::addInstruction(OP_END);

	TFE_VM::finishBuildModule();
	return module;
}

TFE_Module* module_testStack()
{
	TFE_Module* module = TFE_VM::startBuildModule("testStack");
	/*
	.global output, 10

	start:
		push output // push 10
		add output, output, 3
		push output // push 13
		pop r0 // r0 = 13
		pop r1 // r1 = 10
		sub output, r0, r1

		output = 3
	*/
	TFE_VM::addGlobal("output", ARG_IMM_INT(10));
	TFE_VM::addLabel("start");

	TFE_VM::addInstruction(OP_PUSH, ARG_GLOBAL("output"));
	TFE_VM::addInstruction(OP_ADD, ARG_GLOBAL("output"), ARG_GLOBAL("output"), ARG_IMM_INT(3));
	TFE_VM::addInstruction(OP_PUSH, ARG_GLOBAL("output"));
	TFE_VM::addInstruction(OP_POP, ARG_REG(REG_R0));
	TFE_VM::addInstruction(OP_POP, ARG_REG(REG_R1));
	TFE_VM::addInstruction(OP_SUB, ARG_GLOBAL("output"), ARG_REG(REG_R0), ARG_REG(REG_R1));

	TFE_VM::addInstruction(OP_PRN, ARG_GLOBAL("output"));
	TFE_VM::addInstruction(OP_END);

	TFE_VM::finishBuildModule();
	return module;
}

TFE_Module* module_testFunction()
{
	TFE_Module* module = TFE_VM::startBuildModule("testFunction");
	/*
	.global output, 0

	// assume inputs are r0, r1
	// return is r0
	function:
		add r2, r0, r1
		prn r2
		mov r0, r1
		ret

	start:
		mov r0, 12
		mov r1, 22
		call function
		mov output, r0

		output = 34
	*/
	TFE_VM::addGlobal("output", ARG_IMM_INT(0));

	TFE_VM::addLabel("function");
		TFE_VM::addInstruction(OP_ADD, ARG_REG(REG_R2), ARG_REG(REG_R0), ARG_REG(REG_R1));
		TFE_VM::addInstruction(OP_PRN, ARG_REG(REG_R2));
		TFE_VM::addInstruction(OP_MOV, ARG_REG(REG_R0), ARG_REG(REG_R2));
		TFE_VM::addInstruction(OP_RET);

	TFE_VM::addLabel("start");
		TFE_VM::addInstruction(OP_MOV, ARG_REG(REG_R0), ARG_IMM_INT(12));
		TFE_VM::addInstruction(OP_MOV, ARG_REG(REG_R1), ARG_IMM_INT(22));
		TFE_VM::addInstruction(OP_CALL, ARG_LABEL("function"));
		TFE_VM::addInstruction(OP_MOV, ARG_GLOBAL("output"), ARG_REG(REG_R0));
		TFE_VM::addInstruction(OP_END);

	TFE_VM::finishBuildModule();
	return module;
}

TFE_Module* module_testYield()
{
	TFE_Module* module = TFE_VM::startBuildModule("testYield");
	/*
	.global output, 0

	start:
		mov r0, 0
	loop:
		add output, output, 3
		yield 0
		
		inc r0
		cmp r0, 10
		jl loop

	prn output

		output = 30
	*/
	TFE_VM::addGlobal("output", ARG_IMM_INT(0));

	TFE_VM::addLabel("start");
		TFE_VM::addInstruction(OP_MOV, ARG_REG(REG_R0), ARG_IMM_INT(0));

	TFE_VM::addLabel("loop");
		TFE_VM::addInstruction(OP_ADD, ARG_GLOBAL("output"), ARG_GLOBAL("output"), ARG_IMM_INT(3));
		TFE_VM::addInstruction(OP_YIELD, ARG_IMM_INT(0));

		TFE_VM::addInstruction(OP_INC, ARG_REG(REG_R0));
		TFE_VM::addInstruction(OP_CMP, ARG_REG(REG_R0), ARG_IMM_INT(10));
		TFE_VM::addInstruction(OP_JL, ARG_LABEL("loop"));

	TFE_VM::addInstruction(OP_PRN, ARG_GLOBAL("output"));
	TFE_VM::addInstruction(OP_END);

	TFE_VM::finishBuildModule();
	return module;
}

void vm_test()
{
	TFE_Module* module0 = module_testGlobalArithmetic();
	TFE_Module* module1 = module_testBinaryOps();
	TFE_Module* module2 = module_testStack();
	TFE_Module* module3 = module_testFunction();
	TFE_Module* module4 = module_testYield();

	TFE_VM::serializeModule(module0, "testGlobalAdd.tfs");
	TFE_VM::serializeModule(module1, "testBinaryOps.tfs");
	TFE_VM::serializeModule(module2, "testStack.tfs");
	TFE_VM::serializeModule(module3, "testFunction.tfs");
	TFE_VM::serializeModule(module4, "testYield.tfs");

	TFE_VM::runModule(module0);
	assert(getGlobalValue(module0, 0).data.i32 == 30);
	assert(getGlobalValue(module0, 1).data.i32 == -10);
	assert(getGlobalValue(module0, 2).data.i32 == 2048);
	assert(getGlobalValue(module0, 3).data.i32 == 128);
	assert(fabsf(getGlobalValue(module0, 4).data.f32 - 1.5f)<0.01f);
	assert(getGlobalValue(module0, 5).data.i32 == 4);

	TFE_VM::runModule(module1);
	assert(getGlobalValue(module1, 0).data.i32 == -4);

	TFE_VM::runModule(module2);
	assert(getGlobalValue(module2, 0).data.i32 == 3);

	TFE_VM::runModule(module3);
	assert(getGlobalValue(module3, 0).data.i32 == 34);

	bool finished = false;
	while (!finished)
	{
		finished = TFE_VM::runModule(module4);
	}
	assert(getGlobalValue(module4, 0).data.i32 == 30);

	TFE_VM::freeModule(module0);
	TFE_VM::freeModule(module1);
	TFE_VM::freeModule(module2);
	TFE_VM::freeModule(module3);
	TFE_VM::freeModule(module4);

	// Test serialization.
	module0 = TFE_VM::loadModule("testGlobalAdd.tfs");
	TFE_VM::runModule(module0);
	assert(getGlobalValue(module0, 0).data.i32 == 30);
	assert(getGlobalValue(module0, 1).data.i32 == -10);
	assert(getGlobalValue(module0, 2).data.i32 == 2048);
	assert(getGlobalValue(module0, 3).data.i32 == 128);
	assert(fabsf(getGlobalValue(module0, 4).data.f32 - 1.5f) < 0.01f);
	assert(getGlobalValue(module0, 5).data.i32 == 4);
	TFE_VM::freeModule(module0);
}