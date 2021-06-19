#include "message.h"
#include <TFE_Memory/allocator.h>
#include <TFE_System/system.h>
#include <TFE_System/memoryPool.h>
#include <TFE_InfSystem/infSystem.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

using namespace TFE_Memory;
using namespace TFE_InfSystem;

namespace TFE_Message
{
	Allocator* s_messageAddr = nullptr;
	void* s_msgEntity;
	void* s_msgTarget;
	u32 s_msgArg1;
	u32 s_msgArg2;
	u32 s_msgEvent;

	void message_free()
	{
		allocator_free(s_messageAddr);
		s_messageAddr = nullptr;
	}

	void message_addAddress(const char* name, s32 param0, s32 param1, RSector* sector)
	{
		if (!s_messageAddr)
		{
			s_messageAddr = allocator_create(sizeof(MessageAddress));
		}
		MessageAddress* msgAddr = (MessageAddress*)allocator_newItem(s_messageAddr);

		memset(msgAddr->name, 0, 16);
		strcpy(msgAddr->name, name);

		msgAddr->param0 = param0;
		msgAddr->param1 = param1;
		msgAddr->sector = sector;
	}

	MessageAddress* message_getAddress(const char* name)
	{
		MessageAddress* msgAddr = (MessageAddress*)allocator_getHead(s_messageAddr);
		while (msgAddr)
		{
			if (strcasecmp(name, msgAddr->name) == 0)
			{
				return msgAddr;
			}
			msgAddr = (MessageAddress*)allocator_getNext(s_messageAddr);
		}

		TFE_System::logWrite(LOG_ERROR, "INF", "Message_GetAddress: ADDRESS NOT FOUND: %s", name);
		return nullptr;
	}

	void message_sendToObj(SecObject* obj, MessageType msgType, MessageFunc func)
	{
		// TODO: was inf_sendObjMessage
	}

	// Send messages so that entities and the player can interact with the INF system.
	// If msgType = MSG_SET/CLEAR_BITS, MSG_MASTER_ON/OFF, MSG_WAKEUP it is processed directly for this sector AND
	// this iterates through the valid links and calls their msgFunc.
	void message_sendToSector(RSector* sector, SecObject* entity, u32 evt, MessageType msgType)
	{
		// Was: inf_sendSectorMessage()
		// Changed: inf_sendSectorMessageInternal() -> inf_sendSectorMessage()
		inf_sendSectorMessage(sector, msgType);
		inf_sendLinkMessages(sector->infLink, entity, evt, msgType);
	}
}