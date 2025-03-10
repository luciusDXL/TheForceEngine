#include "gameScripts.h"
#include "gs_system.h"
#include "gs_level.h"
#include "gs_game.h"
#include "gs_player.h"
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

using namespace TFE_Jedi;
using namespace TFE_ForceScript;

namespace TFE_DarkForces
{
	bool s_gameScriptRegistered = false;

	GS_System s_gsSystem;
	GS_Level s_gsLevel;
	GS_Game s_gsGame;
	GS_Player s_gsPlayer;

	// Print any script messages, warnings or errors to the editor output.
	void scriptCallback(LogWriteType type, const char* section, s32 row, s32 col, const char* msg)
	{
		TFE_System::logWrite(type, "Dark Forces Game Script", "%s (%d, %d) : %s", section, row, col, msg);
	}
		
	void registerScriptFunctions(ScriptAPI api)
	{
		if (s_gameScriptRegistered) { return; }
		s_gameScriptRegistered = true;
		TFE_ForceScript::overrideCallback(scriptCallback);
		
		s_gsSystem.scriptRegister(api);
		s_gsLevel.scriptRegister(api);
		s_gsGame.scriptRegister(api);
		s_gsPlayer.scriptRegister(api);
	}
}
