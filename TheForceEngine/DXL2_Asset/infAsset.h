#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine INF script
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>
#include <string>
#include <vector>

enum Keys
{
	KEY_RED = (1 << 0),
	KEY_YELLOW = (1 << 1),
	KEY_BLUE = (1 << 2),
	KEY_CODE_1 = (1 << 3),
	KEY_CODE_2 = (1 << 4),
	KEY_CODE_3 = (1 << 5),
	KEY_CODE_4 = (1 << 6),
	KEY_CODE_5 = (1 << 7),
	KEY_CODE_6 = (1 << 8),
	KEY_CODE_7 = (1 << 9),
	KEY_CODE_8 = (1 << 10),
	KEY_CODE_9 = (1 << 11),
};

enum InfItemType
{
	INF_ITEM_LEVEL = 0,
	INF_ITEM_SECTOR,
	INF_ITEM_LINE,
	INF_ITEM_COUNT
};

enum InfClass
{
	INF_CLASS_ELEVATOR = 0,
	INF_CLASS_TRIGGER,
	INF_CLASS_TELEPORTER,
	INF_CLASS_COUNT
};

enum InfSubClass
{
	// Elevators
	ELEVATOR_CHANGE_LIGHT = 0,		// Changes sector Ambient.
	ELEVATOR_BASIC,					// Change floor altitude of sector  (can use Smart Object Reactions)
	ELEVATOR_INV,					// Change ceiling altitude of sector (can use Smart Object Reactions)
	ELEVATOR_MOVE_FLOOR,			// Change floor altitude of sector  (can NOT use Smart Object Reactions)
	ELEVATOR_MOVE_CEILING,			// Change ceiling altitude of sector (can NOT use Smart Object Reactions)
	ELEVATOR_MOVE_FC,				// Changes both the floor and ceiling altitude, stop = floor alt.
	ELEVATOR_SCROLL_FLOOR,			// Scroll floor texture in texels (x8 = world units). Players move with floor by default.
	ELEVATOR_SCROLL_CEILING,		// Scroll the ceiling texture of the sector (in texels).
	ELEVATOR_MOVE_OFFSET,			// Change the second alt of sector.
	ELEVATOR_BASIC_AUTO,			// Same as BASIC except returns to Alt 0 after cycling through all stops. Needs to be triggered twice to go to first stop again.

	ELEVATOR_CHANGE_WALL_LIGHT,		// Change LIGHT of any wall in sector with WF1_CHANGE_WALL_LIGHT set. (Does not work if sector ambient = 31).
	ELEVATOR_MORPH_MOVE1,			// Translate vertex positions of any walls with WF1_WALL_MORPHS set. If adjoined their mirrors will also move. By default the player will not move with the walls. Stop = distance on XZ plane relative to the wall.
	ELEVATOR_MORPH_MOVE2,			// Same as MORPH_MOVE1 but player will be moved by default.

	ELEVATOR_MORPH_SPIN1,			// Rotate vertex positions of any walls with WF1_WALL_MORPHS set. If adjoined their mirrors will also move. By default the player will not spin with the walls. Stop = distance on XZ plane relative to the wall.
	ELEVATOR_MORPH_SPIN2,			// Same as MORPH_SPIN1 but player will be rotated by the walls by default.
	ELEVATOR_MOVE_WALL,				// Same as ELEVATOR_MORPH_MOVE1 but default event_mask = 0
	ELEVATOR_ROTATE_WALL,			// Same as ELEVATOR_MORPH_SPIN1 but default event_mask = 0
	ELEVATOR_SCROLL_WALL,			// Scroll textures on walls with WF1_SCROLL_??_TEX set. Stop values are distances in texels.

	ELEVATOR_DOOR,					// Creates a default door, similar to SEC_FLAGS1_DOOR
	ELEVATOR_DOOR_MID,				// Split door (up and down). Sector altitudes are set for when the door is open. [addon: 0; addon: 1]
	ELEVATOR_DOOR_INV,				// The door opens downward but otherwise acts like ELEVATOR_DOOR

	// Triggers						// If no message is specified, m_trigger is sent.
	TRIGGER_STANDARD,				// Can be applied to a sector or line but not switches.
	TRIGGER,						// Same as standard.
	TRIGGER_SWITCH1,				// Used for switches (wall must have a SIGN). When pressed, the texture will change to the second in the BM. The second cannot be pressed.
	TRIGGER_SINGLE,					// Same as SWITCH1 but can only be used once.
	TRIGGER_TOGGLE,					// Switch that can be toggled between states.

