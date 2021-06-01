#pragma once
#include <TFE_System/types.h>

enum InfMessageType
{
	IMSG_FREE       = 1,		// Internal Only - delete the InfTrigger or InfElevator.
	IMSG_TRIGGER    = 7,
	IMSG_NEXT_STOP  = 8,
	IMSG_PREV_STOP  = 9,
	IMSG_GOTO_STOP  = 11,
	IMSG_REV_MOVE   = 12,
	IMSG_DONE       = 21,
	IMSG_DAMAGE     = 22,
	IMSG_EXPLOSION  = 23,
	IMSG_WAKEUP     = 25,
	IMSG_MASTER_ON  = 29,
	IMSG_MASTER_OFF = 30,
	IMSG_SET_BITS   = 31,
	IMSG_CLEAR_BITS = 32,
	IMSG_COMPLETE   = 33,
	IMSG_LIGHTS     = 34,
};

// Core elevator types.
enum InfElevatorType
{
	IELEV_MOVE_CEILING = 0,
	IELEV_MOVE_FLOOR,
	IELEV_MOVE_OFFSET,
	IELEV_MOVE_WALL,
	IELEV_ROTATE_WALL,
	IELEV_SCROLL_WALL,
	IELEV_SCROLL_FLOOR,
	IELEV_SCROLL_CEILING,
	IELEV_CHANGE_LIGHT,
	IELEV_MOVE_FC,
	IELEV_CHANGE_WALL_LIGHT,
	IELEV_COUNT,
};

// "Special" elevators are "high level" elevators that map to the core
// 11 types (see InfElevatorType) but have special settings and/or
// automatically add stops to make commonly used patterns easier to setup.
enum InfSpecialElevator
{
	IELEV_SP_BASIC = 0,
	IELEV_SP_BASIC_AUTO,
	// Both of these are unimplemented in the final game.
	IELEV_SP_UNIMPLEMENTED,
	IELEV_SP_MID,
	// Back to implemented types.
	IELEV_SP_INV = 4,
	IELEV_SP_DOOR,
	IELEV_SP_DOOR_INV,
	IELEV_SP_DOOR_MID,
	IELEV_SP_MORPH_SPIN1,
	IELEV_SP_MORPH_SPIN2,
	IELEV_SP_MORPH_MOVE1,
	IELEV_SP_MORPH_MOVE2,
	IELEV_SP_EXPLOSIVE_WALL,
	IELEV_SP_COUNT
};

enum InfEventMask
{
	INF_EVENT_CROSS_LINE_FRONT = FLAG_BIT(0),
	INF_EVENT_CROSS_LINE_BACK  = FLAG_BIT(1),
	INF_EVENT_ENTER_SECTOR     = FLAG_BIT(2),
	INF_EVENT_LEAVE_SECTOR     = FLAG_BIT(3),
	INF_EVENT_NUDGE_FRONT      = FLAG_BIT(4),	// front of line or inside sector.
	INF_EVENT_NUDGE_BACK       = FLAG_BIT(5),	// back of line or outside sector.
	INF_EVENT_EXPLOSION        = FLAG_BIT(6),
	INF_EVENT_UNKNOWN          = FLAG_BIT(7),	// skipped slot or unused event?
	INF_EVENT_SHOOT_LINE       = FLAG_BIT(8),	// Shoot or punch line.
	INF_EVENT_LAND             = FLAG_BIT(9),	// Land on floor
	INF_EVENT_10               = FLAG_BIT(10),	// Unknown event
	INF_EVENT_11               = FLAG_BIT(11),	// Unknown Event
	INF_EVENT_31               = FLAG_BIT(31),	// Unknown Event
	INF_EVENT_NONE = 0,
	INF_EVENT_ANY = 0xffffffff
};