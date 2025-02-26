#include <cstring>

#include "agent.h"
#include "util.h"
#include "player.h"
#include "hud.h"
#include "weapon.h"
#include <TFE_Game/igame.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <assert.h>

namespace TFE_DarkForces
{
	///////////////////////////////////////////
	// Shared State
	///////////////////////////////////////////
	AgentData s_agentData[MAX_AGENT_COUNT];
	JBool s_levelComplete = JFALSE;
	JBool s_invalidLevelIndex;
	s32 s_maxLevelIndex = 0;
	s32 s_levelIndex = -1;	// This is actually s_levelIndex2 in the RE code.
	s32 s_agentId = 0;
	s32 s_headerSize = (s32)sizeof(PilotConfigHeader);
	char** s_levelDisplayNames;
	char** s_levelGamePaths;
	char** s_levelSrcPaths;

	static Task* s_levelEndTask = nullptr;

	void agent_writeSavedData(s32 agentId, LevelSaveData* saveData);
	void agent_readSavedData(s32 agentId, LevelSaveData* levelData);
		
	///////////////////////////////////////////
	// API Implementation
	///////////////////////////////////////////
	void agent_checkNameLen(u8& nameLen)
	{
		if (nameLen >= 32)
		{
			TFE_System::logWrite(LOG_ERROR, "Agent", "Agent load - name is too long: %d / 32", nameLen);
			assert(0);
			nameLen = 31;
		}
	}

	void agent_restartEndLevelTask()
	{
		// Add the level complete task.
		if (s_levelComplete)
		{
			agent_createLevelEndTask();
		}
	}

	void agent_serialize(Stream* stream)
	{
		bool write = serialization_getMode() == SMODE_WRITE;

		SERIALIZE(SaveVersionInit, s_levelComplete, JFALSE);
		SERIALIZE(SaveVersionInit, s_invalidLevelIndex, JFALSE);
		SERIALIZE(SaveVersionInit, s_maxLevelIndex, JFALSE);
		SERIALIZE(SaveVersionInit, s_levelIndex, JFALSE);
				
		// Agent.
		char name[64] = { 0 };
		u8 agentNameLen = 0;
		if (write)
		{
			strcpy(name, s_agentData[s_agentId].name);
			name[32] = 0;	// Just in case.

			agentNameLen = (u8)strlen(name);
			agent_checkNameLen(agentNameLen);
		}
		SERIALIZE(SaveVersionInit, agentNameLen, 0);
		if (!write)
		{
			agent_checkNameLen(agentNameLen);
		}
		SERIALIZE_BUF(SaveVersionInit, name, agentNameLen);
		name[agentNameLen] = 0;

		// Agent data.
		LevelSaveData data;
		if (write)
		{
			agent_readSavedData(s_agentId, &data);
		}
		SERIALIZE(SaveVersionInit, data, { 0 });

		// Find the agent.
		if (!write)
		{
			s_agentId = -1;
			// First get the count.
			s32 agentCount = 0;
			for (s32 i = 0; i < 14; i++)
			{
				if (!s_agentData[i].name[0]) { break; }
				agentCount++;
			}
			// Next find the agent, if it exists.
			for (s32 i = 0; i < agentCount; i++)
			{
				if (strcmp(s_agentData[i].name, name) == 0)
				{
					s_agentId = i;
					break;
				}
			}
			// If the agent doesn't exist, create it.
			if (s_agentId < 0)
			{
				if (agentCount < 14)
				{
					// Create a new agent.
					s_agentData[agentCount] = data.agentData;
					s_agentId = agentCount;
					agent_writeSavedData(s_agentId, &data);
				}
				else
				{
					TFE_System::logWrite(LOG_ERROR, "Agent", "Too many agents - cannot create a new agent from save.");
					assert(0);
					s_agentId = 0;
				}
			}
			else
			{
				// Check the agent and make sure it will work...
				if (s_agentData[s_agentId].nextMission < data.agentData.nextMission)
				{
					TFE_System::logWrite(LOG_WARNING, "Agent", "The agent in the save file has completed more levels than the local agent, updating.");
					s_agentData[s_agentId] = data.agentData;
					agent_writeSavedData(s_agentId, &data);
				}
			}
		}
	}

