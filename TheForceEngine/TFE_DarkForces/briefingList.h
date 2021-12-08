#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Game Message
// This handles HUD messages, local messages, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>

namespace TFE_DarkForces
{
	enum { BRIEF_STR_LEN = 13 };

	struct BriefingInfo
	{
		char mission[BRIEF_STR_LEN];
		char archive[BRIEF_STR_LEN];
		char bgAnim[BRIEF_STR_LEN];
		char palette[BRIEF_STR_LEN];
	};
	struct BriefingList
	{
		s32 count;
		BriefingInfo* briefing;
	};

	void briefingList_freeBuffer();
	s32 parseBriefingList(BriefingList* briefingList, const char* filename);
}  // TFE_DarkForces