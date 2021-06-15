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
#include <TFE_Level/fixedPoint.h>

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

	// Send a message to a specific wall or line at a specific location on the wall. This is used for projectiles hitting switches.
	// This always sends the "IMSG_TRIGGER" message.
	// wall:     message target.
	// entity:   entity involved (such as the player) or nullptr.
	// evt:      event(s) (see InfEventMask  in "infPublicTypes.h" such as INF_EVENT_CROSS_LINE_FRONT).
	// paramPos: the parametric horizontal position on the line from [0, length].
	// yPos:     the y position.
	void inf_wallSendMessageAtPos(RWall* wall, SecObject* entity, u32 evt, TFE_CoreMath::fixed16_16 paramPos, TFE_CoreMath::fixed16_16 yPos);
	void inf_wallAndMirrorSendMessageAtPos(RWall* wall, SecObject* entity, u32 evt, TFE_CoreMath::fixed16_16 paramPos, TFE_CoreMath::fixed16_16 yPos);

	// Send a message to a sector, such as entering, leaving, or "nudging".
	// sector:  message target.
	// entity:  entity involved (such as the player) or nullptr.
	// evt:     event(s) (see InfEventMask in "infPublicTypes.h" such as INF_EVENT_ENTER_SECTOR).
	// msgType: message type (such as IMSG_TRIGGER).
	void inf_sectorSendMessage(RSector* sector, SecObject* entity, u32 evt, InfMessageType msgType);

	// Send a message to a specific object.
	void inf_sendObjMessage(SecObject* obj, InfMessageType msgType, void(*func)(void*));

	// Returns JTRUE if the object is sitting on a moving floor or second height.
	JBool inf_isOnMovingFloor(SecObject* obj, InfElevator* elev, RSector* sector);

	// Get the moving elevator velocity.
	void inf_getMovingElevatorVelocity(InfElevator* elev, vec3_fixed* vel, fixed16_16* speed);
	
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

	extern void* s_infMsgEntity;
	extern void* s_infMsgTarget;
	extern u32 s_infMsgArg1;
	extern u32 s_infMsgArg2;
}
