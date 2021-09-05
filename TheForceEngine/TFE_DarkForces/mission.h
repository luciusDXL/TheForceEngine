#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Mission Loop
// The main mission loop, including loading and shutdown.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Task/task.h>

namespace TFE_DarkForces
{
	enum GameMissionMode
	{
		MISSION_MODE_LOADING    = 0,	// causes the loading screen to be displayed.
		MISSION_MODE_MAIN       = 1,	// the main in-game experience.
		MISSION_MODE_UNKNOWN    = 2,	// unknown - not set (as far as I can tell).
		MISSION_MODE_LOAD_START = 3,	// set right as loading starts.
	};
		
	void mission_startTaskFunc(s32 id);
	void mission_setLoadMissionTask(Task* task);

	extern GameMissionMode s_missionMode;
	extern TextureData* s_loadScreen;
	extern u8 s_loadingScreenPal[];
	extern u8 s_levelPalette[];
	extern u8 s_basePalette[];
}  // namespace TFE_DarkForces