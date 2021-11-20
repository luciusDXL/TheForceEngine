#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Game Message
// This handles HUD messages, local messages, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>

namespace TFE_DarkForces
{
	struct ListItem
	{
		char archive[14];
		char scene[10];
		s16 id;
		s16 nextId;
		s16 skip;
		s16 speed;
		s16 music;
		s16 volume;
	};

	void gameList_freeBuffer();
	ListItem* gameList_load(const char* filename);
}  // TFE_DarkForces