	s32 agent_loadData()
	{
		FileStream file;
		if (!openDarkPilotConfig(&file))
		{
			TFE_System::logWrite(LOG_ERROR, "Agent", "Cannot open DarkPilo.cfg");
			return 0;
		}

		s32 agentReadCount = 0;
		for (s32 i = 0; i < MAX_AGENT_COUNT; i++)
		{
			LevelSaveData saveData;
			if (agent_readConfigData(&file, i, &saveData))
			{
				memcpy(&s_agentData[i], &saveData.agentData, sizeof(AgentData));
				agentReadCount++;
			}
		}

		file.close();
		return agentReadCount;
	}

	void agent_levelComplete()
	{
		s_levelComplete = JTRUE;

		s32 curLevel = s_agentData[s_agentId].selectedMission + 1;
		if (curLevel > s_agentData[s_agentId].nextMission)
		{
			s_agentData[s_agentId].nextMission = curLevel;
		}
	}
		
	void levelEndTaskFunc(MessageType msg)
	{
		task_begin;
		while (1)
		{
			hud_sendTextMessage(461);
			task_yield(582);			// ~4 seconds
			hud_sendTextMessage(462);
			task_yield(4369);			// ~30 seconds
		}
		task_end;
	}

	void agent_levelEndTask()
	{
		s_levelEndTask = nullptr;
	}

	void agent_createLevelEndTask()
	{
		if (!s_levelEndTask)
		{
			s_levelEndTask = createSubTask("LevelEnd", levelEndTaskFunc);
		}
	}

	void agent_saveLevelCompletion(u8 diff, s32 levelIndex)
	{
		s_agentData[s_agentId].completed[levelIndex - 1] = diff;
	}
	
	s32 agent_getLevelIndex()
	{
		return s_levelIndex;
	}

	s32 agent_getLevelIndexFromName(const char* name)
	{
		for (s32 i = 0; i < s_maxLevelIndex; i++)
		{
			if (!strcasecmp(s_levelGamePaths[i], name))
			{
				return i + 1;
			}
		}
		return 0;
	}
		
	const char* agent_getLevelName()
	{
		if (!s_maxLevelIndex) { return nullptr; }
		const s32 index = clamp(s_levelIndex, 1, s_maxLevelIndex);
		return s_levelGamePaths[index - 1];
	}

	const char* agent_getLevelDisplayName()
	{
		if (!s_maxLevelIndex) { return nullptr; }
		const s32 index = clamp(s_levelIndex, 1, s_maxLevelIndex);
		return s_levelDisplayNames[index - 1];
	}

	void  agent_setLevelComplete(JBool complete)
	{
		s_levelComplete = complete;
	}

	JBool agent_getLevelComplete()
	{
		return s_levelComplete;
	}

	void agent_setNextLevelByIndex(s32 index)
	{
		if (index <= s_maxLevelIndex)
		{
			s_levelIndex = index;
			s_agentData[s_agentId].selectedMission = index;
		}
	}

	void agent_createNewAgent(s32 agentId, AgentData* data, const char* name)
	{
		FileStream file;
		if (!openDarkPilotConfig(&file))
		{
			TFE_System::logWrite(LOG_ERROR, "Agent", "Cannot open DarkPilo.cfg");
			return;
		}

		LevelSaveData saveData;
		memset(&saveData, 0, sizeof(LevelSaveData));
		memset(data, 0, sizeof(AgentData));

		saveData.inv[0]  = 0xff;			// bryar pistol
		saveData.inv[2]  = 0xff;			// s_itemUnknown1
		saveData.inv[6]  = 0xff;			// s_itemUnknown2
		saveData.inv[30] = WPN_PISTOL;		// current weapon
		saveData.inv[31] = 3;				// lives

		saveData.ammo[0] = min(s_ammoEnergyMax, 100);	// energy
		saveData.ammo[7] = 100;				// shields
		saveData.ammo[8] = 100;				// health
		saveData.ammo[9] = FIXED(2);		// battery

		strCopyAndZero(data->name, name, 32);
		data->difficulty = 1;
		data->nextMission = 1;
		data->selectedMission = 1;
		memcpy(&saveData.agentData, data, sizeof(AgentData));

		agent_writeAgentConfigData(&file, agentId, &saveData);
		file.close();
	}

	void agent_writeSavedData(s32 agentId, LevelSaveData* saveData)
	{
		FileStream file;
		if (!openDarkPilotConfig(&file))
		{
			TFE_System::logWrite(LOG_ERROR, "Agent", "Cannot open DarkPilo.cfg");
			return;
		}
		agent_writeAgentConfigData(&file, agentId, saveData);
		file.close();
	}

