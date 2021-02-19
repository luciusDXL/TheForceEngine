#pragma once
#include <TFE_System/types.h>
#include <TFE_JediRenderer/rmath.h>	// TODO: Move this out so it is cleanly accessible by the renderer and INF system.
#include "infMessageType.h"

struct TextureData;
struct Allocator;

namespace TFE_JediRenderer
{
	struct RWall;
	struct RSector;
	struct SecObject;
}
using namespace TFE_JediRenderer;

namespace TFE_InfSystem
{
	typedef void(*InfLinkMsgFunc)(InfMessageType);
	typedef void(*InfFreeFunc)();
	struct InfLink;

	enum InfElevatorType
	{
		IELEV_MOVE_CEILING = 0,
		IELEV_MOVE_FLOOR = 1,
		IELEV_MOVE_OFFSET = 2,
		IELEV_MOVE_WALL = 3,
		IELEV_ROTATE_WALL = 4,
		IELEV_SCROLL_WALL = 5,
		IELEV_SCROLL_FLOOR = 6,
		IELEV_SCROLL_CEILING = 7,
		IELEV_CHANGE_LIGHT = 8,
		IELEV_MOVE_FC = 9,
		IELEV_CHANGE_WALL_LIGHT = 10,
	};

	enum TriggerCmd
	{
		TCMD_DONE = 7,
		TCMD_NEXT_STOP = 8,
		TCMD_PREV_STOP = 9,
		TCMD_GOTO_STOP = 11,
		TCMD_MASTER_ON = 29,
		TCMD_MASTER_OFF = 30,
		TCMD_SET_BITS = 31,
		TCMD_CLEAR_BITS = 32,
		TCMD_COMPLETE = 33,
		TCMD_LIGHTS = 34
	};

	enum ElevMoveType
	{
		MOVE_STOPPED = -1,
		MOVE_FLOOR = 0,
		MOVE_CEIL = 1,
	};

	enum ElevUpdateFlags
	{
		ELEV_MOVING = (1 << 0),			// elevator should be moving.
		ELEV_MASTER_ON = (1 << 1),		// master on
		ELEV_MOVING_REVERSE = (1 << 2),	// the elevator is moving in reverse.
	};

	enum InfDelay
	{
		// IDELAY_SECONDS >= 0
		IDELAY_HOLD = -1,
		IDELAY_TERMINATE = -2,
		IDELAY_COMPLETE = -3,
	};

	enum TriggerType
	{
		ITRIGGER_STANDARD = 0,
		ITRIGGER_SECTOR = 1,
		ITRIGGER_SWITCH1 = 2,
		ITRIGGER_TOGGLE = 3,
		ITRIGGER_SINGLE = 4,
	};

	enum InfEventMask
	{
		INF_EVENT_CROSS_LINE_FRONT = (1u << 0u),
		INF_EVENT_CROSS_LINE_BACK = (1u << 1u),
		INF_EVENT_ENTER_SECTOR = (1u << 2u),
		INF_EVENT_LEAVE_SECTOR = (1u << 3u),
		INF_EVENT_NUDGE_FRONT = (1u << 4u),	// front of line or inside sector.
		INF_EVENT_NUDGE_BACK = (1u << 5u),	// back of line or outside sector.
		INF_EVENT_EXPLOSION = (1u << 6u),
		INF_EVENT_UNKNOWN = (1u << 7u),		// skipped slot or unused event?
		INF_EVENT_SHOOT_LINE = (1u << 8u),	// Shoot or punch line.
		INF_EVENT_LAND = (1u << 9u),		// Land on floor
		INF_EVENT_ANY = INF_EVENT_CROSS_LINE_FRONT | INF_EVENT_CROSS_LINE_BACK | INF_EVENT_ENTER_SECTOR | INF_EVENT_LEAVE_SECTOR | INF_EVENT_NUDGE_FRONT | INF_EVENT_NUDGE_BACK | INF_EVENT_EXPLOSION | INF_EVENT_SHOOT_LINE | INF_EVENT_LAND
	};

	enum KeyItem
	{
		KEY_RED = 23,
		KEY_YELLOW = 24,
		KEY_BLUE = 25,
	};

	enum ObjectTypeFlags
	{
		OTFLAG_NONE = 0,
		// ... Unknown.
		OTFLAG_PLAYER = (1u << 31u),
		OTFLAG_UNSIGNED = 0xffffffff,
	};

	struct AnimatedTexture
	{
		s32 count;
		s32 index;
		s32 delay;
		s32 nextTime;
		TextureData** frameList;
		TextureData** basePtr;
		TextureData* curFrame;
		u8* baseData;
	};

	struct TriggerTarget
	{
		RSector* sector;
		RWall*   wall;
		u32     flags;
	};

	struct InfTrigger
	{
		TriggerType type;
		InfLink* link;
		AnimatedTexture* animTex;
		Allocator* targets;
		TriggerCmd cmd;
		u32 event;
		s32 arg0;
		s32 arg1;

		s32 u20;
		s32 u24;
		s32 u28;
		s32 u2c;

		u32 u30;
		s32 u34;
		s32 master;
		TextureData* tex;
		s32 soundId;
		s32 state;
		s32* u48;
		s32 textId;
	};

	struct InfMessage
	{
		RSector* sector;
		RWall* wall;
		InfMessageType msgType;
		u32 event;
		s32 arg1;
		s32 arg2;
	};

	struct Slave
	{
		RSector* sector;
		s32 value;
	};

	struct AdjoinCmd
	{
		RWall* wall0;
		RWall* wall1;
		RSector* sector0;
		RSector* sector1;
	};

	struct Stop
	{
		s32 value;
		// -1 = HOLD, -2 = TERMINATE, -3 = COMPLETE, else TICKS (see InfDelay)
		s32 delay;				// INF ticks (145.5/sec)
		Allocator* messages;
		Allocator* adjoinCmds;
		s32 pageId;
		TextureData* floorTex;
		TextureData* ceilTex;
	};

	struct InfElevator
	{
		InfElevator* self;
		InfElevatorType type;
		s32 moveType;
		RSector* sector;
		s32 key;
		s32 fixedStep;
		u32 nextTime;
		s32 u1c;
		Allocator* stops;
		Allocator* slaves;
		Stop* nextStop;
		s32  speed;
		s32* value;
		s32 iValue;
		vec2_fixed dirOrCenter;
		s32 flags;
		s32 sound0;
		s32 sound1;
		s32 sound2;
		s32 soundSource1;
		s32 u54;
		s32 updateFlags;
	};

	struct InfLink
	{
		s32 type;
		InfLinkMsgFunc msgFunc;
		union
		{
			InfElevator* elev;
			InfTrigger* trigger;
			void* target;
		};
		u32 eventMask;
		u32 entityMask;
		Allocator* parent;
		InfFreeFunc freeFunc;
	};
}