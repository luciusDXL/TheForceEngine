#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
// This is unfortunate.
#include "angelscript.h"
#include <vector>
#include <string>

typedef asSFuncPtr ScriptFuncPtr;
#define SCRIPT_FUNCTION asFUNCTION
#define SCRIPT_FUNC_PTR void*

enum ScriptType
{
	SCRIPT_TYPE_LOGIC = 0,
	SCRIPT_TYPE_LEVEL,
	SCRIPT_TYPE_WEAPON,
	SCRIPT_TYPE_UI,
	SCRIPT_TYPE_INF,
	SCRIPT_TYPE_TEST,
	SCRIPT_TYPE_COUNT
};

struct ScriptTypeProp
{
	std::string name;
	size_t offset;
};

namespace DXL2_ScriptSystem
{
	bool init();
	void shutdown();

	bool loadScriptModule(ScriptType type, const char* moduleName);
	void* getScriptFunction(const char* moduleName, const char* funcDecl);
	void executeScriptFunction(ScriptType type, void* funcPtr);
	void executeScriptFunction(ScriptType type, void* funcPtr, s32 arg0, s32 arg1, s32 arg2);

	// This is required to be a template, so must be included here in the header.
	// This also means that "angelscript" must be included more broadly than I would like.
	template <typename T>
	u32 getTypeTraits() { return asGetTypeTraits<T>(); }

	void registerValueType(const char* name, size_t typeSize, u32 typeTraits, std::vector<ScriptTypeProp>& prop);
	void registerRefType(const char* name, std::vector<ScriptTypeProp>& prop);
	void registerGlobalProperty(const char* name, void* ptr);
	void registerEnumType(const char* name);
	void registerEnumValue(const char* enumName, const char* valueName, s32 value);

	void registerFunction(const char* declaration, const ScriptFuncPtr& funcPtr);
	
	void update();
}
