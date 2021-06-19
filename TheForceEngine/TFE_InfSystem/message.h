#pragma once
//////////////////////////////////////////////////////////////////////
// Message Address is the way named sectors are flagged during load
// in order to be looked up by the INF system.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Level/robject.h>
#include <TFE_Level/rsector.h>
#include <vector>
#include <string>

struct RSector;

struct MessageAddress
{
	char name[16];		// 0x00
	s32  param0;		// 0x10
	s32  param1;		// 0x14
	RSector* sector;	// 0x18
};

typedef void(*MessageFunc)(void*);

enum MessageType
{
	MSG_FREE = 1,		// Internal Only - delete the InfTrigger or InfElevator.
	MSG_TRIGGER = 7,
	MSG_NEXT_STOP = 8,
	MSG_PREV_STOP = 9,
	MSG_GOTO_STOP = 11,
	MSG_REV_MOVE = 12,
	MSG_DONE = 21,
	MSG_DAMAGE = 22,
	MSG_EXPLOSION = 23,
	MSG_WAKEUP = 25,
	MSG_MASTER_ON = 29,
	MSG_MASTER_OFF = 30,
	MSG_SET_BITS = 31,
	MSG_CLEAR_BITS = 32,
	MSG_COMPLETE = 33,
	MSG_LIGHTS = 34,
	MSG_COUNT
};

namespace TFE_Message
{
	void message_addAddress(const char* name, s32 param0, s32 param1, RSector* sector);
	MessageAddress* message_getAddress(const char* name);
	void message_free();

	void message_sendToObj(SecObject* obj, MessageType msgType, MessageFunc func);
	void message_sendToSector(RSector* sector, SecObject* entity, u32 evt, MessageType msgType);
}