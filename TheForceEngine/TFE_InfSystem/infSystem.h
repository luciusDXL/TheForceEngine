#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/levelAsset.h>
#include <TFE_Asset/infAsset.h>
#include <vector>
#include <string>

struct RWall;
struct RSector;
struct SecObject;

enum InfMessageType
{
	IMSG_FREE = 1,		// Internal Only - delete the InfTrigger or InfElevator.
	IMSG_TRIGGER = 7,
	IMSG_NEXT_STOP = 8,
	IMSG_PREV_STOP = 9,
	IMSG_GOTO_STOP = 11,
	IMSG_REVERSE_MOVE = 12,
	IMSG_DONE = 21,
	IMSG_WAKEUP = 25,
	IMSG_MASTER_ON = 29,
	IMSG_MASTER_OFF = 30,
	IMSG_SET_BITS = 31,
	IMSG_CLEAR_BITS = 32,
	IMSG_COMPLETE = 33,
	IMSG_LIGHTS = 34,
};

namespace TFE_InfSystem
{
	bool init();
	void shutdown();

	// For now load the INF data directly.
	// Move back to the asset system later.
	void loadINF(const char* levelName);

	// Per elevator frame update.
	void elevatorUpdate();
	// Per frame animated texture update (this may be moved later).
	void animatedTextureUpdate();

	// Send messages so that entities and the player can interact with the INF system.
	void inf_wallSendMessage(RWall* wall, s32 entity, u32 evt, InfMessageType msgType);
	void inf_sectorSendMessage(RSector* sector, SecObject* obj, u32 evt, InfMessageType msgType);
}
