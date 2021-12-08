#include <cstring>

#include "message.h"
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_System/system.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/Task/task.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_DarkForces/logic.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

namespace TFE_Jedi
{
	Allocator* s_messageAddr = nullptr;
	void* s_msgEntity;
	void* s_msgTarget;
	u32 s_msgArg1;
	u32 s_msgArg2;
	u32 s_msgEvent;

	void message_free()
	{
		s_messageAddr = nullptr;
	}

	void message_addAddress(const char* name, s32 param0, s32 param1, RSector* sector)
	{
		if (!s_messageAddr)
		{
			s_messageAddr = allocator_create(sizeof(MessageAddress));
		}
		MessageAddress* msgAddr = (MessageAddress*)allocator_newItem(s_messageAddr);

		assert(strlen(name) <= 16);
		strncpy(msgAddr->name, name, 16);

		msgAddr->param0 = param0;
		msgAddr->param1 = param1;
		msgAddr->sector = sector;
	}

	MessageAddress* message_getAddress(const char* name)
	{
		MessageAddress* msgAddr = (MessageAddress*)allocator_getHead(s_messageAddr);
		while (msgAddr)
		{
			if (strncasecmp(name, msgAddr->name, 16) == 0)
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
		Logic** logicList = (Logic**)allocator_getHead((Allocator*)obj->logic);
		while (logicList)
		{
			Logic* logic = *logicList;
			s_msgTarget = logic;
			{
				if (logic->task == task_getCurrent() && func)
				{
					//s_taskId = msgType;
					func(msgType, logic);
					//s_taskId = 0;
				}
				else if (task_hasLocalMsgFunc(logic->task))
				{
					task_runLocal(logic->task, msgType);
				}
				else
				{
					task_runAndReturn(logic->task, msgType);
				}
			}
			logicList = (Logic**)allocator_getNext((Allocator*)obj->logic);
		}
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