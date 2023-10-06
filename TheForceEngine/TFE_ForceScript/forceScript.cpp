#include "forceScript.h"
#include <TFE_System/system.h>
#include <TFE_FrontEndUI/frontEndUI.h>
#include <assert.h>

#ifdef ENABLE_FORCE_SCRIPT
// TFE_ForceScript wraps Anglescript, so these includes should only exist here.
#include <angelscript.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptbuilder/scriptbuilder.h>

namespace TFE_ForceScript
{
	static asIScriptEngine*  s_engine  = nullptr;
	static asIScriptContext* s_context = nullptr;

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
		res = s_engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(print), asCALL_CDECL);
		assert(res >= 0);

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

	bool createModule(const char* moduleName, const char* sectionName, const char* srcCode)
	{
		CScriptBuilder builder;
		s32 res;
		res = builder.StartNewModule(s_engine, moduleName);
		if (res < 0)
		{
			return false;
		}
		res = builder.AddSectionFromMemory(sectionName, srcCode);
		if (res < 0)
		{
			return false;
		}
		res = builder.BuildModule();
		if (res < 0)
		{
			return false;
		}
		return true;
	}

	asIScriptFunction* findScriptFunc(const char* moduleName, const char* funcName)
	{
		// Find the function that is to be called. 
		asIScriptModule* mod = s_engine->GetModule(moduleName);
		if (!mod)
		{
			// The function couldn't be found. Instruct the script writer
			// to include the expected function in the script.
			TFE_System::logWrite(LOG_ERROR, "Script", "Cannot find module '%s'.\n", moduleName);
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

	void runScriptFunc(asIScriptFunction* func)
	{
		if (!func) { return; }

		// Create our context, prepare it, and then execute
		s_context->Prepare(func);
		s32 res = s_context->Execute();
		if (res != asEXECUTION_FINISHED)
		{
			// The execution didn't complete as expected. Determine what happened.
			if (res == asEXECUTION_EXCEPTION)
			{
				// An exception occurred, let the script writer know what happened so it can be corrected.
				TFE_System::logWrite(LOG_ERROR, "Script", "An exception '%s' occurred. Please correct the code and try again.\n", s_context->GetExceptionString());
			}
		}
	}

	void test()
	{
		// Build a test...
		const char* c_testScript =
			"void main()\n"
			"{\n"
			"	print(\"Hello world\");\n"
			"}\n";

		if (createModule("Test", "Test.as", c_testScript))
		{
			asIScriptFunction* func = findScriptFunc("Test", "void main()");
			runScriptFunc(func);
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