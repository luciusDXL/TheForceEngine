#include "forceScript.h"
#include "float2.h"
#include <TFE_System/system.h>
#include <TFE_FrontEndUI/frontEndUi.h>
#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <assert.h>

#ifdef ENABLE_FORCE_SCRIPT

// TFE_ForceScript wraps Anglescript, so these includes should only exist here.
#include <angelscript.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptarray/scriptarray.h>
#include <scriptbuilder/scriptbuilder.h>

namespace TFE_ForceScript
{
	const asPWORD ThreadId = 1002;
	
	struct ScriptThread
	{
		asIScriptContext* asContext;
		f32 delay;
	};

	static asIScriptEngine* s_engine = nullptr;
	static std::vector<ScriptThread> s_scriptThreads;
	static std::vector<s32> s_freeThreads;
	ScriptMessageCallback s_msgCallback = nullptr;

	static s32 s_typeId[FSTYPE_COUNT] = { 0 };

	// Script message callback.
	void messageCallback(const asSMessageInfo* msg, void* param)
	{
		LogWriteType type = LOG_ERROR;
		if (msg->type == asMSGTYPE_WARNING) { type = LOG_WARNING; }
		else if (msg->type == asMSGTYPE_INFORMATION) { type = LOG_MSG; }

		if (s_msgCallback)
		{
			s_msgCallback(type, msg->section, msg->row, msg->col, msg->message);
			return;
		}
		TFE_System::logWrite(type, "Script", "%s (%d, %d) : %s", msg->section, msg->row, msg->col, msg->message);
	}
		
	void yield(f32 delay)
	{
		asIScriptContext* context = asGetActiveContext();
		if (context)
		{
			const s32 id = (s32)((intptr_t)context->GetUserData(ThreadId));
			assert(id >= 0 && id < (s32)s_scriptThreads.size());
			s_scriptThreads[id].delay = delay;
			context->Suspend();
		}
	}

	void resume(s32 id)
	{
		s_scriptThreads[id].delay = 0.0f;
	}

	s32 getObjectTypeId(FS_BuiltInType type)
	{
		return s_typeId[type];
	}
	
	void init()
	{
		// Create the script engine.
		s_engine = asCreateScriptEngine();

		// Set the message callback to receive information on errors in human readable form.
		s32 res = s_engine->SetMessageCallback(asFUNCTION(messageCallback), 0, asCALL_CDECL);
		assert(res >= 0);

		// Register std::string as the script string type.
		RegisterStdString(s_engine);
		s_typeId[FSTYPE_STRING] = GetStdStringObjectId();

		// Register the default array type.
		RegisterScriptArray(s_engine, true);
		s_typeId[FSTYPE_ARRAY] = GetScriptArrayObjectId();

		// Language features.
		res = s_engine->RegisterGlobalFunction("void yield(float = 0.0)", asFUNCTION(yield), asCALL_CDECL); assert(res >= 0);

		registerScriptMath_float2(s_engine);
		s_typeId[FSTYPE_FLOAT2] = getFloat2ObjectId();
	}

	void destroy()
	{
		// Clean up
		stopAllFunc();
		if (s_engine)
		{
			s_engine->ShutDownAndRelease();
			s_engine = nullptr;
		}
	}

	void overrideCallback(ScriptMessageCallback callback)
	{
		s_msgCallback = callback;
	}

	void update(f32 dt)
	{
		if (dt == 0.0f) { dt = (f32)TFE_System::getDeltaTime(); }
		const s32 count = (s32)s_scriptThreads.size();
		ScriptThread* thread = s_scriptThreads.data();
		for (s32 i = 0; i < count; i++)
		{
			// Allow for holes to keep IDs consistent.
			// Fill holes with new threads.
			if (thread[i].asContext == nullptr) { continue; }

			thread[i].delay = std::max(0.0f, thread[i].delay - dt);
			if (thread[i].delay == 0.0f)
			{
				asIScriptContext* context = thread[i].asContext;
				s32 res = context->Execute();
				if (res != asEXECUTION_SUSPENDED)
				{
					// Finally done!
					s_engine->ReturnContext(context);
					thread[i].asContext = nullptr;
					thread[i].delay = 0.0f;
					s_freeThreads.push_back(i);
				}
			}
		}
	}

