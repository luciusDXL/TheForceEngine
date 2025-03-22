#include "scriptInterface.h"
#include <TFE_ForceScript/ScriptAPI-Shared/sharedScriptAPI.h>
#include <TFE_Editor/LevelEditor/Scripting/levelEditorScripts.h>
#include <TFE_DarkForces/Scripting/gameScripts.h>
#include <TFE_System/math.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_ForceScript/forceScript.h>
#include <TFE_Editor/LevelEditor/infoPanel.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>
#include <TFE_Ui/ui.h>

#include <algorithm>
#include <vector>
#include <string>
#include <map>
#include <angelscript.h>

// Set to 0 to disable tests on load - this should be done for public builds.
// Set to 1 to run tests - this should be done when making script system changes
// to catch regressions.
#define RUN_SCRIPT_TESTS 0

using namespace TFE_Editor;
using namespace TFE_Jedi;
using namespace TFE_ForceScript;

const char* s_enumType;
const char* s_typeName;
const char* s_objVar;

namespace TFE_ScriptInterface
{
#if RUN_SCRIPT_TESTS == 1
	// Tests to run, they should all exist in `ScriptTests/` relative to the TFE executable/directory.
	const char* c_scriptTests[] =
	{
		"Test_float2",
		"Test_float3",
		"Test_float4",
		"Test_float2x2",
	};
#endif

	struct ScriptToRun
	{
		std::string moduleName;
		std::string funcName;
		s32 argCount;
		ScriptArg args[TFE_MAX_SCRIPT_ARG];
	};

	static std::vector<ScriptToRun> s_scriptsToRun;
	static ScriptAPI s_api = API_UNKNOWN;
	static const char* s_searchPath = nullptr;

	void registerScriptInterface(ScriptAPI api)
	{
		switch (api)
		{
			case API_LEVEL_EDITOR:
			{
				registerSharedScriptAPI(api);
				#if ENABLE_EDITOR == 1
					LevelEditor::registerScriptFunctions(api);
				#endif
			} break;
			case API_SYSTEM_UI:
			{
			} break;
			case API_GAME:
			{
				registerSharedScriptAPI(api);
				TFE_DarkForces::registerScriptFunctions(api);
			} break;
		}
	}

	template<typename... Args>
	void runScript(const char* scriptName, const char* scriptFunc, Args... inputArgs)
	{
		ScriptToRun script;
		script.moduleName = scriptName;
		script.funcName = scriptFunc;
		script.argCount = 0;
		// Unpack the function arguments and store in `script.args`.
		scriptArgIterator(script.argCount, script.args, inputArgs...);

		s_scriptsToRun.push_back(script);
	}

	void runScript(const char* scriptName, const char* scriptFunc)
	{
		s_scriptsToRun.push_back({ scriptName, scriptFunc, 0 });
	}

	bool recompileScript(const char* scriptName)
	{
		// Delete the existing module if it exists.
		TFE_ForceScript::deleteModule(scriptName);

		char scriptPath[TFE_MAX_PATH];
		TFE_ForceScript::ModuleHandle scriptMod = nullptr;
		// Reload and compile the script.
		if (s_searchPath)
		{
			sprintf(scriptPath, "%s/%s.fs", s_searchPath, scriptName);
			scriptMod = TFE_ForceScript::createModule(scriptName, scriptPath, false, s_api);
		}
		// Try loading from the archives and full search paths.
		if (!scriptMod)
		{
			sprintf(scriptPath, "%s.fs", scriptName);
			scriptMod = TFE_ForceScript::createModule(scriptName, scriptPath, true, s_api);
		}
		return scriptMod != nullptr;
	}

	void setAPI(ScriptAPI api, const char* searchPath)
	{
		if (api != API_UNKNOWN)
		{
			s_api = ScriptAPI(api | API_SHARED);
		}
		s_searchPath = searchPath;

		// Automatically run tests if enabled, this will write the results to the console and 
		// Output window in Visual Studio.
	#if RUN_SCRIPT_TESTS == 1
		const size_t testCount = TFE_ARRAYSIZE(c_scriptTests);
		char path[TFE_MAX_PATH];
		for (size_t i = 0; i < testCount; i++)
		{
			const char* name = c_scriptTests[i];
			sprintf(path, "ScriptTests/%s.fs", name);

			TFE_ForceScript::ModuleHandle scriptMod = TFE_ForceScript::getModule(name);
			if (!scriptMod)
			{
				scriptMod = TFE_ForceScript::createModule(name, path, false, s_api);
			}
			if (scriptMod)
			{
				TFE_ForceScript::FunctionHandle func = TFE_ForceScript::findScriptFuncByName(scriptMod, "main");
				if (func)
				{
					TFE_ForceScript::execFunc(func);
				}
			}
		}
	#endif
	}

	void reset()
	{
		TFE_ForceScript::stopAllFunc();
	}
		
	void update()
	{
		if (s_api == API_UNKNOWN) { return; }

		char scriptPath[TFE_MAX_PATH];
		const s32 count = (s32)s_scriptsToRun.size();
		const ScriptToRun* script = s_scriptsToRun.data();
		for (s32 i = 0; i < count; i++, script++)
		{
			// Does the module already exist?
			const char* moduleName = script->moduleName.c_str();
			TFE_ForceScript::ModuleHandle scriptMod = TFE_ForceScript::getModule(moduleName);
			// If not than try to load and compile it.
			if (!scriptMod)
			{
				if (s_searchPath)
				{
					sprintf(scriptPath, "%s/%s.fs", s_searchPath, moduleName);
					scriptMod = TFE_ForceScript::createModule(moduleName, scriptPath, false, s_api);
				}
				// Try loading from the archives and full search paths.
				if (!scriptMod)
				{
					sprintf(scriptPath, "%s.fs", moduleName);
					scriptMod = TFE_ForceScript::createModule(moduleName, scriptPath, true, s_api);

				}
			}
			if (scriptMod)
			{
				TFE_ForceScript::FunctionHandle func = TFE_ForceScript::findScriptFuncByName(scriptMod, script->funcName.c_str());
				TFE_ForceScript::execFunc(func, script->argCount, script->args);
			}
		}
		s_scriptsToRun.clear();
	}
}
