#include "audioDevice.h"
#include <TFE_System/system.h>
#include "RtAudio.h"

//This system uses "RtAudio" as the low level, cross platform interface to the Audio system.
//https://www.music.mcgill.ca/~gary/rtaudio/

namespace TFE_AudioDevice
{
	static RtAudio* s_device = NULL;
	static u32 s_outputDevice;
	static u32 s_inputDevice;

	static RtAudio::DeviceInfo s_InputInfo;
	static RtAudio::DeviceInfo s_OutputInfo;

	static u32  s_audioFrameSize;
	static bool s_streamStarted;

	bool init(u32 audioFrameSize)
	{
		s_device = new RtAudio();
		if (!s_device) { return false; }

		s_outputDevice = s_device->getDefaultOutputDevice();
		s_inputDevice  = s_device->getDefaultInputDevice();

		s_InputInfo  = s_device->getDeviceInfo(s_inputDevice);
		s_OutputInfo = s_device->getDeviceInfo(s_outputDevice);

		s_streamStarted  = false;
		s_audioFrameSize = audioFrameSize;

		return true;
	}

	void destroy()
	{
		stopOutput();
		delete s_device;
	}

	void errorCallback(RtAudioError::Type type, const std::string &errorText)
	{
		TFE_System::logWrite(LOG_ERROR, "Audio Device", "%s", errorText.c_str());
	}

	bool startOutput(StreamCallback callback, void* userData, u32 channels, u32 sampleRate)
	{
		if (!s_device) { return false; }

		RtAudio::StreamParameters  outParam;
		RtAudio::StreamParameters  inParam;
		RtAudio::StreamParameters* param[2] = { NULL, NULL };

		outParam.deviceId = s_outputDevice;
		outParam.nChannels = channels;
		outParam.firstChannel = 0;
		param[0] = &outParam;

		RtAudio::StreamOptions options = {};
		options.numberOfBuffers = 4;

		s_device->openStream(param[0], param[1], RTAUDIO_FLOAT32, sampleRate, &s_audioFrameSize, callback, userData, &options, errorCallback);
		s_device->startStream();
		s_streamStarted = true;

		return true;
	}

	void stopOutput()
	{
		if (s_device && s_streamStarted)
		{
			TFE_System::logWrite(LOG_MSG, "Audio", "Stop Audio Stream.");
			s_device->stopStream();
			s_streamStarted = false;
		}
	}
}
