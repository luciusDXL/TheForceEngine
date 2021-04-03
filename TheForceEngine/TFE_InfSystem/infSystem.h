#pragma once
//////////////////////////////////////////////////////////////////////
// INF System
// Classic Dark Forces (DOS) Jedi derived INF system. The INF System
// is a "script-like" system that controls level interactivity,
// level animation, and tracks completion.
//
// Copyright note:
// While the project as a whole is licensed under GPL-2.0, some of the
// code under TFE_InfSystem/ was derived from reverse-engineered
// code from "Dark Forces" (DOS) which is copyrighted by LucasArts.
//
// I consider the reverse-engineering to be "Fair Use" - a means of 
// supporting the games on other platforms and to improve support on
// existing platforms without claiming ownership of the games
// themselves or their IPs.
//
// That said using this code in a commercial project is risky without
// permission of the original copyright holders (LucasArts).
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
	// These get called (potentially) every frame and update the current INF state or
	// state of objects interacting with the system.

	// Per elevator frame update.
	void update_elevators();
	// Per frame teleport update.
	void update_teleports();
	
	// ** Runtime API **
	// Messages are the way entities and the player interact with the INF system during gameplay.

	// Send a message to a specific wall or line, such as when "nudging" a switch or crossing a line.
	// wall:    message target.
	// entity:  entity involved (such as the player) or nullptr.
	// evt:     event(s) (see InfEventMask  in "infPublicTypes.h" such as INF_EVENT_CROSS_LINE_FRONT).
	// msgType: message type (such as IMSG_TRIGGER).
	void inf_wallSendMessage(RWall* wall, SecObject* entity, u32 evt, InfMessageType msgType);

	// Send a message to a sector, such as entering, leaving, or "nudging".
	// sector:  message target.
	// entity:  entity involved (such as the player) or nullptr.
	// evt:     event(s) (see InfEventMask in "infPublicTypes.h" such as INF_EVENT_ENTER_SECTOR).
	// msgType: message type (such as IMSG_TRIGGER).
	void inf_sectorSendMessage(RSector* sector, SecObject* entity, u32 evt, InfMessageType msgType);
	
	// ** Loadtime API **
	// These functions are used at load-time to setup special elevators based on sector flags or load
	// the level INF.

	// Used when loading levels.
	struct InfElevator;

	// For now load the INF data directly.
	// Move back to the asset system later.
	bool loadINF(const char* levelName);

	InfElevator* inf_allocateSpecialElevator(RSector* sector, InfSpecialElevator type);
	InfElevator* inf_allocateElevItem(RSector* sector, InfElevatorType type);
}
