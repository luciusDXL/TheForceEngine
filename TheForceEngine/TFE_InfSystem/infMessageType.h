#pragma once
enum InfMessageType
{
	IMSG_FREE       = 1,		// Internal Only - delete the InfTrigger or InfElevator.
	IMSG_TRIGGER    = 7,
	IMSG_NEXT_STOP  = 8,
	IMSG_PREV_STOP  = 9,
	IMSG_GOTO_STOP  = 11,
	IMSG_REV_MOVE   = 12,
	IMSG_DONE       = 21,
	IMSG_WAKEUP     = 25,
	IMSG_MASTER_ON  = 29,
	IMSG_MASTER_OFF = 30,
	IMSG_SET_BITS   = 31,
	IMSG_CLEAR_BITS = 32,
	IMSG_COMPLETE   = 33,
	IMSG_LIGHTS     = 34,
};