	void stopAllFunc()
	{
		if (s_engine)
		{
			const s32 count = (s32)s_scriptThreads.size();
			ScriptThread* thread = s_scriptThreads.data();

			for (s32 i = 0; i < count; i++)
			{
				if (thread[i].asContext == nullptr) { continue; }

				asIScriptContext* context = thread[i].asContext;
				context->Abort();
				s_engine->ReturnContext(context);
			}
		}
		// Take this opportunity to defrag.
		s_scriptThreads.clear();
		s_freeThreads.clear();
	}

	void* getEngine()
	{
		return s_engine;
	}

	ModuleHandle getModule(const char* moduleName)
	{
		return s_engine->GetModule(moduleName);
	}

	void deleteModule(const char* moduleName)
	{
		asIScriptModule* mod = s_engine->GetModule(moduleName);
		if (!mod) { return; }
		mod->Discard();
	}
		
	ModuleHandle createModule(const char* moduleName, const char* filePath, bool allowReadFromArchive, u32 accessMask)
	{
		CScriptBuilder builder;
		builder.SetReadMode(!allowReadFromArchive); // true to read from disk, false to read from the TFE filesystem.
		s32 res = builder.StartNewModule(s_engine, moduleName);
		if (res < 0)
		{
			return nullptr;
		}
		// Set Access Mask
		asIScriptModule* mod = builder.GetModule();
		if (!mod)
		{
			return nullptr;
		}
		mod->SetAccessMask(accessMask);
		// Load the source and build.
		res = builder.AddSectionFromFile(filePath);
		if (res < 0)
		{
			return nullptr;
		}
		res = builder.BuildModule();
		if (res < 0)
		{
			return nullptr;
		}
		return builder.GetModule();
	}
					
	ModuleHandle createModule(const char* moduleName, const char* sectionName, const char* srcCode, u32 accessMask)
	{
		CScriptBuilder builder;
		s32 res = builder.StartNewModule(s_engine, moduleName);
		if (res < 0)
		{
			return nullptr;
		}
		// Set Access Mask
		asIScriptModule* mod = builder.GetModule();
		if (!mod)
		{
			return nullptr;
		}
		mod->SetAccessMask(accessMask);
		// Add the source and build.
		res = builder.AddSectionFromMemory(sectionName, srcCode);
		if (res < 0)
		{
			return nullptr;
		}
		res = builder.BuildModule();
		if (res < 0)
		{
			return nullptr;
		}
		return builder.GetModule();
	}

	FunctionHandle findScriptFuncByDecl(ModuleHandle modHandle, const char* funcDecl)
	{
		if (!modHandle) { return nullptr; }

		// Find the function that is to be called. 
		asIScriptModule* mod = (asIScriptModule*)modHandle;
		if (!mod)
		{
			// The function couldn't be found. Instruct the script writer
			// to include the expected function in the script.
			TFE_System::logWrite(LOG_ERROR, "Script", "Cannot find module '%s'.\n", mod->GetName());
			return nullptr;
		}

		asIScriptFunction* func = mod->GetFunctionByDecl(funcDecl);
		if (!func)
		{
			// The function couldn't be found. Instruct the script writer
			// to include the expected function in the script.
			TFE_System::logWrite(LOG_ERROR, "Script", "The script must have the function with declaration '%s'. Please add it and try again.\n", funcDecl);
			return nullptr;
		}
		return func;
	}

	FunctionHandle findScriptFuncByName(ModuleHandle modHandle, const char* funcName)
	{
		if (!modHandle) { return nullptr; }

		// Find the function that is to be called. 
		asIScriptModule* mod = (asIScriptModule*)modHandle;
		if (!mod)
		{
			// The function couldn't be found. Instruct the script writer
			// to include the expected function in the script.
			TFE_System::logWrite(LOG_ERROR, "Script", "Cannot find module '%s'.\n", mod->GetName());
			return nullptr;
		}

		asIScriptFunction* func = mod->GetFunctionByName(funcName);
		if (!func)
		{
			// The function couldn't be found. Instruct the script writer
			// to include the expected function in the script.
			TFE_System::logWrite(LOG_ERROR, "Script", "The script must have the function with name '%s'. Please add it and try again.\n", funcName);
			return nullptr;
		}
		return func;
	}

