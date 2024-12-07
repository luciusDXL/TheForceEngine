#include <TFE_Input/replay.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/filestream.h>
#include <version.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <sstream>
#include <unordered_map>
#include <windows.h>
#include <thread>
#include <TFE_Input/inputmapping.h>
#include <array>
#include <map>
#include <TFE_Settings/settings.h>
#include <string>
#include <TFE_DarkForces/random.h>
#include <TFE_DarkForces/time.h>
#include <TFE_DarkForces/player.h>
#include <TFE_Game/saveSystem.h>
#include <TFE_DarkForces/darkForcesMain.h>
#include <TFE_FrontEndUI/frontEndUi.h>

namespace TFE_Input
{
	// Handle replay File Paths
	FileStream s_replayFile;
	static char s_replayDir[TFE_MAX_PATH];
	static char s_replayPath[TFE_MAX_PATH];

	// Recording States
	static bool s_recording = false;
	static bool s_playback = false;
	static bool eyeSet = false;

	angle14_16 r_yaw = 0;
	angle14_16 r_pitch = 0;
	angle14_16 r_roll = 0;

	// Main storage of event structures. 
	std::unordered_map<int, ReplayEvent> inputEvents;
	u64 replayStartTime = 0;
	s32 replay_seed = 0;

	void initReplays()
	{
		sprintf(s_replayDir, "%sReplays/", TFE_Paths::getPath(PATH_USER_DOCUMENTS));
		TFE_Paths::fixupPathAsDirectory(s_replayDir);

		if (!FileUtil::directoryExits(s_replayDir))
		{
			FileUtil::makeDirectory(s_replayDir);
		}
		sprintf(s_replayPath, "%stest.demo", s_replayDir);
	}

	bool isRecording()
	{
		return s_recording;
	}

	void setRecording(bool recording)
	{
		s_recording = recording;
	}

	bool isDemoPlayback()
	{
		return s_playback;
	}

	void setDemoPlayback(bool playback)
	{
		s_playback = playback;
	}

	void saveTick()
	{
		int inputCounter = getCounter();		
		inputEvents[inputCounter].curTick = TFE_DarkForces::s_curTick;
	}

	

	void loadTick()
	{
		int inputCounter = getCounter();
		TFE_DarkForces::s_curTick = inputEvents[inputCounter].curTick;		
	}

	void recordReplaySeed()
	{
		replay_seed = TFE_DarkForces::getSeed();
		TFE_System::logWrite(LOG_MSG, "REPLAY", "Recording Seed: %d", replay_seed);
	}


	void restoreReplaySeed()
	{
		TFE_DarkForces::setSeed(replay_seed);
		TFE_System::logWrite(LOG_MSG, "REPLAY", "Loading Seed: %d", replay_seed);
	}

	void clearEvents()
	{
		int iSize = inputEvents.size();
		for (int i = 0; i < iSize; i++)
		{
			inputEvents[i].clear();
		}
		inputEvents.clear();
	}

	string convertToString(vector<s32> keysDown)
	{
		std::ostringstream oss;
		for (size_t i = 0; i < keysDown.size(); ++i) {
			if (i != 0) oss << ",";
			oss << keysDown[i];
		}
		return oss.str();
	}

	vector<s32> convertFromString(string keyStr)
	{
		std::vector<s32> intVector;
		std::istringstream iss(keyStr);
		std::string token;
		while (std::getline(iss, token, ',')) {
			intVector.push_back(std::stoi(token));
		};
		return intVector;
	}

	vector<s32> serializeInputs(Stream* stream, vector<s32> inputList, bool writeFlag)
	{
		int keySize = 0;
		string keyString;

		if (writeFlag)
		{
			keyString = convertToString(inputList);
			if (keyString.size() == 0)
			{
				int x;
			}
			keySize = keyString.size();
		}

		SERIALIZE(ReplayVersionInit, keySize, 0);
		if (!writeFlag)
		{
			keyString.resize(keySize);
		}
		SERIALIZE_BUF(ReplayVersionInit, &keyString[0], keySize);


		if (!writeFlag && keySize > 0)
		{
			return convertFromString(keyString);
		}
		return inputList;
	}

