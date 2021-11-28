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
#include <assert.h>

namespace TFE_DarkForces
{
	///////////////////////////////////////////
	// Shared State
	///////////////////////////////////////////
	AgentData s_agentData[MAX_AGENT_COUNT];
	JBool s_levelComplete = JFALSE;
	JBool s_invalidLevelIndex;
	s32 s_maxLevelIndex;
	s32 s_levelIndex = -1;	// This is actually s_levelIndex2 in the RE code.
	s32 s_agentId = 0;
	char** s_levelDisplayNames;
	char** s_levelGamePaths;
	char** s_levelSrcPaths;

	static Task* s_levelEndTask = nullptr;
		
	///////////////////////////////////////////
	// API Implementation
	///////////////////////////////////////////
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

	void agent_updateLevelStats()
	{
		s32 curLevel = s_agentData[s_agentId].selectedMission + 1;
		if (curLevel > s_agentData[s_agentId].nextMission)
		{
			s_agentData[s_agentId].nextMission = curLevel;
		}
	}

	void agent_levelComplete()
	{
		s_levelComplete = JTRUE;
		agent_updateLevelStats();
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
		
	const char* agent_getLevelName()
	{
		return s_levelGamePaths[s_levelIndex - 1];
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

	void agent_createNewAgent(s32 agentId, AgentData* data)
	{
		FileStream file;
		if (!openDarkPilotConfig(&file))
		{
			TFE_System::logWrite(LOG_ERROR, "Agent", "Cannot open DarkPilo.cfg");
			return;
		}

		LevelSaveData saveData;
		agent_readConfigData(&file, agentId, &saveData);
		memcpy(&saveData.agentData, data, sizeof(AgentData));

		u8 inv[32] = { 0 };
		inv[0]  = 0xff;
		inv[2]  = 0xff;
		inv[6]  = 0xff;
		inv[30] = WPN_PISTOL;
		inv[31] = 3;

		s32 ammo[10] = { 0 };
		ammo[0] = 100;
		ammo[7] = 100;
		ammo[8] = 100;
		ammo[9] = FIXED(2);

		memset(saveData.inv,  0, 32*14);
		memset(saveData.ammo, 0, sizeof(s32)*10*14);

		memcpy(saveData.inv, inv, 32);
		memcpy(saveData.ammo, ammo, sizeof(s32)*10);

		agent_writeAgentConfigData(&file, agentId, &saveData);
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
		if (!file.open(&filePath, FileStream::MODE_READ))
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
			return JFALSE;
		}
		else
		{
			s_maxLevelIndex = count;
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

			if (displayName[0] && gamePath[0])
			{
				s_levelDisplayNames[i] = copyAndAllocateString(displayName);
				s_levelGamePaths[i]    = copyAndAllocateString(gamePath);
				s_levelSrcPaths[i]     = nullptr;
			}
		}

		game_free(buffer);
		return JTRUE;
	}

	JBool agent_readConfigData(FileStream* file, s32 agentId, LevelSaveData* saveData)
	{
		const s32 dataSize = (s32)sizeof(LevelSaveData);

		// Note the extra 5 is from the 5 byte header.
		s32 offset = 5 + agentId * dataSize;
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
		s32 fileOffset = agentId*sizeof(LevelSaveData) + 5;
		if (!file->seek(fileOffset))
		{
			return JFALSE;
		}
		file->writeBuffer(saveData, sizeof(LevelSaveData));
		return JTRUE;
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
		if (!darkPilot.open(path, FileStream::MODE_WRITE))
		{
			return JFALSE;
		}

		u8 header[] = { 'P', 'C', 'F', 0x12, 0x0e };
		LevelSaveData clearData = { 0 };
		darkPilot.write(header, 5);
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
		assert(file);

		// TFE uses its own local copy of the save game data to avoid corrupting existing data.
		// If this copy does not exist, then copy it.
		char programDataPath[TFE_MAX_PATH];
		char sourcePath[TFE_MAX_PATH];
		TFE_Paths::appendPath(PATH_PROGRAM_DATA, "DARKPILO.CFG", programDataPath);
		if (!FileUtil::exists(programDataPath))
		{
			TFE_Paths::appendPath(PATH_SOURCE_DATA, "DARKPILO.CFG", sourcePath);
			if (FileUtil::exists(sourcePath))
			{
				FileUtil::copyFile(sourcePath, programDataPath);
			}
			else
			{
				TFE_System::logWrite(LOG_WARNING, "DarkForcesMain", "Cannot find 'DARKPILO.CFG' at '%s'. Creating a new file for save data.", sourcePath);
				createDarkPilotConfig(programDataPath);
			}
		}
		// Then try opening the file.
		if (!file->open(programDataPath, FileStream::MODE_READWRITE))
		{
			return JFALSE;
		}
		// Then verify the file.
		u8 buffer[5];
		file->readBuffer(buffer, 5);
		if (buffer[3] == 0x12 && buffer[4] == 0x0e && strncasecmp((char*)buffer, "PCF", 3) == 0)
		{
			return JTRUE;
		}
		// If it is not correct, then close the file and return false.
		file->close();
		return JFALSE;
	}
}  // namespace TFE_DarkForces