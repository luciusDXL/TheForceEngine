#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Agent - handles save game data, level lists, etc.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/filestream.h>

namespace TFE_DarkForces
{
	enum AgentConstants
	{
		MAX_LEVEL_COUNT = 14,
		MAX_AGENT_COUNT = 14,
	};

#pragma pack(push)
#pragma pack(1)
	struct PilotConfigHeader
	{
		char signature[3];	// "PCF"
		u8   version;		// 18
		s8   count;			// Number of agents.
	};
	struct AgentData
	{
		char name[32];
		s32 selectedMission;
		s32 nextMission;
		u8 difficulty;
		u8 completed[MAX_LEVEL_COUNT];
	};
	struct LevelSaveData
	{
		AgentData agentData;

		// Inventory
		// 32 items * 14 levels = 448 values
		u8 inv[448];

		// Ammo (includes health, shields, energy)
		// 10 items * 14 levels = 140 values (each 4 bytes).
		s32 ammo[140];

		s32 pad;	// unused, but the struct size must stay the same.
	};
#pragma pack(pop)

	void agent_serialize(Stream* stream);
	void agent_restartEndLevelTask();

	s32 agent_loadData();
	JBool agent_loadLevelList(const char* fileName);
	void  agent_updateAgentSavedData();
	void  agent_levelEndTask();
	JBool agent_readConfigData(FileStream* file, s32 agentId, LevelSaveData* saveData);
	JBool agent_writeAgentConfigData(FileStream* file, s32 agentId, const LevelSaveData* saveData);
	void  agent_writeSavedData(s32 agentId, LevelSaveData* saveData);
	void  agent_readSavedData(s32 agentId, LevelSaveData* levelData);
	void  agent_readSavedDataForLevel(s32 agentId, s32 levelIndex);
	void  agent_saveLevelCompletion(u8 diff, s32 levelIndex);
	s32   agent_saveInventory(s32 agentId, s32 nextLevel);
	void  agent_createNewAgent(s32 agentId, AgentData* data, const char* name);

	JBool openDarkPilotConfig(FileStream* file);
	void  agent_setNextLevelByIndex(s32 index);
	s32   agent_getLevelIndex();
	s32   agent_getLevelIndexFromName(const char* name);
	const char* agent_getLevelName();
	const char* agent_getLevelDisplayName();

	void  agent_setLevelComplete(JBool complete);
	JBool agent_getLevelComplete();

	void agent_levelComplete();
	void agent_createLevelEndTask();

	JBool createDarkPilotConfig(const char* path);
	   
	extern AgentData s_agentData[MAX_AGENT_COUNT];
	extern s32 s_maxLevelIndex;
	extern s32 s_agentId;
	extern JBool s_invalidLevelIndex;
	extern JBool s_levelComplete;
	extern char** s_levelDisplayNames;
	extern char** s_levelGamePaths;
	extern char** s_levelSrcPaths;
}  // namespace TFE_DarkForces