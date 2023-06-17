#pragma once
#include <TFE_System/types.h>
#include <TFE_Jedi/InfSystem/message.h>
#include <TFE_Jedi/Task/task.h>

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
	INF_EVENT_NUDGE_FRONT      = FLAG_BIT(4),	// Front of line or inside sector.
	INF_EVENT_NUDGE_BACK       = FLAG_BIT(5),	// Back of line or outside sector.
	INF_EVENT_EXPLOSION        = FLAG_BIT(6),
	INF_EVENT_UNUSED1          = FLAG_BIT(7),	// Unused slot.
	INF_EVENT_SHOOT_LINE       = FLAG_BIT(8),	// Shoot or punch line.
	INF_EVENT_LAND             = FLAG_BIT(9),	// Land on floor
	INF_EVENT_UNUSED2          = FLAG_BIT(10),
	INF_EVENT_UNUSED3          = FLAG_BIT(11),
	INF_EVENT_INTERNAL         = FLAG_BIT(31),	// Internal Event
	INF_EVENT_NONE = 0,
	INF_EVENT_ANY = 0xffffffff
};

enum LinkType
{
	LTYPE_SECTOR = 0,
	LTYPE_TRIGGER,
	LTYPE_TELEPORT,
};

struct Allocator;
namespace TFE_Jedi
{
	typedef void(*InfFreeFunc)(void*);

	struct InfElevator;
	struct InfTrigger;
	struct Teleport;

	struct InfLink
	{
		LinkType type;				// Sector or Trigger
		Task* task;					// Either the Elevator or Trigger msg func.
		union
		{
			InfElevator* elev;		// The actual INF item.
			InfTrigger* trigger;
			Teleport* teleport;
			void* target;
		};
		u32 eventMask;				// The event mask which helps determine if a link can be activated.
		u32 entityMask;				// The entity mask (as above).
		Allocator* parent;			// The parent list of links.
		InfFreeFunc freeFunc;		// The function to use to free the link.
		// TFE:
		s32 serializeIndex;			// An index used to fix up pointers after all of the data is loaded.
	};
}