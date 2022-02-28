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

enum TFE_ValueType
{
	VTYPE_INT = 0,		// 32-bit integer.
	VTYPE_FLOAT,		// 32-bit float.
	VTYPE_REG,			// Register (see TFE_Register)
	VTYPE_GLOBAL,		// Global variable (stored as index)
	// Script structs and functions
	VTYPE_OBJ,			// A "object" defined in script.
	VTYPE_FUNC,			// A script function pointer.
	// Native structures and functions.
	VTYPE_C_OBJ,		// A "C" object.
	VTYPE_NATIVE_FUNC,	// A native function pointer.
	VTYPE_COUNT
};

enum TFE_ValueFlags
{
	VFLAG_NONE	= 0,
	VFLAG_ARRAY = FLAG_BIT(0),
	VFLAG_STUB  = FLAG_BIT(1),	// Value flags are only 2-bits, so stub out the second bit so its clear.
};

struct TFE_ValueData
{
	union
	{
		s32 i32;
		u32 u32;
		f32 f32;
	};
};

struct TFE_Value
{
	TFE_ValueData data;	// actual value, index (for global variables or registers), or local pointer.
	struct
	{
		u32 type  : 6;	// type of value.
		u32 flags : 2;	// general flags.
		u32 size  : 24;	// size of value - number of array elements, length of string, etc.
	};
};

struct TFE_VmString
{
	char* data;	// pointer to the string data, stored as a string table.
	u32 length;	// length of the string.
	u32 hash;	// hash value of the string.
};

struct TFE_ScriptStructDef
{
	// Structure data, used to instantiate a new object.
	u32 memCount;		// number of members.
	TFE_Value* members;	// structure members with default values.

	// Debug data.
	TFE_VmString name;
	TFE_VmString* memberNames;
};

struct TFE_ScriptStructInst
{
	u32 defId;			// struture definition id.
	TFE_Value* members;	// members of the structure.
};