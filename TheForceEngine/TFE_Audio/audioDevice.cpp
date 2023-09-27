#include <vector>
#include <TFE_System/system.h>
#include <SDL_audio.h>

#include "audioDevice.h"

// Audio Output using SDL Audio API
namespace TFE_AudioDevice
{
	static u32 s_audioFrameSize;
	static int s_outputDevice = -1;
	static bool s_streamStarted = false;
	static std::vector<OutputDeviceInfo> s_outputDeviceList;
	static SDL_AudioDeviceID s_adevid = 0;

	static int sdla_queryaudiodevs(void)
	{
		int d = SDL_GetNumAudioDevices(0);
		s_outputDeviceList.clear();
		s_outputDeviceList.push_back({"<autoselect>", 0});
		for (u32 i = 0; i < d; i++) {
			const char *dn = SDL_GetAudioDeviceName(i, 0);
			TFE_System::logWrite(LOG_MSG, "Audio", "Device %02d: %s", i, dn);
			s_outputDeviceList.push_back({dn, i+1});
		}
		return d;
	}

	bool init(u32 audioFrameSize, s32 deviceId/*=-1*/, bool useNullDevice/*=false*/)
	{
		if (s_adevid != 0)
		{
			destroy();
		}

		if (useNullDevice)
		{
			TFE_System::logWrite(LOG_WARNING, "Audio", "Audio disabled.");
			return false;
		}

		const char *dn = SDL_GetCurrentAudioDriver();
		TFE_System::logWrite(LOG_MSG, "Audio", "SDLAudio using interface '%s'", dn);

		int cnt = sdla_queryaudiodevs();
		if (cnt < 1)
		{
			TFE_System::logWrite(LOG_WARNING, "Audio", "no output devices!");
		}

		// Validate the device ID.
		if (deviceId >= 0)
		{
			bool found = false;
			for (int i = 0; i < s_outputDeviceList.size(); i++)
			{
				if (s_outputDeviceList[i].id == deviceId)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				deviceId = -1;
			}
		}

		s_outputDevice = deviceId < 0 ? getDefaultOutputDevice() : deviceId;
		s_streamStarted  = false;
		s_audioFrameSize = audioFrameSize;

		return true;
	}

	s32 getDefaultOutputDevice()
	{
		return 0;
	}

	s32 getOutputDeviceId()
	{
		return s_outputDevice;
	}

	s32 getOutputDeviceCount()
	{
		return (s32)s_outputDeviceList.size();
	}

	void destroy()
	{
		stopOutput();
	}

	// Fills in deviceNames for all devices.
	bool queryOutputDevices()
	{
		return sdla_queryaudiodevs() > 0;
	}

	bool startOutput(SDL_AudioCallback callback, void* userData, u32 channels, u32 sampleRate)
	{
		SDL_AudioDeviceID adevid;
		SDL_AudioSpec specin, specout;
		const char *dn;

		if (s_outputDevice < 0 || getOutputDeviceCount() < 1)
			return false;

		specin.freq = (int)sampleRate;
		specin.format = AUDIO_F32LSB;
		specin.channels = channels;
		specin.callback = callback;
		specin.userdata = userData;
		specin.samples = s_audioFrameSize;
		dn = s_outputDeviceList[s_outputDevice].name.c_str();

		TFE_System::logWrite(LOG_MSG, "Audio", "Starting up audio stream for device '%s'", dn);
		if (s_outputDevice < 1)
			dn = NULL;

		adevid = SDL_OpenAudioDevice(dn, 0, &specin, &specout, 0);
		if (adevid == 0)
		{
			TFE_System::logWrite(LOG_ERROR, "Audio", "Open Audio Device '%s' failed with '%s'",
					s_outputDeviceList[s_outputDevice].name.c_str(), SDL_GetError());
			return false;
		}

		s_adevid = adevid;
		SDL_PauseAudioDevice(adevid, 0);	// unpause
		s_streamStarted = true;

		return true;
	}

	void stopOutput()
	{
		if (s_streamStarted)
		{
			TFE_System::logWrite(LOG_MSG, "Audio", "Stop Audio Stream.");
			SDL_CloseAudioDevice(s_adevid);
			s_streamStarted = false;
			s_adevid = 0;
		}
	}

	const OutputDeviceInfo* getOutputDeviceList(s32& count, s32& curOutput)
	{
		count = s32(s_outputDeviceList.size());
		curOutput = s32(s_outputDevice);
		return s_outputDeviceList.data();
	}
}
