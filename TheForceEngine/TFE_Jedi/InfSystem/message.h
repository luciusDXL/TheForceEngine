#pragma once
//////////////////////////////////////////////////////////////////////
// Message Address is the way named sectors are flagged during load
// in order to be looked up by the INF system.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rsector.h>
#include <vector>
#include <string>

enum MessageType
{
	MSG_FREE_TASK    = -1,
	MSG_RUN_TASK     = 0,
	MSG_FREE         = 1,
	MSG_SWITCH_WPN   = 2,
	MSG_START_FIRING = 4,
	MSG_STOP_FIRING  = 5,
	MSG_HOLSTER      = 6,
	MSG_TRIGGER      = 7,
	MSG_NEXT_STOP    = 8,
	MSG_PREV_STOP    = 9,
	MSG_GOTO_STOP    = 11,
	MSG_CRUSH        = 12,
	MSG_SEQ_COMPLETE = 20,
	MSG_DONE         = 21,
	MSG_DAMAGE       = 22,
	MSG_EXPLOSION    = 23,
	MSG_PICKUP       = 24,
	MSG_WAKEUP       = 25,
	MSG_TERMINAL_VEL = 26,
	MSG_MASTER_ON    = 29,
	MSG_MASTER_OFF   = 30,
	MSG_SET_BITS     = 31,
	MSG_CLEAR_BITS   = 32,
	MSG_COMPLETE     = 33,
	MSG_LIGHTS       = 34,
	MSG_CAMERA		 = 35,		// TFE - camera feature
	MSG_COUNT
};

typedef void(*MessageFunc)(MessageType, void*);
struct MessageAddress
{
	char name[16];
	s32  param0;
	s32  param1;
	RSector* sector;
};

namespace TFE_Jedi
{
	// Add and retrieve addresses, this way named sectors can be accessed without looping through the entire set.
	void message_addAddress(const char* name, s32 param0, s32 param1, RSector* sector);
	MessageAddress* message_getAddress(const char* name);
	void message_free();

	// Send a message to either an object or sector.
	void message_sendToObj(SecObject* obj, MessageType msgType, MessageFunc func);
	void message_sendToSector(RSector* sector, SecObject* entity, u32 evt, MessageType msgType);

	// Serialisation
	void level_serializeMessageAddresses(Stream* stream);

	// Optional message values - set these before calling message_XXX() and read inside the message function.
	extern void* s_msgEntity;
	extern void* s_msgTarget;
	extern u32 s_msgArg1;
	extern u32 s_msgArg2;
	extern u32 s_msgEvent;
}