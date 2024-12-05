#pragma once

#include <TFE_System/types.h>
#include <TFE_Input/inputEnum.h>
#include <array>
#include <vector>
#include <unordered_map>
#include <string>
#include <TFE_DarkForces/time.h>

namespace TFE_Input
{
	enum ReplayVersion : u32
	{
		ReplayVersionInit = 1,
		ReplayVersionCur = ReplayVersionInit
	};


	struct ReplayEvent
	{
		std::vector<s32> keysDown;
		std::vector<s32> keysPressed;
		std::vector<s32> mouseDown;
		std::vector<s32> mousePos;
		std::vector<s32> mouseWheel;
		Tick curTick;
		Tick prevTick;
		fixed16_16 deltaTime;
		f64 timeAccum;
		fixed16_16 frameTicks[13];
		//std::tuple<Tick, Tick, f64, fixed16_16> timeData;

		void clear() {
			keysDown.clear();
			keysPressed.clear();
			mouseDown.clear();
			mousePos.clear();
			mouseWheel.clear();
			curTick = 0;
			prevTick = 0;
			deltaTime = 0;
			timeAccum = 0.0;
			std::fill(std::begin(frameTicks), std::end(frameTicks), 0);
			//timeData = std::make_tuple(0, 0, 0.0, 0); // Reset tuple values
		}
	};

	extern std::unordered_map<int, ReplayEvent> inputEvents;

	void initReplays();
	void recordReplayTime(u64 startTime);
	void startRecording();
	void endRecording();
	void loadReplay();

	bool isRecording();
	void setRecording(bool recording);
	bool isDemoPlayback();
	void setDemoPlayback(bool playback);
	void recordReplaySeed();
	void restoreReplaySeed();
	void saveTick();
	void loadTick();
	void recordEye();
	void setEye();

	std::string convertToString(std::vector<s32> keysDown);
	std::vector<s32> convertFromString(std::string keyStr);
}