	bool loadReplayHeader(const char* filename, TFE_SaveSystem::SaveHeader* header)
	{
		char filePath[TFE_MAX_PATH];
		sprintf(filePath, "%s%s", s_replayDir, filename);

		bool ret = false;
		FileStream stream;
		if (stream.open(filePath, Stream::MODE_READ))
		{
			loadHeader(&stream, header, filename);
			strcpy(header->fileName, filename);
			stream.close();
			ret = true;
		}
		return ret;
	}

	void populateReplayDirectory(std::vector<TFE_SaveSystem::SaveHeader>& dir)
	{
		dir.clear();
		FileList fileList;
		FileUtil::readDirectory(s_replayDir, "demo", fileList);
		size_t saveCount = fileList.size();
		dir.resize(saveCount);

		const std::string* filenames = fileList.data();
		TFE_SaveSystem::SaveHeader* headers = dir.data();
		for (size_t i = 0; i < saveCount; i++)
		{
			loadReplayHeader(filenames[i].c_str(), &headers[i]);
		}
	}

	void serializeDemo(Stream* stream, bool writeFlag)
	{
		int fileHandler = 0;
		int updateCounter = 0;
		TFE_DarkForces::time_pause(JTRUE);
		if (writeFlag)
		{
			serialization_setMode(SMODE_WRITE);
			fileHandler = s_replayFile.open(s_replayPath, Stream::MODE_WRITE);
			replayStartTime = TFE_System::getStartTime();
			TFE_SaveSystem::saveHeader(stream, "TEST");

			//void saveHeader(Stream * stream, const char* saveName);
			//void loadHeader(Stream * stream, SaveHeader * header, const char* fileName);
		}
		else
		{
			serialization_setMode(SMODE_READ);
			fileHandler = s_replayFile.open(s_replayPath, Stream::MODE_READ);
			clearEvents();
			TFE_SaveSystem::SaveHeader * header = new TFE_SaveSystem::SaveHeader();
			TFE_SaveSystem::loadHeader(stream, header, "TEST");
		}

		if (fileHandler > 0)
		{
			SERIALIZE_VERSION(ReplayVersionInit);
			SERIALIZE(ReplayVersionInit, replayStartTime, 0);
			SERIALIZE(ReplayVersionInit, replay_seed, 0);

			u32 plTick = TFE_DarkForces::s_playerTick;
			u32 plPrevTick = TFE_DarkForces::s_prevPlayerTick;
			SERIALIZE(ReplayVersionInit, plTick, 0);
			SERIALIZE(ReplayVersionInit, plPrevTick, 0);

			TFE_DarkForces::time_serialize(stream);

			int inputListsSize = getCounter();
			SERIALIZE(ReplayVersionInit, inputListsSize, 0);

			SERIALIZE(ReplayVersionInit, r_yaw, 0);
			SERIALIZE(ReplayVersionInit, r_pitch, 0);
			SERIALIZE(ReplayVersionInit, r_roll, 0);

			if (writeFlag)
			{

				// Loop through inputEvents	
				for (const auto& pair : inputEvents)
				{
					updateCounter = pair.first;
					
					if (updateCounter > inputListsSize) continue;
					SERIALIZE(ReplayVersionInit, updateCounter, 0);
					serializeInputs(stream, pair.second.keysDown, writeFlag);
					serializeInputs(stream, pair.second.keysPressed, writeFlag);
					serializeInputs(stream, pair.second.mouseDown, writeFlag);
					serializeInputs(stream, pair.second.mousePos, writeFlag);
					serializeInputs(stream, pair.second.mouseWheel, writeFlag);

					u32 curTick = pair.second.curTick;
					SERIALIZE(ReplayVersionInit, curTick, 0);					
				}
			}
			else
			{

				TFE_DarkForces::s_playerTick = plTick;
				TFE_DarkForces::s_prevPlayerTick = plPrevTick;

				for (int i = 0; i < inputListsSize; i++)
				{
					SERIALIZE(ReplayVersionInit, updateCounter, 0);
					ReplayEvent event = {};
					event.keysDown = serializeInputs(stream, event.keysDown, writeFlag);
					event.keysPressed = serializeInputs(stream, event.keysPressed, writeFlag);
					event.mouseDown = serializeInputs(stream, event.mouseDown, writeFlag);
					event.mousePos = serializeInputs(stream, event.mousePos, writeFlag);
					event.mouseWheel = serializeInputs(stream, event.mouseWheel, writeFlag);
					

					u32 curTick = 0;
					SERIALIZE(ReplayVersionInit, curTick, 0);
					event.curTick = curTick;					
					inputEvents[updateCounter] = event;
				}
				resetCounter();
				setMaxInputCounter(updateCounter);
			}

			s_replayFile.close();
		}

		if (!writeFlag)
		{
			TFE_System::setStartTime(replayStartTime);
		}
		TFE_DarkForces::time_pause(JFALSE);
	}

