#include "hud.h"
#include <TFE_FileSystem/paths.h>
#include "gameMessage.h"

namespace TFE_DarkForces
{
	fixed16_16 s_flashEffect = 0;
	GameMessages s_hudMessages;
	// Font* s_hudFont;
	char s_hudMessage[80];

	void hud_sendTextMessage(s32 msgId)
	{
		// TODO: Was sendTextMessage()
	}

	void hud_loadGameMessages()
	{
		FilePath filePath;
		if (TFE_Paths::getFilePath("text.msg", &filePath))
		{
			parseMessageFile(&s_hudMessages, &filePath, 1);
		}
		s_hudMessage[0] = 0;
		if (TFE_Paths::getFilePath("glowing.fnt", &filePath))
		{
			// s_hudFont = font_load(filePath);
		}
	}
}  // TFE_DarkForces