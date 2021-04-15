#include "message.h"
#include <TFE_Memory/allocator.h>
#include <TFE_System/system.h>
#include <TFE_System/memoryPool.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

using namespace TFE_Memory;

namespace Message
{
	Allocator* s_messageAddr = nullptr;

	void free()
	{
		allocator_free(s_messageAddr);
		s_messageAddr = nullptr;
	}

	void addAddress(const char* name, s32 param0, s32 param1, RSector* sector)
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

	MessageAddress* getAddress(const char* name)
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
}