	void handleEye()
	{
		if (!eyeSet)
		{
			if (isRecording())
			{
				r_yaw = TFE_DarkForces::s_playerEye->yaw;
				r_pitch = TFE_DarkForces::s_playerEye->pitch;
				r_roll = TFE_DarkForces::s_playerEye->roll;
			}
			if (isDemoPlayback())
			{
				TFE_DarkForces::s_playerEye->yaw = r_yaw;
				TFE_DarkForces::s_playerEye->pitch = r_pitch;
				TFE_DarkForces::s_playerEye->roll = r_roll;
			}
			eyeSet = true;
		}
	}

	void startRecording()
	{		
		if (FileUtil::exists(s_replayPath))
		{
			FileUtil::deleteFile(s_replayPath);
		}
		eyeSet = false;
		clearEvents();
		recordReplaySeed();
		inputMapping_endFrame();
		s_recording = true;		
		setDemoPlayback(false);
		resetCounter();
		saveTick();
		setMaxInputCounter(0);
	}

	void endRecording()
	{
		s_recording = false;
		FileStream* stream = &s_replayFile;
		serializeDemo(&s_replayFile, true);
		inputMapping_endFrame();
	}

	void recordReplayTime(u64 startTime)
	{
		replayStartTime = startTime;
	}

	void loadReplayWrapper(string replayFile, string modName, string levelName)
	{
		string x = replayFile; 
		string y = modName;
		string z = levelName;
		TFE_Settings::getGameSettings()->df_enableRecording = true;
		//IGame* curGame =  TFE_FrontEndUI::getCurrentGame();
		//curGame->enable Cutscenes(JFALSE);
		TFE_DarkForces::enableCutscenes(JFALSE);

	}

	void loadReplay()
	{
		if (TFE_Settings::getGameSettings()->df_enableRecording) return;
		eyeSet = false;
		clearEvents();
		resetCounter();
		setCounter(1);
		inputMapping_endFrame();
		FileStream* stream = &s_replayFile;
		serializeDemo(&s_replayFile, false);
		restoreReplaySeed();
		setDemoPlayback(true);
		TFE_DarkForces::s_curTick = inputEvents[0].curTick;
		TFE_DarkForces::s_prevTick = inputEvents[0].prevTick;
		TFE_DarkForces::s_deltaTime = inputEvents[0].deltaTime;
		//memcpy(TFE_DarkForces::s_frameTicks, inputEvents[0].frameTicks, sizeof(fixed16_16) * TFE_ARRAYSIZE(inputEvents[1].frameTicks));
	}
}