#pragma once
#include <TFE_System/types.h>
#include "audioOutput.h"
#include <string>
#include <SDL_audio.h>

namespace TFE_AudioDevice
{
	bool init(u32 audioFrameSize = 256u, s32 deviceId=-1, bool useNullDevice=false);
	void destroy();

	bool startOutput(SDL_AudioCallback callback, void* userData = 0, u32 channels = 2, u32 sampleRate = 44100);
	void stopOutput();

	s32 getDefaultOutputDevice();
	s32 getOutputDeviceId();
	s32 getOutputDeviceCount();

	const OutputDeviceInfo* getOutputDeviceList(s32& count, s32& curOutput);
};
