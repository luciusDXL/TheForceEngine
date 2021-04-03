#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////
#include "infPublicTypes.h"
#include <TFE_System/types.h>

struct RWall;
struct RSector;
struct SecObject;

namespace TFE_InfSystem
{
	bool init();
	void shutdown();
	
	// ** Per-frame update functions **
	// Per elevator frame update.
	void update_elevators();
	// Per frame teleport update.
	void update_teleports();

	// ** Runtime API **
	// Send messages so that entities and the player can interact with the INF system.
	// If msgType = IMSG_SET/CLEAR_BITS the msg is processed directly for this wall OR
	// this iterates through the valid links and calls their msgFunc.
	// wall:    message target.
	// entity:  entity involved (such as the player) or nullptr.
	// evt:     event(s) (see InfEventMask above).
	// msgType: message type (such as IMSG_TRIGGER).
	void inf_wallSendMessage(RWall* wall, SecObject* entity, u32 evt, InfMessageType msgType);

	// Send messages so that entities and the player can interact with the INF system.
	// If msgType = IMSG_SET/CLEAR_BITS, IMSG_MASTER_ON/OFF, IMSG_WAKEUP it is processed directly for this sector AND
	// this iterates through the valid links and calls their msgFunc.
	// For evt, see InfEventMask above.
	// sector:  message target.
	// entity:  entity involved (such as the player) or nullptr.
	// evt:     event(s) (see InfEventMask above).
	// msgType: message type (such as IMSG_TRIGGER).
	void inf_sectorSendMessage(RSector* sector, SecObject* entity, u32 evt, InfMessageType msgType);

	// ** Loadtime API **
	// Used when loading levels.
	struct InfElevator;

	// For now load the INF data directly.
	// Move back to the asset system later.
	bool loadINF(const char* levelName);

	InfElevator* inf_allocateSpecialElevator(RSector* sector, InfSpecialElevator type);
	InfElevator* inf_allocateElevItem(RSector* sector, InfElevatorType type);
}
