#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Agent - handles save game data, level lists, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/filestream.h>

namespace TFE_DarkForces
{
	struct LevelSaveData;

	s32 agent_loadData();
	JBool agent_loadLevelList(const char* fileName);
	JBool agent_readConfigData(FileStream* file, s32 agentId, LevelSaveData* saveData);

	JBool openDarkPilotConfig(FileStream* file);
}  // namespace TFE_DarkForces