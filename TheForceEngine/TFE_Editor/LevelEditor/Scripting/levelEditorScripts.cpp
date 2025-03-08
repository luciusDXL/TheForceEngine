#include "levelEditorScripts.h"
#include "ls_draw.h"
#include "ls_selection.h"
#include "ls_system.h"
#include "ls_level.h"
#include <TFE_System/math.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_ForceScript/scriptInterface.h>
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

using namespace TFE_Editor;
using namespace TFE_Jedi;
using namespace TFE_ForceScript;

namespace LevelEditor
{
	static char s_scriptBuffer[4096];

	bool s_levelScriptRegistered = false;
	bool s_execFromOutput = false;
	LS_Draw s_lsDraw;
	LS_Selection s_lsSelection;
	LS_System s_lsSystem;
	LS_Level s_lsLevel;

	// Print any script messages, warnings or errors to the editor output.
	void scriptCallback(LogWriteType type, const char* section, s32 row, s32 col, const char* msg)
	{
		if (s_execFromOutput)
		{
			if (type == LOG_ERROR || type == LOG_CRITICAL)
			{
				infoPanelAddMsg(LE_MSG_ERROR, "%s", msg);
			}
		}
		else
		{
			const LeMsgType msgType[] =
			{
				LE_MSG_INFO,    //LOG_MSG
				LE_MSG_WARNING, //LOG_WARNING
				LE_MSG_ERROR,   //LOG_ERROR
				LE_MSG_ERROR,   //LOG_CRITICAL
			};
			infoPanelAddMsg(msgType[type], "%s %d, %d: %s", section, row, col, msg);
		}
	}
		
	void registerScriptFunctions(ScriptAPI api)
	{
		if (s_levelScriptRegistered) { return; }
		s_levelScriptRegistered = true;
		TFE_ForceScript::overrideCallback(scriptCallback);
		
		s_lsDraw.scriptRegister(api);
		s_lsSelection.scriptRegister(api);
		s_lsSystem.scriptRegister(api);
		s_lsLevel.scriptRegister(api);

		// Math/Intrinsics.
	}

	void executeLine(const char* line)
	{
		s_execFromOutput = true;

		// Short-cuts to make running script commands on the console easier.
		strcpy(s_scriptBuffer, "#include \"EditorDef/Scripts/scriptConsole.h\"");
		// Wrap the console line in a function.
		// Add an extra ';' at the end so it isn't required when typing in the console.
		// (If one is typed, you get ;; which will still compile fine)
		strcat(s_scriptBuffer, "void main()\n{\n");
		strcat(s_scriptBuffer, line);
		strcat(s_scriptBuffer, ";}\n");

		TFE_ForceScript::ModuleHandle lineMod = TFE_ForceScript::createModule("LineMod", "LineMod", s_scriptBuffer, API_SHARED | API_LEVEL_EDITOR);
		if (lineMod)
		{
			TFE_ForceScript::FunctionHandle func = TFE_ForceScript::findScriptFuncByName(lineMod, "main");
			TFE_ForceScript::execFunc(func);
		}

		s_execFromOutput = false;
	}

	void showLevelScript(const char* scriptName)
	{
		char scriptPath[TFE_MAX_PATH];
		sprintf(scriptPath, "EditorDef/Scripts/%s.fs", scriptName);
		FileStream file;
		if (file.open(scriptPath, Stream::MODE_READ))
		{
			std::vector<char> text;
			const size_t size = file.getSize();
			text.resize(size + 1);
			file.readBuffer(text.data(), (u32)size);
			text.data()[size] = 0;
			file.close();

			char title[256];
			sprintf(title, "Script - %s", scriptName);
			TFE_Editor::showMessageBox(title, "%s", text.data());
		}
	}
}
