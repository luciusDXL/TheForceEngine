#include "gs_game.h"
#include <TFE_System/system.h>
#include <TFE_DarkForces/time.h>

#include <angelscript.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	const f32 c_timeScale = 1.0f / f32(TICKS_PER_SECOND);

	f32 GS_Game::getGameTime()
	{
		return f32(s_curTick) * c_timeScale;
	}

	bool GS_Game::scriptRegister(ScriptAPI api)
	{
		ScriptClassBegin("Game", "game", api);
		{
			// Functions
			ScriptObjMethod("float getGameTime()", getGameTime);
		}
		ScriptClassEnd();
	}
}

