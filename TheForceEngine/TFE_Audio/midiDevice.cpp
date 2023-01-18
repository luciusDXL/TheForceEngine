#include <cstring>

#include "midiDevice.h"
#ifdef __linux__
 #include <rtmidi/RtMidi.h>
#else
 #include "RtMidi.h"
#endif
#include <TFE_System/system.h>
#include <algorithm>

#ifdef _WIN32
// Windows library required to access the midi device(s).
#pragma comment( lib, "winmm.lib" )
#endif

//This system uses "RtMidi" as the low level, cross platform interface to midi devices.
//https://www.music.mcgill.ca/~gary/rtmidi/

namespace TFE_MidiDevice
{
	RtMidiOut *s_midiout = nullptr;
	static s32 s_openPort = -1;

	void midiErrorCallback(RtMidiError::Type type, const std::string &errorText, void *userData)
	{
		TFE_System::logWrite(LOG_ERROR, "Midi Device", "%s", errorText.c_str());
	}

	bool init()
	{
		s_midiout = new RtMidiOut();
		s_midiout->setErrorCallback(midiErrorCallback);
		s_openPort = -1;

		return true;
	}

	void destroy()
	{
		if (s_openPort >= 0)
		{
			s_midiout->closePort();
		}
		delete s_midiout;
		s_midiout = nullptr;
		s_openPort = -1;
	}

	// Returns the number of devices.
	u32 getDeviceCount()
	{
		return s_midiout ? s_midiout->getPortCount() : 0;
	}

	void getDeviceName(u32 index, char* buffer, u32 maxLength)
	{
		if (index >= getDeviceCount()) { return; }
		const std::string& name = s_midiout->getPortName(index);
		const u32 copyLength = std::min((u32)name.length(), maxLength - 1);
		strncpy(buffer, s_midiout->getPortName(index).c_str(), copyLength);
		buffer[copyLength] = 0;
	}

	bool selectDevice(s32 index)
	{
		if (!s_midiout) { return false; }
		if (index < 0)
		{
			index = 0;
		}
		if (index != s_openPort && index >= 0 && index < getDeviceCount())
		{
			s_midiout->openPort(index);
			s_openPort = (s32)index;
			return true;
		}
		return false;
	}

	void sendMessage(const u8* msg, u32 size)
	{
		if (s_midiout) { s_midiout->sendMessage(msg, (size_t)size); }
	}

	void sendMessage(u8 arg0, u8 arg1, u8 arg2)
	{
		const u8 msg[3] = { arg0, arg1, arg2 };
		if (s_midiout) { s_midiout->sendMessage(msg, 3); }
	}
}
