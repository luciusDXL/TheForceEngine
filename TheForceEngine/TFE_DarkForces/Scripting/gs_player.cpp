#include "gs_player.h"
#include "assert.h"
#include <TFE_DarkForces/player.h>
#include <TFE_System/system.h>
#include <angelscript.h>

namespace TFE_DarkForces
{
	void GS_Player::setPlayerHealth(s32 health)
	{
		s_playerInfo.health = health;
	}

	bool GS_Player::scriptRegister(ScriptAPI api)
	{
		ScriptClassBegin("Player", "player", api);
		{
			// Functions
			ScriptObjMethod("void setPlayerHealth(int)", setPlayerHealth);
			
			// Getters
			ScriptLambdaPropertyGet("int get_health()", s32, { return s_playerInfo.health; });
		}
		ScriptClassEnd();
	}
}