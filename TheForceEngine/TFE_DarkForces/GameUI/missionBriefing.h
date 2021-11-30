#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Mission Briefing
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

namespace TFE_DarkForces
{
	void  missionBriefing_start(const char* archive, const char* bgAnim, const char* mission, const char* palette, s32 skill);
	JBool missionBriefing_update(s32* skill, JBool* abort);
}
