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
		std::vector<s32> mouseDown;
		std::vector<s32> mousePos;
		Tick curTick;
		std::tuple<Tick, Tick, f64, fixed16_16> timeData;
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
	void recordTiming();
	void loadTiming();
	void saveTick(std::tuple<Tick, Tick, f64, fixed16_16> tData);
	void sTick(Tick curTick);
	Tick lTick();
	std::tuple<Tick, Tick, f64, fixed16_16> loadTick();

	std::string convertToString(std::vector<s32> keysDown);
	std::vector<s32> convertFromString(std::string keyStr);
}