#include "audioDevice.h"
#include <TFE_System/system.h>
#include "RtAudio.h"

//This system uses "RtAudio" as the low level, cross platform interface to the Audio system.
//https://www.music.mcgill.ca/~gary/rtaudio/

namespace TFE_AudioDevice
{
	static const char* c_audioErrorType[] =
	{
		"WARNING",           /*!< A non-critical error. */
		"DEBUG_WARNING",     /*!< A non-critical error which might be useful for debugging. */
		"UNSPECIFIED",       /*!< The default, unspecified error type. */
		"NO_DEVICES_FOUND",  /*!< No devices found on system. */
		"INVALID_DEVICE",    /*!< An invalid device ID was specified. */
		"MEMORY_ERROR",      /*!< An error occured during memory allocation. */
		"INVALID_PARAMETER", /*!< An invalid parameter was specified to a function. */
		"INVALID_USE",       /*!< The function was called incorrectly. */
		"DRIVER_ERROR",      /*!< A system driver error occured. */
		"SYSTEM_ERROR",      /*!< A system error occured. */
		"THREAD_ERROR"       /*!< A thread error occured. */
	};

	static RtAudio* s_device = NULL;
	static u32 s_outputDevice;
	static u32 s_inputDevice;

	static RtAudio::DeviceInfo s_InputInfo;
	static RtAudio::DeviceInfo s_OutputInfo;

	static u32  s_audioFrameSize;
	static bool s_streamStarted;

	struct OutputDeviceInfo
	{
		std::string name;
		u32 id;
	};
	static std::vector<OutputDeviceInfo> s_outputDeviceList;
	static u32 s_defaultOutputDeviceId;

	bool queryOutputDevices();

	bool init(u32 audioFrameSize, s32 deviceId/*=-1*/, bool useNullDevice/*=false*/)
	{
		if (useNullDevice)
		{
			TFE_System::logWrite(LOG_WARNING, "Audio", "Audio disabled.");
			s_device = nullptr;
			return false;
		}

		try
		{
			s_device = new RtAudio();
		}
		catch(RtAudioError& error)
		{
			TFE_System::logWrite(LOG_ERROR, "Audio", "Cannot initialize the audio system. Error: '%s', type: '%s'", error.getMessage().c_str(), c_audioErrorType[error.getType()]);
			s_device = nullptr;
		}
		if (!s_device) { return false; }

		// Query the output devices.
		if (!queryOutputDevices())
		{
			delete s_device;
			s_device = nullptr;
			return false;
		}
		// Validate the device ID.
		if (deviceId >= 0)
		{
			bool foundDevice = false;
			for (size_t i = 0; i < s_outputDeviceList.size(); i++)
			{
				if (s_outputDeviceList[i].id == deviceId)
				{
					foundDevice = true;
					break;
				}
			}
			if (!foundDevice)
			{
				deviceId = -1;
			}
		}

		u32 deviceCount = s_device->getDeviceCount();
		u32 currentDeviceId = deviceId < 0 ? s_device->getDefaultOutputDevice() : deviceId;
		for (u32 i = 0; i < deviceCount; i++)
		{
			s_device->getDeviceInfo(i);
		}

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
		s_device = nullptr;
	}

	// Fills in deviceNames for all devices.
	bool queryOutputDevices()
	{
		try
		{
			s_defaultOutputDeviceId = s_device->getDefaultOutputDevice();
			TFE_System::logWrite(LOG_MSG, "Audio", "Default Audio Output ID: %u.", s_defaultOutputDeviceId);

			s_outputDeviceList.clear();
			u32 deviceCount = s_device->getDeviceCount();
			TFE_System::logWrite(LOG_MSG, "Audio", "Device Count: %u.", deviceCount);
			for (u32 i = 0; i < deviceCount; i++)
			{
				RtAudio::DeviceInfo srcInfo = s_device->getDeviceInfo(i);
				TFE_System::logWrite(LOG_MSG, "Audio", "Device: '%s', ID: %u, Output Channel Count: %u.", srcInfo.name.c_str(), i, srcInfo.outputChannels);
				if (srcInfo.outputChannels < 2)
				{
					continue;
				}
				s_outputDeviceList.push_back({ srcInfo.name, i });
			}
			return !s_outputDeviceList.empty();
		}
		catch (RtAudioError& error)
		{
			TFE_System::logWrite(LOG_ERROR, "Audio", "Cannot query audio output devices. Error: '%s', type: '%s'", error.getMessage().c_str(), c_audioErrorType[error.getType()]);
		}
		return false;
	}

	void errorCallback(RtAudioError::Type type, const std::string &errorText)
	{
		TFE_System::logWrite(LOG_ERROR, "Audio Device", "Error: '%s', type: '%s'", errorText.c_str(), c_audioErrorType[type]);
	}

	bool startOutput(StreamCallback callback, void* userData, u32 channels, u32 sampleRate)
	{
		if (!s_device) { return false; }

		try
		{
			RtAudio::StreamParameters  outParam;
			RtAudio::StreamParameters  inParam;
			RtAudio::StreamParameters* param[2] = { NULL, NULL };
			TFE_System::logWrite(LOG_MSG, "Audio", "Starting up audio stream for device '%s', id %u.", s_OutputInfo.name.c_str(), s_outputDevice);

			outParam.deviceId = s_outputDevice;
			outParam.nChannels = channels;
			outParam.firstChannel = 0;
			param[0] = &outParam;

			RtAudio::StreamOptions options = {};
			options.numberOfBuffers = 4;

			s_device->openStream(param[0], param[1], RTAUDIO_FLOAT32, sampleRate, &s_audioFrameSize, callback, userData, &options, errorCallback);
			s_device->startStream();
			s_streamStarted = true;
		}
		catch (RtAudioError& error)
		{
			TFE_System::logWrite(LOG_ERROR, "Audio", "Cannot start audio stream for output device %u.", s_outputDevice);
			TFE_System::logWrite(LOG_ERROR, "Audio", "Error: '%s', type: '%s'", error.getMessage().c_str(), c_audioErrorType[error.getType()]);
			s_streamStarted = false;
			return false;
		}

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
