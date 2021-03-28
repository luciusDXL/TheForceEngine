#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////
#include "infMessageType.h"
#include <TFE_System/types.h>
#include <vector>
#include <string>

struct RWall;
struct RSector;
struct SecObject;

namespace TFE_InfSystem
{
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

	bool init();
	void shutdown();

	// For now load the INF data directly.
	// Move back to the asset system later.
	bool loadINF(const char* levelName);

	// Per elevator frame update.
	void update_elevators();
	// Per frame teleport update.
	void update_teleports();

	// Send messages so that entities and the player can interact with the INF system.
	// If msgType = IMSG_SET/CLEAR_BITS the msg is processed directly for this wall OR
	// this iterates through the valid links and calls their msgFunc.
	void inf_wallSendMessage(RWall* wall, SecObject* entity, u32 evt, InfMessageType msgType);

	// Send messages so that entities and the player can interact with the INF system.
	// If msgType = IMSG_SET/CLEAR_BITS, IMSG_MASTER_ON/OFF, IMSG_WAKEUP it is processed directly for this sector AND
	// this iterates through the valid links and calls their msgFunc.
	void inf_sectorSendMessage(RSector* sector, SecObject* obj, u32 evt, InfMessageType msgType);

	// Used when loading levels.
	struct InfElevator;

	InfElevator* inf_allocateSpecialElevator(RSector* sector, InfSpecialElevator type);
	InfElevator* inf_allocateElevItem(RSector* sector, InfElevatorType type);
}
