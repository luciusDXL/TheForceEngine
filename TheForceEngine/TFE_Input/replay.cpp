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

namespace TFE_Input
{
	static char s_replayDir[TFE_MAX_PATH];
	static char s_replayPath[TFE_MAX_PATH];
	FileStream s_replayFile;
	static bool s_recording = false;
	static bool s_playback = false;
	static Tick replayCurTick;
	static Tick replayPrevTick;
	static f64 replayTimeAccum = 0.0;
	static fixed16_16 replayDeltaTime;
	static fixed16_16 replayFrameTicks[13] = { 0 };
	angle14_16 r_yaw = 0;
	angle14_16 r_pitch = 0;
	angle14_16 r_roll = 0;


	std::unordered_map<int, ReplayEvent> inputEvents;
	u64 s_startTime = 0;
	s32 replay_seed = 0;
	int tickCounter = 0;

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

	void recordTiming()
	{
		TFE_DarkForces::time_pause(JTRUE);
		/*replayCurTick = TFE_DarkForces::s_curTick;
		replayPrevTick = TFE_DarkForces::s_prevTick;
		replayTimeAccum = TFE_DarkForces::getTimeAccum();;
		replayDeltaTime = TFE_DarkForces::s_deltaTime;
		memcpy(replayFrameTicks, TFE_DarkForces::s_frameTicks, sizeof(fixed16_16) * TFE_ARRAYSIZE(TFE_DarkForces::s_frameTicks));
		*/
		recordReplaySeed();
		sTick(TFE_DarkForces::s_curTick);
		TFE_DarkForces::time_pause(JFALSE);
	}

	void loadTiming()
	{
		TFE_DarkForces::time_pause(JTRUE);
		/*
		TFE_DarkForces::s_curTick = replayCurTick;
		TFE_DarkForces::s_prevTick = replayPrevTick;
		TFE_DarkForces::setTimeAccum(replayTimeAccum);
		TFE_DarkForces::s_deltaTime = replayDeltaTime;
		memcpy(TFE_DarkForces::s_frameTicks, replayFrameTicks, sizeof(fixed16_16) * TFE_ARRAYSIZE(replayFrameTicks));
		*/
		restoreReplaySeed();
		TFE_DarkForces::s_curTick = lTick();
		TFE_DarkForces::time_pause(JFALSE);
	}

	void sTick(Tick curTick)
	{
		int inputCounter = getCounter();
		inputEvents[inputCounter].curTick = curTick;
		//TFE_System::logWrite(LOG_MSG, "Progam Flow", "STICK curtick = %d counter = %d ", curTick, inputCounter);

	}

	void saveTick()
	{
		int inputCounter = getCounter();		
		//TFE_System::logWrite(LOG_MSG, "Progam Flow", "SAVETICK curtick = %d prevtick = %d accum = %d delta = %d", get<0>(tData), get<1>(tData), get<2>(tData), get<3>(tData));
		if (inputCounter > 0)
		{
			inputEvents[inputCounter - 1].curTick = TFE_DarkForces::s_curTick;
		}
		else
		{
			inputEvents[inputCounter].curTick = TFE_DarkForces::s_curTick;
		}
		//inputEvents[inputCounter].prevTick = TFE_DarkForces::s_prevTick;
		//inputEvents[inputCounter].deltaTime = TFE_DarkForces::s_deltaTime;
		//inputEvents[inputCounter].timeAccum = TFE_DarkForces::getTimeAccum();
		//memcpy(inputEvents[inputCounter].frameTicks, TFE_DarkForces::s_frameTicks, sizeof(fixed16_16) * TFE_ARRAYSIZE(TFE_DarkForces::s_frameTicks));
	}

	Tick lTick()
	{
		int inputCounter = getCounter();
		Tick curTick = inputEvents[inputCounter].curTick;
		//TFE_System::logWrite(LOG_MSG, "Progam Flow", "LTICK curtick = %d counter = %d ", curTick, inputCounter);
		return curTick;
	}

