#include "forceScript.h"
#include <TFE_System/system.h>
#include <TFE_FrontEndUI/frontEndUI.h>
#include <cstring>
#include <algorithm>
#include <assert.h>

#ifdef ENABLE_FORCE_SCRIPT
#define FS_TEST_MEMORY_LOAD 1
// TFE_ForceScript wraps Anglescript, so these includes should only exist here.
#include <angelscript.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptbuilder/scriptbuilder.h>

namespace TFE_ForceScript
{
	const asPWORD TaskMgrId = 1002;

	static asIScriptEngine*  s_engine  = nullptr;
	static asIScriptContext* s_context = nullptr;

	struct ScriptThread
	{
		asIScriptContext* asContext;
		f32 delay;
	};
	std::vector<ScriptThread> s_scriptThreads;
	std::vector<s32> s_freeThreads;

	void test();

	// Script message callback.
	void messageCallback(const asSMessageInfo* msg, void* param)
	{
		LogWriteType type = LOG_ERROR;
		if (msg->type == asMSGTYPE_WARNING) { type = LOG_WARNING; }
		else if (msg->type == asMSGTYPE_INFORMATION) { type = LOG_MSG; }
		TFE_System::logWrite(type, "Script", "%s (%d, %d) : %s", msg->section, msg->row, msg->col, msg->message);
	}

	// Print the script string to the standard output stream
	void print(std::string& msg)
	{
		TFE_FrontEndUI::logToConsole(msg.c_str());
	}

	void yield(f32 delay)
	{
		asIScriptContext* context = asGetActiveContext();
		if (context)
		{
			const s32 id = (s32)context->GetUserData(TaskMgrId);
			assert(id >= 0 && id < (s32)s_scriptThreads.size());
			s_scriptThreads[id].delay = delay;
			context->Suspend();
		}
	}

	void resume(s32 id)
	{
		s_scriptThreads[id].delay = 0.0f;
	}

	void update()
	{
		const f32 dt = (f32)TFE_System::getDeltaTime();
		const s32 count = (s32)s_scriptThreads.size();
		ScriptThread* thread = s_scriptThreads.data();
		for (s32 i = 0; i < count; i++)
		{
			// Allow for holes to keep IDs consistent.
			// Fill holes with new threads.
			if (thread[i].asContext == nullptr) { continue; }

			thread[i].delay = std::max(0.0f, s_scriptThreads[i].delay - dt);
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

	void init()
	{
		// Create the script engine.
		s_engine = asCreateScriptEngine();

		// Set the message callback to receive information on errors in human readable form.
		s32 res = s_engine->SetMessageCallback(asFUNCTION(messageCallback), 0, asCALL_CDECL);
		assert(res >= 0);

		// Register std::string as the script string type.
		RegisterStdString(s_engine);

		// Register a test function.
		res = s_engine->RegisterGlobalFunction("void system_print(const string &in)", asFUNCTION(print), asCALL_CDECL); assert(res >= 0);
		res = s_engine->RegisterGlobalFunction("void system_yield(float)", asFUNCTION(yield), asCALL_CDECL); assert(res >= 0);
		
		s_context = s_engine->CreateContext();
		test();
	}

	void destroy()
	{
		// Clean up
		if (s_context)
		{
			s_context->Release();
			s_context = nullptr;
		}
		if (s_engine)
		{
			s_engine->ShutDownAndRelease();
			s_engine = nullptr;
		}
	}

	ModuleHandle createModule(const char* moduleName, const char* filePath)
	{
		CScriptBuilder builder;
		s32 res;
		res = builder.StartNewModule(s_engine, moduleName);
		if (res < 0)
		{
			return nullptr;
		}
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
					
	ModuleHandle createModule(const char* moduleName, const char* sectionName, const char* srcCode)
	{
		CScriptBuilder builder;
		s32 res;
		res = builder.StartNewModule(s_engine, moduleName);
		if (res < 0)
		{
			return nullptr;
		}
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

	FunctionHandle findScriptFunc(ModuleHandle modHandle, const char* funcName)
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

		asIScriptFunction* func = mod->GetFunctionByDecl(funcName);
		if (!func)
		{
			// The function couldn't be found. Instruct the script writer
			// to include the expected function in the script.
			TFE_System::logWrite(LOG_ERROR, "Script", "The script must have the function 'void main()'. Please add it and try again.\n");
			return nullptr;
		}
		return func;
	}

	// Add a script function to be executed during the update.
	void execFunc(FunctionHandle funcHandle)
	{
		if (!funcHandle) { return; }
		asIScriptFunction* func = (asIScriptFunction*)funcHandle;

		// Get a free context.
		asIScriptContext* context = s_engine->RequestContext();
		if (!context)
		{
			// ERROR
			return;
		}
		// Then prepare it for execution.
		s32 res = context->Prepare(func);
		if (res < 0)
		{
			s_engine->ReturnContext(context);
			return;
		}
		// Get the new thread ID and prepare it.
		// The function will be executed during the next update.
		s32 id = -1;
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
		context->SetUserData((void*)id, TaskMgrId);
	}

	void test()
	{
		// Build a test...
	#if FS_TEST_MEMORY_LOAD == 1
		const char* c_testScript =
			"void main()\n"
			"{\n"
			"	system_print(\"Hello world\");\n"
			"}\n";

		ModuleHandle mod = createModule("Test", "Test.as", c_testScript);
	#else
		ModuleHandle mod = createModule("Test", "Scripts/Test/Test.fs");
	#endif
		if (mod)
		{
			FunctionHandle func = findScriptFunc(mod, "void main()");
			execFunc(func);
		}
	}
}  // TFE_ForceScript

#else

namespace TFE_ForceScript
{
	void init() {} 
	void destroy() {}
}

#endif