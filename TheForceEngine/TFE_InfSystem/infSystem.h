#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////
#include "infMessageType.h"
#include <TFE_System/types.h>
#include <TFE_Asset/levelAsset.h>
#include <TFE_Asset/infAsset.h>
#include <vector>
#include <string>

namespace TFE_JediRenderer
{
	struct RWall;
	struct RSector;
	struct SecObject;
}

namespace TFE_InfSystem
{
	bool init();
	void shutdown();

	// For now load the INF data directly.
	// Move back to the asset system later.
	void loadINF(const char* levelName);

	// Per elevator frame update.
	void update_elevators();
	// Per frame animated texture update (this may be moved later).
	void update_animatedTextures();

	// Send messages so that entities and the player can interact with the INF system.
	void inf_wallSendMessage(TFE_JediRenderer::RWall* wall, s32 entity, u32 evt, InfMessageType msgType);
	void inf_sectorSendMessage(TFE_JediRenderer::RSector* sector, TFE_JediRenderer::SecObject* obj, u32 evt, InfMessageType msgType);
}
