#pragma once
//////////////////////////////////////////////////////////////////////
// Note this is still the original TFE code, reverse-engineered game
// UI code will not be available until future releases.
//////////////////////////////////////////////////////////////////////

#include <TFE_DarkForces/util.h>
#include <TFE_System/types.h>

namespace TFE_DarkForces
{
	// Return True if the menu is still running.
	// levelIndex will hold the selected level index (1 - 14).
	JBool agentMenu_update(s32* levelIndex);

	void agentMenu_load(LangHotkeys* hotkeys);

	// Reset Presistent State.

	s32 agentMenu_getAgentID();
	void agentMenu_setAgentId(s32 id);
	void agentMenu_setAgentCount(s32 count);
	s32 agentMenu_getAgentCount();
	void agentMenu_createNewAgent();
	void agentMenu_setAgentName(const char* name);
	void agentMenu_resetState();
}
