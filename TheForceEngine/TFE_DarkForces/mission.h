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
	void mission_startTaskFunc(s32 id);
	void mission_setLoadMissionTask(Task* task);

	extern TextureData* s_loadScreen;
	extern u8 s_loadingScreenPal[];
}  // namespace TFE_DarkForces