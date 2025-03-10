#pragma once

#include <TFE_System/types.h>
#include <TFE_Input/inputEnum.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <TFE_DarkForces/time.h>
#include <TFE_Game/saveSystem.h>

namespace TFE_Input
{
	// Struct for storing replay events
	struct ReplayEvent
	{
		// Store keyboard and mouse input
		std::vector<s32> keysDown;
		std::vector<s32> keysPressed;
		std::vector<s32> mousePos;

		// Store the tick information
		Tick curTick;
		Tick prevTick;
		Tick plTick;
		Tick plPrevTick;
		fixed16_16 frameTicks[13];

		// Store time information
		fixed16_16 deltaTime;
		f64 timeAccum;

		// Store PDA vector position
		Vec2i pdaPosition = {};

		// Wipe the replay event
		void clear() {
			keysDown.clear();
			keysPressed.clear();
			mousePos.clear();
			curTick = 0;
			prevTick = 0;
			deltaTime = 0;
			timeAccum = 0.0;
			plTick = 0;
			plPrevTick = 0;
			std::fill(std::begin(frameTicks), std::end(frameTicks), 0);
			pdaPosition = {};
		}
	};
	
	extern std::unordered_map<int, ReplayEvent> inputEvents;
	extern char s_replayDir[TFE_MAX_PATH];

	std::string convertToString(std::vector<s32> keysDown);
	std::vector<s32> convertFromString(std::string keyStr);

	void initReplays();

	void storePDAPosition(Vec2i pos);
	Vec2i getPDAPosition();

	void loadReplayFromPath(const char* replayPath);
	void populateReplayDirectory(std::vector<TFE_SaveSystem::SaveHeader>& dir);
	bool loadReplayHeader(const char* filename, TFE_SaveSystem::SaveHeader* header);
	void loadReplayWrapper(string replayFile, string modName, string levelName);
	void getAgentPath(char* agentPath);

	bool isRecording();
	void setRecording(bool recording);
	bool isDemoPlayback();
	void setDemoPlayback(bool playback);
	bool isReplaySystemLive();

	bool sendHudStartMessage();
	bool isReplayPaused();
	void increaseReplayFrameRate();
	void decreaseReplayFrameRate();

	bool startReplayStatus();
	void recordReplaySeed();
	void restoreReplaySeed();
	void recordReplayTime(u64 startTime);

	void logReplayPosition(int counter);

	void saveTick(); 
	void loadTick();

	void sendEndPlaybackMsg();
	void sendEndRecordingMsg();

	void recordEvent(int action, KeyboardCode keyCode, bool isPress);
	void replayEvent();

	void startRecording();
	void endRecording();
	void loadReplay();
	void endReplay();

}