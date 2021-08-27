#include "agent.h"
#include "util.h"
#include "player.h"
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
	
	s32 agent_getLevelIndex()
	{
		return s_levelIndex;
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

	void agent_updateAgentSavedData()
	{
		FileStream file;
		if (!openDarkPilotConfig(&file))
		{
			TFE_System::logWrite(LOG_ERROR, "Agent", "Cannot open DarkPilo.cfg");
			return;
		}

		for (s32 i = 0; i < 14; i++)	// i = [ebp-0x08]
		{
			LevelSaveData saveData;
			agent_readConfigData(&file, i, &saveData);

			// Copy Agent data into saved data.
			memcpy(&saveData, &s_agentData[i], sizeof(AgentData));
			agent_writeAgentConfigData(&file, i, &saveData);
		}

		file.close();
	}
		
	JBool agent_loadLevelList(const char* fileName)
	{
		FilePath filePath;
		if (!TFE_Paths::getFilePath(fileName, &filePath))
		{
			return JFALSE;
		}
		char* buffer = nullptr;
		u32 len = FileStream::readContents(&filePath, (void**)&buffer);
		TFE_Parser parser;
		parser.init(buffer, len);
		parser.addCommentString("#");

		size_t bufferPos = 0;
		const char* line = parser.readLine(bufferPos);
		s32 count;
		if (sscanf(line, "LEVELS %d", &count) < 1)
		{
			free(buffer);
			return JFALSE;
		}
		else
		{
			s_maxLevelIndex = count;
			if (count)
			{
				s_levelDisplayNames = (char**)malloc(count * sizeof(char*));
				s_levelGamePaths    = (char**)malloc(count * sizeof(char*));
				s_levelSrcPaths     = (char**)malloc(count * sizeof(char*));
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

		free(buffer);
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