	FunctionHandle findScriptFuncByNameNoCase(ModuleHandle modHandle, const char* funcName)
	{
		if (!modHandle) { return nullptr; }

		// Find the function that is to be called. 
		asIScriptModule* mod = (asIScriptModule*)modHandle;
		if (!mod)
		{
			// The function couldn't be found. Instruct the script writer
			// to include the expected function in the script.
			TFE_System::logWrite(LOG_ERROR, "Script", "Cannot find module '%s'.\n", mod->GetName());
			return nullptr;
		}

		// Manually search.
		u32 funcCount = mod->GetFunctionCount();
		for (u32 f = 0; f < funcCount; f++)
		{
			asIScriptFunction* func = mod->GetFunctionByIndex(f);
			const char* name = func->GetName();
			if (strcasecmp(name, funcName) == 0)
			{
				return func;
			}
		}

		// The function couldn't be found. Instruct the script writer
		// to include the expected function in the script.
		TFE_System::logWrite(LOG_ERROR, "Script", "The script must have the function with name '%s'. Please add it and try again.\n", funcName);
		return nullptr;
	}
				
	// Add a script function to be executed during the update.
	s32 execFunc(FunctionHandle funcHandle, s32 argCount, const ScriptArg* arg)
	{
		s32 id = -1;
		if (!funcHandle) { return id; }
		asIScriptFunction* func = (asIScriptFunction*)funcHandle;

		// Get a free context.
		asIScriptContext* context = s_engine->RequestContext();
		if (!context)
		{
			// ERROR
			return id;
		}
		// Then prepare it for execution.
		s32 res = context->Prepare(func);
		if (res < 0)
		{
			s_engine->ReturnContext(context);
			return id;
		}
		// Set arguments.
		argCount = std::min(argCount, (s32)func->GetParamCount());
		for (s32 i = 0; i < argCount; i++)
		{
			switch (arg[i].type)
			{
				case ARG_S32:
				{
					context->SetArgDWord(i, *((asDWORD*)&arg[i].iValue));
				} break;
				case ARG_U32:
				{
					context->SetArgDWord(i, arg[i].uValue);
				} break;
				case ARG_F32:
				{
					context->SetArgFloat(i, arg[i].fValue);
				} break;
				case ARG_BOOL:
				{
					context->SetArgByte(i, arg[i].bValue ? 1 : 0);
				} break;
				case ARG_OBJECT:
				{
					context->SetArgObject(i, arg[i].objPtr);
				} break;
				case ARG_STRING:
				{
					context->SetArgObject(i, (void*)&arg[i].stdStr);
				} break;
				case ARG_FLOAT2:
				{
					float2 f2(arg[i].float2Value.x, arg[i].float2Value.z);
					context->SetArgObject(i, (void*)&f2);
				} break;
			};
		}
		// Get the new thread ID and prepare it.
		// The function will be executed during the next update.
		if (!s_freeThreads.empty())
		{
			id = s_freeThreads.back();
			s_freeThreads.pop_back();
		}
		if (id < 0)
		{
			id = (s32)s_scriptThreads.size();
			s_scriptThreads.push_back({});
		}
		s_scriptThreads[id].asContext = context;
		s_scriptThreads[id].delay = 0.0f;
		context->SetUserData((void*)((intptr_t)id), ThreadId);

		return id;
	}
}  // TFE_ForceScript

#else

namespace TFE_ForceScript
{
	// Initialize and destroy script system.
	void init() {}
	void destroy() {}
	void overrideCallback(ScriptMessageCallback callback) {}
	// Run any active script functions.
	void update(f32 dt) {}
	// Stop all running script functions.
	void stopAllFunc() {}

	// Allow other systems to access the underlying engine.
	void* getEngine() { return nullptr; }

	// Compile module.
	ModuleHandle createModule(const char* moduleName, const char* sectionName, const char* srcCode) { return nullptr; }
	ModuleHandle createModule(const char* moduleName, const char* filePath) { return nullptr; }
	// Find a specific script function in a module.
	FunctionHandle findScriptFuncByDecl(ModuleHandle modHandle, const char* funcDecl) { return nullptr; }
	FunctionHandle findScriptFuncByName(ModuleHandle modHandle, const char* funcName) { return nullptr; }

	// Types
	s32 getObjectTypeId(FS_BuiltInType type) { return 0; }

	// Execute a script function.
	// It won't be executed immediately, it will happen on the next TFE_ForceScript update.
	// ---------------------------------------
	// Returns -1 on failure or id on success.
	// Note id is only valid as long as the function is running.
	s32 execFunc(FunctionHandle funcHandle, s32 argCount, const ScriptArg* arg) { return -1; }
	// Resume a suspended script function given by id.
	void resume(s32 id) { return; }
}

#endif