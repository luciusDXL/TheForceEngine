#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <string>

class asIScriptEngine;

enum ScriptAPI
{
	API_SHARED = 1,
	API_LEVEL_EDITOR = 2,
	API_SYSTEM_UI = 4,
	API_GAME = 8,
	API_UNKNOWN = 65536,
};

extern const char* s_enumType;
extern const char* s_typeName;
extern const char* s_objVar;

//Helpers
#define xstr(s) str(s)
#define str(s) #s

#define ScriptClassBegin(typeName, typeInst, api) \
s32 res = 0; \
asIScriptEngine* engine = (asIScriptEngine*)TFE_ForceScript::getEngine(); \
ScriptSetAPI(api); \
ScriptObjectType(typeName); \
ScriptObjectVariable(typeInst)

#define ScriptClassEnd() ScriptClass(std::string(s_typeName + std::string(" ") + s_objVar).c_str()); return res >= 0

#define ScriptSetAPI(api) engine->SetDefaultAccessMask(api);
#define ScriptCurType std::remove_reference<decltype(*this)>::type
#define ScriptEnumRegister(type) s_enumType = type; res = engine->RegisterEnum(type); assert(res >= 0)
#define ScriptEnum(name, value) res = engine->RegisterEnumValue(s_enumType, name, value); assert(res >= 0)
#define ScriptEnumStr(value) res = engine->RegisterEnumValue(s_enumType, xstr(value), value); assert(res >= 0)

// objType in C++
// objName in script
// function declaration in script
// function name
// Example: ScriptObjMethod(LS_Level, "Level", "string getSlot()", getSlot);
//                      <C++ objType> <objName> <function decl>    <C++ member function>
#define ScriptObjMethod(decl, funcName) res = engine->RegisterObjectMethod(s_typeName, decl, asMETHOD(ScriptCurType, funcName), asCALL_THISCALL);  assert(res >= 0);
#define ScriptObjFunc(decl, funcName) res = engine->RegisterObjectMethod(s_typeName, decl, asFUNCTION(funcName), asCALL_CDECL_OBJLAST);  assert(res >= 0);

#define ScriptGenericMethod(decl, funcName) res = engine->RegisterObjectMethod(s_typeName, decl, asFUNCTION(funcName), asCALL_GENERIC); assert(res >= 0)

#define ScriptPropertyGet(decl, funcName) res = engine->RegisterObjectMethod(s_typeName, std::string(decl + std::string(" const property")).c_str(), asMETHOD(ScriptCurType, funcName), asCALL_THISCALL); assert(res >= 0);
#define ScriptPropertySet(decl, funcName) res = engine->RegisterObjectMethod(s_typeName, std::string(decl + std::string(" property")).c_str(), asMETHOD(ScriptCurType, funcName), asCALL_THISCALL); assert(res >= 0);

#define ScriptPropertyGetFunc(decl, funcName) res = engine->RegisterObjectMethod(s_typeName, std::string(decl + std::string(" const property")).c_str(), asFUNCTION(funcName), asCALL_CDECL_OBJLAST);  assert(res >= 0);
#define ScriptPropertySetFunc(decl, funcName) res = engine->RegisterObjectMethod(s_typeName, std::string(decl + std::string(" property")).c_str(), asFUNCTION(funcName), asCALL_CDECL_OBJLAST);  assert(res >= 0);

#define ScriptLambdaPropertyGet(decl, ret, lambda) res = engine->RegisterObjectMethod(s_typeName, std::string(decl + std::string(" const property")).c_str(), asFUNCTIONPR([]() lambda, (), ret), asCALL_CDECL_OBJLAST);  assert(res >= 0)
#define ScriptLambdaPropertySet(decl, args, lambda) res = engine->RegisterObjectMethod(s_typeName, std::string(decl + std::string(" property")).c_str(), asFUNCTIONPR([]args lambda, args, void), asCALL_CDECL_OBJLAST);  assert(res >= 0)

#define ScriptMemberVariable(decl, var) res = engine->RegisterObjectProperty(s_typeName, decl, asOFFSET(ScriptCurType, var)); assert(res >= 0)

// objName in script
// function declaration in script
// args = named arguments, example: (std::string& name, f32 value)
// returnType = return type, example: void
// lambda = {funcBody;}
// Example, setup in the class "Level" with variable "level":
//     ScriptLambdaMethod("void setName(const string &in)", (std::string& name), void, { s_level.name = name; });
//                        <script declaration>              <arguments>, <return value> <function body>
// Called in script as: level.setName("Level Name");
#define ScriptLambdaMethod(decl, args, ret, lambda) res = engine->RegisterObjectMethod(s_typeName, decl, asFUNCTIONPR([]args lambda, args, ret), asCALL_CDECL_OBJLAST);  assert(res >= 0)

#define ScriptValueType(name) s_typeName = name; res = engine->RegisterObjectType(name, sizeof(ScriptCurType), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<ScriptCurType>()); assert(res >= 0)
#define ScriptObjectType(objName) s_typeName = objName; res = engine->RegisterObjectType(objName, sizeof(ScriptCurType), asOBJ_VALUE | asOBJ_POD); assert(res >= 0)
#define ScriptObjectVariable(objVar) s_objVar = objVar

#define ScriptClass(decl) res = engine->RegisterGlobalProperty(decl, this);  assert(res >= 0)
#define ScriptResult res >= 0