	// Chute
	TELEPORTER_CHUTE,				// Silent teleport that acts like falling through one sector to the next.

	// None
	SUBCLASS_NONE,

	// Counts
	ELEVATOR_COUNT = ELEVATOR_DOOR_INV + 1,
	TRIGGER_COUNT = TRIGGER_TOGGLE - TRIGGER_STANDARD + 1,
	TELEPORTER_COUNT = 1,
};

enum InfEventMask
{
	INF_EVENT_CROSS_LINE_FRONT	= (1u << 0u),
	INF_EVENT_CROSS_LINE_BACK	= (1u << 1u),
	INF_EVENT_ENTER_SECTOR		= (1u << 2u),
	INF_EVENT_LEAVE_SECTOR		= (1u << 3u),
	INF_EVENT_NUDGE_FRONT		= (1u << 4u),	// front of line or inside sector.
	INF_EVENT_NUDGE_BACK		= (1u << 5u),	// back of line or outside sector.
	INF_EVENT_EXPLOSION			= (1u << 6u),
	INF_EVENT_UNKNOWN			= (1u << 7u),	// skipped slot or unused event?
	INF_EVENT_SHOOT_LINE		= (1u << 8u),	// Shoot or punch line.
	INF_EVENT_LAND				= (1u << 9u),	// Land on floor
	INF_EVENT_ANY               = INF_EVENT_CROSS_LINE_FRONT | INF_EVENT_CROSS_LINE_BACK | INF_EVENT_ENTER_SECTOR | INF_EVENT_LEAVE_SECTOR | INF_EVENT_NUDGE_FRONT | INF_EVENT_NUDGE_BACK | INF_EVENT_EXPLOSION | INF_EVENT_SHOOT_LINE | INF_EVENT_LAND
};

//enum InfEntityMask
// Cannot use the enum{} because it seems to be forced to signed int32.
static const u32 INF_ENTITY_ENEMY = (1u << 0u);
static const u32 INF_ENTITY_WEAPON = (1u << 3u);
static const u32 INF_ENTITY_PLAYER = (1u << 31u);

enum InfMoveFlags
{
	INF_MOVE_NONE    = 0,
	INF_MOVE_FLOOR   = (1 << 0),
	INF_MOVE_SECALT  = (1 << 1),
	INF_MOVE_UNKNOWN = (1 << 2),	// check out the effect on FUELSTAT
};

enum SoundEffectIndex
{
	SFX_LEAVE_STOP = 0,
	SFX_MOVING,
	SFX_ARRIVE_STOP,
};

enum InfMessage
{
	INF_MSG_M_TRIGGER = 0,	// Trigger an elevator (next stop) or another trigger. Param: EventValue (optional) - event value (matched with event_mask).
	INF_MSG_GOTO_STOP,		// Sends an elevator to the stop specified. Param: stopNumber.
	INF_MSG_NEXT_STOP,		// Sends an elevator to its next stop. Param: EventValue (optional) - event value (matched with event_mask).
	INF_MSG_PREV_STOP,		// Sends an elevator to its previous stop. Param: EventValue (optional) - event value (matched with event_mask).
	INF_MSG_MASTER_ON,		// Turns an elevator or trigger's master on. Also turns on generators in sector. Param: EventValue (optional) - event value (matched with event_mask).
	INF_MSG_MASTER_OFF,		// Turns an elevator or trigger's master off. Also turns off generators in sector. Param: EventValue (optional) - event value (matched with event_mask).
	INF_MSG_CLEAR_BITS,		// Clear bits from wall or sector. Param: flagNumber (1,2,3), bitsToClear.
	INF_MSG_SET_BITS,		// Set bits from wall or sector. Param: flagNumber (1,2,3), bitsToClear.
	INF_MSG_COMPLETE,		// Triggers GOL in PDA, moves elevator to next stop. Goal number (TRIG: num in GOL file).
	INF_MSG_DONE,			// Puts a switch on its first texture, making it usable again (unless it is TRIGGER_SINGLE).
	INF_MSG_WAKEUP,			// Vue with "Pause:true" will continue if this is sent to the 3D object's sector.
	INF_MSG_LIGHTS,			// Sets sector ambient, for all sectors, to the value in their flags3.
	// Special elevator functions
	INF_MSG_ADJOIN,			// Param: sector1, line1, sector2, line2
	INF_MSG_PAGE,			// Param: sound file
	INF_MSG_TEXT,			// Param: text number
	INF_MSG_TEXTURE,		// Copy floor or ceiling texture from another sector; Param: flag (0 = floor / 1 = ceil), donor (sectorId)
	INF_MSG_INVALID,
	INF_MSG_COUNT = INF_MSG_LIGHTS + 1
};

