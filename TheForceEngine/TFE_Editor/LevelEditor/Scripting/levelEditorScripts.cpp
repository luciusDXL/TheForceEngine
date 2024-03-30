#include "levelEditorScripts.h"
#include "levelEditScript_system.h"
#include "levelEditScript_level.h"
#include <TFE_System/math.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_ForceScript/forceScript.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

#ifdef ENABLE_FORCE_SCRIPT
#include <angelscript.h>

using namespace TFE_Editor;
using namespace TFE_Jedi;

namespace LevelEditor
{
	void registerScriptFunctions(void* engine_)
	{
		asIScriptEngine* engine = (asIScriptEngine*)engine_;

		s32 res = 0;
		// System API.
		// void yield(float delay);
		res = engine->RegisterGlobalFunction("void system_print(const string &in)", asFUNCTION(system_print), asCALL_CDECL); assert(res >= 0);
		res = engine->RegisterGlobalFunction("void system_printWarning(const string &in)", asFUNCTION(system_printWarning), asCALL_CDECL); assert(res >= 0);
		res = engine->RegisterGlobalFunction("void system_printError(const string &in)", asFUNCTION(system_printError), asCALL_CDECL); assert(res >= 0);

		// Math/Intrinsics.
		// Register float2, float3, float4, ...

		// Level API.
		res = engine->RegisterGlobalFunction("int level_getSectorCount(void)", asFUNCTION(level_getSectorCount), asCALL_CDECL); assert(res >= 0);
		res = engine->RegisterGlobalFunction("int level_getObjectCount(void)", asFUNCTION(level_getObjectCount), asCALL_CDECL); assert(res >= 0);
	}
}

#else

namespace LevelEditor
{
	void registerScriptFunctions(void* engine)
	{
	}
}

#endif