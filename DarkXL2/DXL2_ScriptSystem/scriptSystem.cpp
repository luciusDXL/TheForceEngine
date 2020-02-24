#include "scriptSystem.h"
#include <scriptstdstring/scriptstdstring.h>
#include <scriptbuilder/scriptbuilder.h>
#include <DXL2_System/system.h>
#include <DXL2_FileSystem/filestream.h>
#include <DXL2_FileSystem/paths.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

namespace DXL2_ScriptSystem
{
	static const char* c_scriptDir[] =
	{
		"Logics",	// SCRIPT_TYPE_LOGIC = 0,
		"Levels",	// SCRIPT_TYPE_LEVEL,
		"Weapons",	// SCRIPT_TYPE_WEAPON,
		"UI",		// SCRIPT_TYPE_UI,
		"Inf",		// SCRIPT_TYPE_INF,
		"Test",		// SCRIPT_TYPE_TEST,
	};

	asIScriptEngine* s_engine;
	asIScriptContext* s_context[SCRIPT_TYPE_COUNT];

	void messageCallback(const asSMessageInfo *msg, void *param);

	// Print the script string to the standard output stream
	void DXL2_Print(std::string& msg)
	{
		DXL2_System::logWrite(LOG_MSG, "Scripting", "%s", msg.c_str());
	}
	
	void DXL2_PrintInt(std::string& msg, s32 value0)
	{
		DXL2_System::logWrite(LOG_MSG, "Scripting", "%s %d", msg.c_str(), value0);
	}

	void DXL2_PrintUint(std::string& msg, u32 value0)
	{
		DXL2_System::logWrite(LOG_MSG, "Scripting", "%s %u", msg.c_str(), value0);
	}

	void DXL2_PrintFloat(std::string& msg, f32 value0)
	{
		DXL2_System::logWrite(LOG_MSG, "Scripting", "%s %f", msg.c_str(), value0);
	}

	bool init()
	{
		// Create the script engine
		s_engine = asCreateScriptEngine();
		if (!s_engine)
		{
			DXL2_System::logWrite(LOG_ERROR, "Scripting", "Failed to create scripting engine.");
			return false;
		}

		// Set the message callback to receive information on errors in human readable form.
		s32 res = s_engine->SetMessageCallback(asFUNCTION(messageCallback), 0, asCALL_CDECL);
		assert(res >= 0);

		// AngelScript doesn't have a built-in string type, as there is no definite standard 
		// string type for C++ applications. Every developer is free to register its own string type.
		// The SDK do however provide a standard add-on for registering a string type, so it's not
		// necessary to implement the registration yourself if you don't want to.
		RegisterStdString(s_engine);

		// Register the function that we want the scripts to call 
		res = s_engine->RegisterGlobalFunction("void DXL2_Print(const string &in)", asFUNCTION(DXL2_Print), asCALL_CDECL); assert(res >= 0);
		res = s_engine->RegisterGlobalFunction("void DXL2_Print(const string &in, int value)", asFUNCTION(DXL2_PrintInt), asCALL_CDECL); assert(res >= 0);
		res = s_engine->RegisterGlobalFunction("void DXL2_Print(const string &in, uint value)", asFUNCTION(DXL2_PrintUint), asCALL_CDECL); assert(res >= 0);
		res = s_engine->RegisterGlobalFunction("void DXL2_Print(const string &in, float value)", asFUNCTION(DXL2_PrintFloat), asCALL_CDECL); assert(res >= 0);
		
		// Create contexts, one for each script type.
		for (u32 i = 0; i < SCRIPT_TYPE_COUNT; i++)
		{
			s_context[i] = s_engine->CreateContext();
		}

		return true;
	}

	void shutdown()
	{
		for (u32 i = 0; i < SCRIPT_TYPE_COUNT; i++)
		{
			s_context[i]->Release();
		}

		s_engine->ShutDownAndRelease();
		s_engine = nullptr;
	}

	void registerFunction(const char* declaration, const ScriptFuncPtr& funcPtr)
	{
		s32 res = s_engine->RegisterGlobalFunction(declaration, funcPtr, asCALL_CDECL); assert(res >= 0);
	}

	void registerValueType(const char* name, size_t typeSize, u32 typeTraits, std::vector<ScriptTypeProp>& prop)
	{
		// Register a primitive type, that doesn't need any special management of the content
		s32 res = s_engine->RegisterObjectType(name, (s32)typeSize, asOBJ_VALUE | asOBJ_POD | typeTraits); assert(res >= 0);
		const size_t propCount = prop.size();
		for (size_t i = 0; i < propCount; i++)
		{
			res = s_engine->RegisterObjectProperty(name, prop[i].name.c_str(), (s32)prop[i].offset); assert(res >= 0);
		}
	}

