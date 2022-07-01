#pragma once

#include <TFE_System/types.h>
#include "vmConfig.h"

#ifdef VM_ENABLE
namespace TFE_ForceScript
{
	// Note the values are designed so that, for basic types, they can be directly cast to pointers
	// and used as-is.
	// Pointers can be used by masking the lower 48-bits and casting.
	// More complex types, such as intNxM and floatNxM and arrays are stored as baseType + count and then data is packed
	// in additional u64 "words".

	// Values are stored as:
	// [Value : 48-bits] | [Type : 4-bits] | [TypeMod : 4-bits] | [Flags : 8-bits]
	// Value can be: int, float, pointer, packed characters, data size for arrays or complex types.
	// Type is FS_Type (see below).
	// TypeMod is N (2-bits) + M (2-bits) for vector/matrix types, or number of arguments for functions for quick testing.
	// Flags is FS_Flags (see below).

	enum FS_Type
	{
		FST_NULL = 0,
		// Basic types.
		FST_INT,
		FST_FLOAT,
		// Packed types.
		FST_PACKED_STRING,
		FST_INLINE_STRING,
		// Complex types.
		FST_INT_NxM,
		FST_FLOAT_NxM,
		// Composite types.
		FST_TABLE,
		FST_STRUCT,
		FST_STRING,
		FST_TUPLE,
		// Always implemented as a reference (pointer).
		FST_FUNC,
		FST_COUNT
	};

	enum FS_Flags
	{
		FSF_NONE  = 0,
		FSF_ARRAY = FLAG_BIT(0),		// Array of FS_Type, Value is the array size.
		FSF_REF   = FLAG_BIT(1),		// Pointer to global memory, Value is the pointer.
		FSF_STATIC_TYPE = FLAG_BIT(2),	// The value is statically typed.
	};

	#define FS_TypeDataShift 48ull
	#define FS_TypeModShift  52ull
	#define FS_TypeMask 15ull
	#define FS_ValueMask ((1ull << 48ull) - 1ull)

	typedef u64 FsValue;
	#define fsValue_getIntPtr(v)     ((s32*)&v)
	#define fsValue_getFloatPtr(v)   ((f32*)&v)
	#define fsValue_getVoidPointer(v) (void*)(v&FS_ValueMask)
	#define fsValue_getValueData(v) (v>>FS_TypeDataShift)
	#define fsValue_getType(v) FS_Type((v>>FS_TypeDataShift) & FS_TypeMask)
	#define fsValue_getTypeMod(v) u32((v>>FS_TypeModShift) & FS_TypeMask)
	
	inline FsValue fsValue_createNull()
	{
		return FsValue(u64(FST_NULL) << FS_TypeDataShift);
	}

	inline FsValue fsValue_createInt(s32 i)
	{
		u64 valueData = FST_INT;
		return u64(*((u32*)&i)) | (valueData << FS_TypeDataShift);
	}

	inline FsValue fsValue_createFloat(f32 f)
	{
		u64 valueData = FST_FLOAT;
		return u64(*((u32*)&f)) | (valueData << FS_TypeDataShift);
	}

	inline s32 fsValue_readAsInt(const FsValue* const v)
	{
		const FS_Type type = fsValue_getType(*v);
		const u32 typeMod = fsValue_getTypeMod(*v); // intNxM and floatNxM values are packed in the next slot(s); stored as y | x; w | z
		if (type == FST_INT)
		{
			return typeMod ? *((s32*)(v+1)) : *((s32*)v);
		}
		else if (type == FST_FLOAT)
		{
			return typeMod ? s32(*((f32*)(v+1))) : s32(*((f32*)v));
		}
		return 0;	// Invalid.
	}

	inline f32 fsValue_readAsFloat(const FsValue* const v)
	{
		const FS_Type type = fsValue_getType(*v);
		const u32 typeMod = fsValue_getTypeMod(*v);
		if (type == FST_FLOAT)
		{
			return typeMod ? *((f32*)(v+1)) : *((f32*)v);
		}
		else if (type == FST_INT)
		{
			return typeMod ? f32(*((s32*)(v+1))) : f32(*((s32*)v));
		}
		return 0.0f;	// Invalid.
	}

	void fsValue_toString(FsValue value, char* outStr);
}
#endif