enum InfStopValue0Type
{
	INF_STOP0_ABSOLUTE = 0,		// Absolute value (height, light level, etc.)
	INF_STOP0_RELATIVE,			// Relative value
	INF_STOP0_SECTORNAME,		// Sector
	INF_STOP0_COUNT
};

enum InfStopValue1Type
{
	INF_STOP1_TIME = 0,			// Time an elevator remains at the stop (in seconds).
	INF_STOP1_HOLD,				// Hold at stop indefinitely.
	INF_STOP1_TERMINATE,		// Elevator will stay at the stop permanently.
	INF_STOP1_COMPLETE,			// Mission will be complete when reaching this stop.
	INF_STOP1_COUNT
};

struct InfoMsgArg
{
	union
	{
		u32 uintValue;
		s32 intValue;
		char strValue[32];
	};
};

struct InfArg
{
	union
	{
		s32 iValue;
		f32 fValue;
	};
};

struct InfFunction
{
	u32  code;		// func | clientCount << 8 | argCount << 16  (top bot available).
	u32* client;	// sectorId | (wallId << 16), where hasWall = wallId < 0xffff
	InfArg* arg;
};

// A trigger will have 1 stop with code = 0 | funcCount << 8, time = 0
struct InfStop
{
	u32 code;		// InfStopValue0Type | InfStopValue1Type << 4 | funcCount << 8
	InfArg value0;
	f32 time;		// time at stop or 0.
	InfFunction* func;
};

struct InfVariables
{
	bool master = true;	// Determines if an elevator or trigger can function.
	u32 event = 0;		// Custom event value(s) for trigger.
	s32 start = 0;		// Determines which stop an elevator starts at on level load.
	/////////////////////// Masks ///////////////////////
	u32 event_mask = 0;	// (InfEventMask) Determines if an event will operate on an elevator or trigger. Defaults: 52 (elev basic, inv, basic_auto), 60 (morph1/2, spin1/2), other elev 0; trigger 0xffffffff
	u32 entity_mask =   // (InfEntityMask) Defines the type of entities that can activate a Trigger.
		INF_ENTITY_PLAYER;
	//////////////////////// Flags & keys ///////////////////////
	u32 key = 0;		// Key required to activate an elevator (yellow, blue, red).
	u32 flags = 0;		// (InfMoveFlags) Determines whether the player moves with morphing or scrolling elevators. Defaults: 3 (scroll_floor, morph2, spin2) otherwise 0.
	/////////////////////// Movement & rotation ///////////////////////
	Vec2f center =		// Center coordinates for rotation.
	{ 0.0f, 0.0f };
	f32 angle = 0.0f;	// Direction of texture scrolling or horizontal elevators. For walls 0 = down others 0 = north.
	f32 speed = 0.0f;	// Speed an elevator moves between stops. 0 = instant.
	f32 speed_addon[2]; // Addon for door_mid
	/////////////////////// Sound ///////////////////////
	s32 sound[3];		// VOC sound index. 0 is used for triggers, 0 - 2 for elevators. Note -1 means no sound at all.
	/////////////////////// Target/Slave ///////////////////////
	s32 target;			// Target sector for teleport.
};

struct InfClassData
{
	u8  iclass, isubclass;
	u16 stopCount;		// Can have more than 256 stops.
	u16 slaveCount;
	s16 mergeStart;		// start of merges - since these will still affect objects.
	u32 stateIndex;		// Used by the runtime to map to the runtime state.

	InfVariables var;
	InfStop* stop;
	u16* slaves;
};

struct InfItem
{
	u32 id;			// sectorId | wallNum << 16; hasWall = wallNum < 0xffff
	u8 type;
	u8 classCount;
	u8 pad[2];

	InfClassData* classData;
};

struct InfData
{
	u32 itemCount;
	s32 completeId;
	s32 doorStart;
	InfItem* item;
};

class MemoryPool;

namespace DXL2_InfAsset
{
	bool load(const char* name);
	u32  countSectorFlagDoors();
	void setupDoors();
	void unload();

	InfData* getInfData();
	const char* getClassName(u32 iclass);
	const char* getSubclassName(u32 iclass, u32 sclass);
	const char* getFuncName(u32 funcId);

	MemoryPool* getMemoryPool(bool clearPool);
};