	void agent_updateAgentSavedData()
	{
		FileStream file;
		if (!openDarkPilotConfig(&file))
		{
			TFE_System::logWrite(LOG_ERROR, "Agent", "Cannot open DarkPilo.cfg");
			return;
		}

		for (s32 i = 0; i < MAX_AGENT_COUNT; i++)
		{
			LevelSaveData saveData;
			agent_readConfigData(&file, i, &saveData);

			// Copy Agent data into saved data.
			memcpy(&saveData, &s_agentData[i], sizeof(AgentData));
			agent_writeAgentConfigData(&file, i, &saveData);
		}

		file.close();
	}

	s32 agent_saveInventory(s32 agentId, s32 nextLevel)
	{
		if (nextLevel > 14) { return 0; }

		FileStream file;
		if (!openDarkPilotConfig(&file)) { return 0; }

		LevelSaveData saveData;
		if (!agent_readConfigData(&file, agentId, &saveData))
		{
			file.close();
			return 0;
		}

		s32 levelIndex = nextLevel - 1;
		s32* ammo = &saveData.ammo[levelIndex * 10];
		u8* inv = &saveData.inv[levelIndex * 32];
		player_writeInfo(inv, ammo);
		s32 written = agent_writeAgentConfigData(&file, agentId, &saveData);
		file.close();

		return written;
	}

	JBool agent_loadLevelList(const char* fileName)
	{
		FilePath filePath;
		if (!TFE_Paths::getFilePath(fileName, &filePath))
		{
			return JFALSE;
		}
		char* buffer = nullptr;
		FileStream file;
		if (!file.open(&filePath, Stream::MODE_READ))
		{
			return JFALSE;
		}
		u32 len = (u32)file.getSize();
		buffer = (char*)game_alloc(len+1);
		file.readBuffer(buffer, len);
		file.close();
		buffer[len] = 0;

		TFE_Parser parser;
		parser.init(buffer, len);
		parser.addCommentString("#");

		size_t bufferPos = 0;
		const char* line = parser.readLine(bufferPos);
		s32 count;
		if (sscanf(line, "LEVELS %d", &count) < 1)
		{
			game_free(buffer);
			s_maxLevelIndex = 0;
			return JFALSE;
		}
		else
		{
			s_maxLevelIndex = min(count, MAX_LEVEL_COUNT);
			if (count)
			{
				s_levelDisplayNames = (char**)game_alloc(count * sizeof(char*));
				s_levelGamePaths    = (char**)game_alloc(count * sizeof(char*));
				s_levelSrcPaths     = (char**)game_alloc(count * sizeof(char*));
			}
		}

		for (s32 i = 0; i < count; i++)
		{
			line = parser.readLine(bufferPos);
			char* displayName = strtok((char*)line, ",");
			char* gamePath    = strtok(nullptr, ", \t\n\r");
			char* srcPath     = strtok(nullptr, ", \t\n\r");

			s_levelDisplayNames[i] = nullptr;
			s_levelGamePaths[i] = nullptr;
			s_levelSrcPaths[i] = nullptr;

			if (displayName && gamePath)
			{
				if (displayName[0])
				{
					s_levelDisplayNames[i] = copyAndAllocateString(displayName);
				}
				if (gamePath[0])
				{
					s_levelGamePaths[i] = copyAndAllocateString(gamePath);
				}
			}
		}

		game_free(buffer);
		return JTRUE;
	}

	JBool agent_readConfigData(FileStream* file, s32 agentId, LevelSaveData* saveData)
	{
		const s32 dataSize = (s32)sizeof(LevelSaveData);

		s32 offset = s_headerSize + agentId * dataSize;
		if (!file->seek(offset))
		{
			return JFALSE;
		}
		if (file->readBuffer(saveData, dataSize) != dataSize)
		{
			return JFALSE;
		}
		return JTRUE;
	}

	JBool agent_writeAgentConfigData(FileStream* file, s32 agentId, const LevelSaveData* saveData)
	{
		s32 fileOffset = agentId*sizeof(LevelSaveData) + s_headerSize;
		if (!file->seek(fileOffset))
		{
			return JFALSE;
		}
		file->writeBuffer(saveData, sizeof(LevelSaveData));
		return JTRUE;
	}

	void agent_readSavedData(s32 agentId, LevelSaveData* levelData)
	{
		FileStream file;
		if (!openDarkPilotConfig(&file))
		{
			TFE_System::logWrite(LOG_ERROR, "Agent", "Cannot open DarkPilo.cfg");
			return;
		}
		agent_readConfigData(&file, agentId, levelData);
		file.close();
	}

