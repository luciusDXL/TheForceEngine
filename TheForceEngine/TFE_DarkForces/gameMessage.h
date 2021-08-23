#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Game Message
// This handles HUD messages, local messages, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

namespace TFE_DarkForces
{
	struct GameMessage
	{
		s32 id;
		char* text;
		s32 priority;
	};
	struct GameMessages
	{
		s32 count;
		GameMessage* msgList;
	};

	s32 parseMessageFile(GameMessages* messages, const char* fileName, s32 mode);
}  // TFE_DarkForces