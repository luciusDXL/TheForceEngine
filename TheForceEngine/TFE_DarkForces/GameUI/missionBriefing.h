#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Mission Briefing
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_DarkForces/Landru/lrect.h>

namespace TFE_DarkForces
{
	enum MissionBriefingConstants
	{
		BRIEF_VERT_MARGIN = 0,
		BRIEF_LINE_SCROLL = 12,
		BRIEF_PAGE_SCROLL = BRIEF_LINE_SCROLL * 10,
	};

	void  missionBriefing_start(const char* archive, const char* bgAnim, const char* mission, const char* palette, s32 skill);
	void  missionBriefing_cleanup();
	JBool missionBriefing_update(s32* skill, JBool* abort);

	extern s16 s_briefY;
	extern s32 s_briefingMaxY;
	extern LRect s_overlayRect;
}
