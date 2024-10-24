#pragma once
//////////////////////////////////////////////////////////////////////
// Constants linked to custom HUD messages used by The Force Engine.
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include "system.h"

enum TFE_Message
{
	TFE_MSG_SAVE = 0,
	TFE_MSG_SECRET,
	TFE_MSG_FLYMODE,
	TFE_MSG_NOCLIP,
	TFE_MSG_TESTER,
	TFE_MSG_ADDLIFE,
	TFE_MSG_ADDLIFEFAIL,
	TFE_MSG_SUBLIFE,
	TFE_MSG_CAT,
	TFE_MSG_DIE,
	TFE_MSG_ONEHITKILL,
	TFE_MSG_HARDCORE,
	TFE_MSG_FULLBRIGHT,
	TFE_MSG_HD,
	TFE_MSG_COUNT
};

namespace TFE_System
{
	const char* getMessage(TFE_Message msg);
	bool loadMessages(const char* path);
	void freeMessages();
}