	void loadTick()
	{
		int inputCounter = getCounter();
		TFE_DarkForces::s_curTick = inputEvents[inputCounter].curTick;
		//TFE_DarkForces::s_prevTick = inputEvents[inputCounter].prevTick;
		//TFE_DarkForces::setTimeAccum(inputEvents[inputCounter].timeAccum);
		//TFE_DarkForces::s_deltaTime = inputEvents[inputCounter].deltaTime;
		//memcpy(TFE_DarkForces::s_frameTicks, inputEvents[inputCounter].frameTicks, sizeof(fixed16_16) * TFE_ARRAYSIZE(inputEvents[inputCounter].frameTicks));
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

	void serializeDemo(Stream* stream, bool writeFlag)
	{
		int fileHandler = 0;
		int updateCounter = 0;
		TFE_DarkForces::time_pause(JTRUE);
		if (writeFlag)
		{
			serialization_setMode(SMODE_WRITE);
			fileHandler = s_replayFile.open(s_replayPath, Stream::MODE_WRITE);
		}
		else
		{
			serialization_setMode(SMODE_READ);
			fileHandler = s_replayFile.open(s_replayPath, Stream::MODE_READ);
			inputEvents.clear();
		}





		if (fileHandler > 0)
		{
			SERIALIZE_VERSION(ReplayVersionInit);
			SERIALIZE(ReplayVersionInit, s_startTime, 0);
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

					//TFE_System::logWrite(LOG_MSG, "STORING", "upd = %d wheelsize = %d", updateCounter, pair.second.mouseWheel.size());
					
					serializeInputs(stream, pair.second.mouseWheel, writeFlag);

					u32 curTick = pair.second.curTick;
				//	TFE_System::logWrite(LOG_MSG, "STORING", "STORING CURTIME %d COUNTER %d", curTick, updateCounter);
					SERIALIZE(ReplayVersionInit, curTick, 0);
					/*
					std::tuple<Tick, Tick, f64, fixed16_16> tData = pair.second.timeData;
					u32 curTick = std::get<0>(tData);
					u32 prevTick = std::get<1>(tData);
					f64 timeAccum = std::get<2>(tData);
					fixed16_16 deltaTime = std::get<3>(tData);
					SERIALIZE(ReplayVersionInit, curTick, 0);
					SERIALIZE(ReplayVersionInit, prevTick, 0);
					SERIALIZE(ReplayVersionInit, timeAccum, 0.0);
					SERIALIZE(ReplayVersionInit, deltaTime, 0);*/

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
					//TFE_System::logWrite(LOG_MSG, "LOADING", "LOADING CURTIME %d COUNTER %d", curTick, updateCounter);
					event.curTick = curTick;
					/*
					u32 curTick;
					u32 prevTick;
					f64 timeAccum;
					fixed16_16 deltaTime;

					SERIALIZE(ReplayVersionInit, curTick, 0);
					SERIALIZE(ReplayVersionInit, prevTick, 0);
					SERIALIZE(ReplayVersionInit, timeAccum, 0.0);
					SERIALIZE(ReplayVersionInit, deltaTime, 0);


					event.curTick = curTick;
					std::tuple<Tick, Tick, f64, fixed16_16> tData = std::make_tuple(curTick, prevTick, timeAccum, deltaTime);
					event.timeData = tData;
					*/
					inputEvents[updateCounter] = event;
				}
				resetCounter();
				setMaxInputCounter(updateCounter);
			}

			s_replayFile.close();
		}

		if (!writeFlag)
		{
			TFE_System::setStartTime(s_startTime);
			TFE_System::logWrite(LOG_MSG, "PLAYBACK START DATE", "INPUT UPDATE TIME %d MAX COUNT %d", s_startTime, updateCounter);
		}
		else
		{
			TFE_System::logWrite(LOG_MSG, "RECORD START DATE", "INPUT UPDATE TIME %d", s_startTime);
		}
		TFE_DarkForces::time_pause(JFALSE);
	}

	void recordEye()
	{
		r_yaw = TFE_DarkForces::s_playerEye->yaw;
		r_pitch = TFE_DarkForces::s_playerEye->pitch;
		r_roll = TFE_DarkForces::s_playerEye->roll;
		TFE_System::logWrite(LOG_MSG, "REPLAY", "Record yaw=%d pitch=%d roll=%d", r_yaw, r_pitch, r_roll);

	}

	void setEye()
	{
		TFE_DarkForces::s_playerEye->yaw = r_yaw;
		TFE_DarkForces::s_playerEye->pitch = r_pitch;
		TFE_DarkForces::s_playerEye->roll = r_roll;
		TFE_System::logWrite(LOG_MSG, "REPLAY", "Set yaw=%d pitch=%d roll=%d", r_yaw, r_pitch, r_roll);
	}

	void startRecording()
	{		
		if (FileUtil::exists(s_replayPath))
		{
			FileUtil::deleteFile(s_replayPath);
		}

		for (int i = 0; i < inputEvents.size(); i++)
		{
			inputEvents[i].clear();
		}

		recordReplaySeed();
		inputMapping_endFrame();
		//TFE_DarkForces::resetTime();
		s_recording = true;		
		setDemoPlayback(false);
		resetCounter();
		setMaxInputCounter(0);
	}

	void endRecording()
	{
		s_recording = false;
		FileStream* stream = &s_replayFile;
		serializeDemo(&s_replayFile, true);
		setDemoPlayback(false);
		inputMapping_endFrame();
	}

	void recordReplayTime(u64 startTime)
	{
		s_startTime = startTime;
	}

	void loadReplay()
	{
		if (TFE_Settings::getGameSettings()->df_enableRecording) return;
		resetCounter();
		restoreReplaySeed();
		inputMapping_endFrame();
		//TFE_DarkForces::resetTime();
		FileStream* stream = &s_replayFile;
		serializeDemo(&s_replayFile, false);
		setDemoPlayback(true);
		//TFE_System::logWrite(LOG_MSG, "Progam Flow", "STARTTICK BEFORE  %d", TFE_DarkForces::s_curTick);
		TFE_DarkForces::s_curTick = inputEvents[1].curTick;
		TFE_DarkForces::s_prevTick = inputEvents[1].prevTick;
		//TFE_DarkForces::setTimeAccum(inputEvents[1].timeAccum);
		TFE_DarkForces::s_deltaTime = inputEvents[1].deltaTime;
		memcpy(TFE_DarkForces::s_frameTicks, inputEvents[1].frameTicks, sizeof(fixed16_16) * TFE_ARRAYSIZE(inputEvents[1].frameTicks));
		//TFE_System::logWrite(LOG_MSG, "Progam Flow", "STARTTICK AFTER  %d", TFE_DarkForces::s_curTick);
	}
}