	void registerRefType(const char* name, std::vector<ScriptTypeProp>& prop)
	{
		// Register a primitive type, that doesn't need any special management of the content
		s32 res = s_engine->RegisterObjectType(name, 0, asOBJ_REF | asOBJ_NOCOUNT); assert(res >= 0);
		const size_t propCount = prop.size();
		for (size_t i = 0; i < propCount; i++)
		{
			res = s_engine->RegisterObjectProperty(name, prop[i].name.c_str(), (s32)prop[i].offset); assert(res >= 0);
		}
	}

	void registerGlobalProperty(const char* name, void* ptr)
	{
		s32 res = s_engine->RegisterGlobalProperty(name, ptr); assert(res >= 0);
	}

	void registerEnumType(const char* name)
	{
		s_engine->RegisterEnum(name);
	}
	
	void registerEnumValue(const char* enumName, const char* valueName, s32 value)
	{
		s_engine->RegisterEnumValue(enumName, valueName, value);
	}

	bool loadScriptModule(ScriptType type, const char* moduleName)
	{
		if (s_engine->GetModule(moduleName))
		{
			return true;
		}

		const char* programDir = DXL2_Paths::getPath(PATH_PROGRAM);
		char path[DXL2_MAX_PATH];
		sprintf(path, "%sScripts/%s/%s.dxs", programDir, c_scriptDir[type], moduleName);

		CScriptBuilder builder;
		s32 res = builder.StartNewModule(s_engine, moduleName);
		if (res < 0)
		{
			DXL2_System::logWrite(LOG_ERROR, "Scripting", "Unrecoverable error while starting a new module.");
			return false;
		}
		res = builder.AddSectionFromFile(path);
		if (res < 0)
		{
			// The builder wasn't able to load the file. Maybe the file
			// has been removed, or the wrong name was given, or some
			// preprocessing commands are incorrectly written.
			DXL2_System::logWrite(LOG_ERROR, "Scripting", "Cannot open or parse file.");
			return false;
		}
		res = builder.BuildModule();
		if (res < 0)
		{
			// An error occurred. Instruct the script writer to fix the 
			// compilation errors that were listed in the output stream.
			DXL2_System::logWrite(LOG_ERROR, "Scripting", "Compile error(s).");
			return false;
		}
		return true;
	}

	void* getScriptFunction(const char* moduleName, const char* funcDecl)
	{
		// Find the function that is to be called. 
		asIScriptModule* module = s_engine->GetModule(moduleName);
		return module->GetFunctionByDecl(funcDecl);
	}

	void executeScriptFunction(ScriptType type, void* funcPtr)
	{
		if (!funcPtr) { return; }

		asIScriptFunction* func = (asIScriptFunction*)funcPtr;
		s_context[type]->Prepare(func);
		const s32 res = s_context[type]->Execute();
		if (res != asEXECUTION_FINISHED)
		{
			// The execution didn't complete as expected. Determine what happened.
			if (res == asEXECUTION_EXCEPTION)
			{
				// An exception occurred, let the script writer know what happened so it can be corrected.
				DXL2_System::logWrite(LOG_ERROR, "Scripting", "An exception '%s' occurred. Please correct the code and try again.\n", s_context[type]->GetExceptionString());
			}
		}
	}

	void executeScriptFunction(ScriptType type, void* funcPtr, s32 arg0, s32 arg1, s32 arg2)
	{
		if (!funcPtr) { return; }

		asIScriptFunction* func = (asIScriptFunction*)funcPtr;
		s_context[type]->Prepare(func);
		s_context[type]->SetArgDWord(0, arg0);
		s_context[type]->SetArgDWord(1, arg1);
		s_context[type]->SetArgDWord(2, arg2);
		const s32 res = s_context[type]->Execute();
		if (res != asEXECUTION_FINISHED)
		{
			// The execution didn't complete as expected. Determine what happened.
			if (res == asEXECUTION_EXCEPTION)
			{
				// An exception occurred, let the script writer know what happened so it can be corrected.
				DXL2_System::logWrite(LOG_ERROR, "Scripting", "An exception '%s' occurred. Please correct the code and try again.\n", s_context[type]->GetExceptionString());
			}
		}
	}

	void update()
	{
	}

	//////////////////////////
	//
	// Implement a simple message callback function
	void messageCallback(const asSMessageInfo *msg, void *param)
	{
		LogWriteType type = LOG_ERROR;
		if (msg->type == asMSGTYPE_WARNING)
			type = LOG_WARNING;
		else if (msg->type == asMSGTYPE_INFORMATION)
			type = LOG_MSG;

		DXL2_System::logWrite(type, "Scripting", "%s (%d, %d) : %s\n", msg->section, msg->row, msg->col, msg->message);
	}
}