	void agent_readSavedDataForLevel(s32 agentId, s32 levelIndex)
	{
		FileStream file;
		if (!openDarkPilotConfig(&file))
		{
			TFE_System::logWrite(LOG_ERROR, "Agent", "Cannot open DarkPilo.cfg");
			return;
		}
		LevelSaveData levelData;
		agent_readConfigData(&file, agentId, &levelData);
		file.close();

		s32* ammo = &levelData.ammo[(levelIndex - 1) * 10];
		u8* inv = &levelData.inv[(levelIndex - 1) * 32];
		player_readInfo(inv, ammo);
	}

	// Creates a new Dark Pilot config file, which is used for saving.
	JBool createDarkPilotConfig(const char* path)
	{
		FileStream darkPilot;
		if (!darkPilot.open(path, Stream::MODE_WRITE))
		{
			return JFALSE;
		}

		PilotConfigHeader header =
		{
			{'P', 'C', 'F'},	// signature.
			0x12,				// version used by Dark Forces.
			14,					// maximum number of agents.
		};
		darkPilot.writeBuffer(&header, s_headerSize);
		LevelSaveData clearData = { 0 };
		for (s32 i = 0; i < MAX_AGENT_COUNT; i++)
		{
			darkPilot.writeBuffer(&clearData, sizeof(LevelSaveData));
		}
		darkPilot.close();
		return JTRUE;
	}
		
	// This function differs slightly from Dark Forces in the following ways:
	// 1. First it tries to open the CFG file from PATH_PROGRAM_DATA/DarkPilot.cfg
	// 2. If (1) fails, it will then attempt to copy the file from PATH_SOURCE/DarkPilot.cfg to PATH_PROGRAM_DATA/DarkPilot.cfg
	// 3. Instead of returning a handle, the caller passes in a FileStream pointer to be filled in.
	// This is done so that the original data cannot be corrupted by a potentially buggy Alpha build.
	// Also, later, the TFE based save format will likely change - so importing will be necessary anyway.
	JBool openDarkPilotConfig(FileStream* file)
	{
		bool triedonce = false;
		assert(file);

		// TFE uses its own local copy of the save game data to avoid corrupting existing data.
		// If this copy does not exist, then copy it.
		char documentsPath[TFE_MAX_PATH];
		char programDataPath[TFE_MAX_PATH];
		char sourcePath[TFE_MAX_PATH];
		TFE_Paths::appendPath(PATH_USER_DOCUMENTS, "DARKPILO.CFG", documentsPath);
		if (!FileUtil::exists(documentsPath))
		{
			// First check in /ProgramData since that is where the previous version stored it.
			TFE_Paths::appendPath(PATH_PROGRAM_DATA, "DARKPILO.CFG", programDataPath);
			if (FileUtil::exists(programDataPath))
			{
				TFE_System::logWrite(LOG_WARNING, "DarkForcesMain", "'DARKPILO.CFG' copied from '%s' to '%s', ProgramData/ will no longer be used and can be deleted.",
					programDataPath, documentsPath);
				FileUtil::copyFile(programDataPath, documentsPath);
				// Cleanup after TFE.
				FileUtil::deleteFile(programDataPath);
			}
			else
			{
				// Then try the source data path.
				TFE_Paths::appendPath(PATH_SOURCE_DATA, "DARKPILO.CFG", sourcePath);
				if (FileUtil::exists(sourcePath))
				{
					FileUtil::copyFile(sourcePath, documentsPath);
				}
				else
				{
					// Finally generate a new one.
					TFE_System::logWrite(LOG_WARNING, "DarkForcesMain", "Cannot find 'DARKPILO.CFG' at '%s'. Creating a new file for save data.", sourcePath);
newpilo:
					createDarkPilotConfig(documentsPath);
				}
			}
		}
		// Then try opening the file.
		if (!file->open(documentsPath, Stream::MODE_READWRITE))
		{
			TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "cannot open DARKPILO.CFG");
			return JFALSE;
		}
		// Then verify the file.
		PilotConfigHeader header;
		file->readBuffer(&header, sizeof(PilotConfigHeader));
		if (header.version == 0x12 && header.count == 14 && strncasecmp(header.signature, "PCF", 3) == 0)
		{
			return JTRUE;
		}
		// If it is not correct, then close the file and return false.
		file->close();
		if (!triedonce)
		{
			TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "DARKPILO.CFG corrupted; creating new");
			triedonce = true;
			goto newpilo;
		}
		return JFALSE;
	}
}  // namespace TFE_DarkForces
