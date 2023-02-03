#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Game Message
// This handles HUD messages, local messages, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>

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
		s32 count = 0;
		GameMessage* msgList = nullptr;
	};

	void gameMessage_freeBuffer();
	s32 parseMessageFile(GameMessages* messages, const FilePath* path, s32 mode);
	GameMessage* getGameMessage(GameMessages* messages, s32 msgId);
}  // TFE_DarkForces