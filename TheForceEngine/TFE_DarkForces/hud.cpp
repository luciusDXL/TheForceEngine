#include "hud.h"
#include "time.h"
#include "gameMessage.h"
#include <TFE_FileSystem/paths.h>

namespace TFE_DarkForces
{
	enum HudConstants
	{
		HUD_COLORS_START    = 24,
		HUD_COLORS_COUNT    = 8,
		HUD_MAX_PRIORITY    = 0,
		HUD_HIGH_PRIORITY   = 1,
		HUD_LOWEST_PRIORITY = 9,

		HUD_MSG_LONG_DUR  = 1165,	// 8 seconds
		HUD_MSG_SHORT_DUR =  436,	// 3 seconds
	};

	fixed16_16 s_flashEffect = 0;
	GameMessages s_hudMessages;
	// Font* s_hudFont;
	char s_hudMessage[80];
	s32 s_hudCurrentMsgId = 0;
	s32 s_hudMsgPriority = HUD_LOWEST_PRIORITY;
	Tick s_hudMsgExpireTime;

	JBool s_screenDirtyLeft[4]  = { 0 };
	JBool s_screenDirtyRight[4] = { 0 };

	GameMessage* hud_getMessage(GameMessages* messages, s32 msgId);
	char* strCopyAndZero(char* dst, const char* src, s32 bufferSize);

	void hud_sendTextMessage(s32 msgId)
	{
		GameMessage* msg = hud_getMessage(&s_hudMessages, msgId);
		// Only display the message if it is the same or lower priority than the current message.
		if (!msg || msg->priority > s_hudMsgPriority)
		{
			return;
		}

		const char* msgText = msg->text;
		if (!msgText[0]) { return; }
		strCopyAndZero(s_hudMessage, msgText, 80);

		s_hudMsgExpireTime = s_curTick + ((msg->priority <= HUD_HIGH_PRIORITY) ? HUD_MSG_LONG_DUR : HUD_MSG_SHORT_DUR);
		s_hudCurrentMsgId  = msgId;
		s_hudMsgPriority   = msg->priority;
	}

	void hud_clearMessage()
	{
		s_hudMessage[0] = 0;
		s_hudCurrentMsgId = 0;
		s_hudMsgPriority = HUD_LOWEST_PRIORITY;
		for (s32 i = 0; i < 4; i++)
		{
			s_screenDirtyLeft[i]  = JFALSE;
			s_screenDirtyRight[i] = JFALSE;
		}
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

	GameMessage* hud_getMessage(GameMessages* messages, s32 msgId)
	{
		s32 count = messages->count;
		GameMessage* msg = messages->msgList;

		for (s32 i = 0; i < count; i++, msg++)
		{
			if (msgId == msg->id)
			{
				return msg;
			}
		}
		return nullptr;
	}

	char* strCopyAndZero(char* dst, const char* src, s32 bufferSize)
	{
		char* dstPtr = dst;
		while (bufferSize > 0)
		{
			char ch = *src;
			if (ch == 0) { break; }
			bufferSize--;
			*dst = ch;

			src++;
			dst++;
		}
		while (bufferSize)
		{
			bufferSize--;
			*dst = 0;
			dst++;
		}
		return dstPtr;
	}
}  // TFE_DarkForces