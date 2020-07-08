#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
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

// Enums and Global properties.
#define SCRIPT_ENUM(type)				TFE_ScriptSystem::registerEnumType(#type);
#define SCRIPT_ENUM_VALUE(type, value)	TFE_ScriptSystem::registerEnumValue(#type, #value, value);
#define SCRIPT_GLOBAL_PROPERTY(type, name, ptr) TFE_ScriptSystem::registerGlobalProperty(#type " @" #name, &ptr);
#define SCRIPT_GLOBAL_PROPERTY_CONST(type, name, ptr) TFE_ScriptSystem::registerGlobalProperty("const " #type " " #name, &ptr);

// Structure definitions available to scripts.
#define SCRIPT_STRUCT_START TFE_ScriptSystem::s_prop.clear();
#define SCRIPT_STRUCT_MEMBER(str, type, name) TFE_ScriptSystem::s_prop.push_back({ #type " " #name, offsetof(str, name) });
#define SCRIPT_STRUCT_MEMBER_CONST(str, type, name) TFE_ScriptSystem::s_prop.push_back({ "const " #type " " #name, offsetof(str, name) });
#define SCRIPT_STRUCT_VALUE(str) TFE_ScriptSystem::registerValueType(#str, sizeof(str), TFE_ScriptSystem::getTypeTraits<str>(), TFE_ScriptSystem::s_prop);
#define SCRIPT_STRUCT_REF(str) TFE_ScriptSystem::registerRefType(#str, TFE_ScriptSystem::s_prop);

// Native Functions
#define SCRIPT_NATIVE_FUNC(name, decl) TFE_ScriptSystem::registerFunction(#decl, SCRIPT_FUNCTION(name))

namespace TFE_ScriptSystem
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

	extern std::vector<ScriptTypeProp> s_prop;
}
