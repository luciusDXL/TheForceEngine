#include <algorithm>
#include <cstring>

#include "midi.h"

#include "systemMidiDevice.h"
#ifdef __linux__
 #include <rtmidi/RtMidi.h>
#else
 #include "RtMidi.h"
#endif
#include <TFE_System/system.h>

#ifdef _WIN32
// Windows library required to access the midi device(s).
#pragma comment( lib, "winmm.lib" )
#endif

//This system uses "RtMidi" as the low level, cross platform interface to midi devices.
//https://www.music.mcgill.ca/~gary/rtmidi/

namespace TFE_Audio
{
	static const char* c_SystemMidi_Name = "System Midi";
	enum
	{
		SMID_DRUM_CHANNEL = 9,
	};

	void midiErrorCallback(RtMidiError::Type, const std::string&, void*);

	void SystemMidiDevice::recordMidiState(u8 cmd, u8 a1, u8 a2)
	{
		u8 id = cmd & 0xf0;
		u8 chan = cmd & 0x0f;
		if (id == MID_PROGRAM_CHANGE)
		{
			m_midiState.pg[chan] = a1;
		}
		else if ((id == MID_CONTROL_CHANGE) && (a1 < 120))
		{
			(m_midiState.cc[chan])[a1] = a2;
		}
	}

	// replay all the program change and control change
	// messages for each of the 16 channels to the
	// midi device.
	void SystemMidiDevice::restoreMidiState(void)
	{
		u8 buf[3];
		SDL_LockMutex(m_portLock);
		for (u8 i = 0; i < MIDI_CHANNEL_COUNT; i++)
		{
			buf[0] = MID_PROGRAM_CHANGE + i;
			buf[1] = m_midiState.pg[i];
			_message(buf, 2);
			buf[0] = MID_CONTROL_CHANGE + i;
			buf[1] = 121;	// reset all controllers
			buf[2] = 0;
			_message(buf, 3);
			auto m = (m_midiState.cc)[i];
			for (auto mit = m.begin(); mit != m.end(); mit++)
			{
				buf[1] = mit->first;
				buf[2] = mit->second;
				_message(buf, 3);
			}
		}
		SDL_UnlockMutex(m_portLock);
	}
	
	SystemMidiDevice::SystemMidiDevice()
	{
		m_midiout = nullptr;
		m_outputId = 0;
		m_midiState.clear();
		m_portLock = SDL_CreateMutex();
		if (!m_portLock)
			return;
		m_midiout = new RtMidiOut();
		if (!m_midiout)
			return;
		m_midiout->setErrorCallback(midiErrorCallback);

		getOutputCount();
	}

	SystemMidiDevice::~SystemMidiDevice()
	{
		exit();
		SDL_DestroyMutex(m_portLock);
		m_portLock = nullptr;
	}

	void SystemMidiDevice::exit()
	{
		if (m_midiout && (m_outputId > 0))
		{
			m_midiout->closePort();
			delete m_midiout;
			m_midiout = nullptr;
		}
		m_outputId = -1;
		m_midiState.clear();
	}

	const char* SystemMidiDevice::getName()
	{
		return c_SystemMidi_Name;
	}

	void SystemMidiDevice::_message(const u8* msg, u32 len)
	{
		if (m_outputId > 0)
			m_midiout->sendMessage(msg, (size_t)len);
	}

	void SystemMidiDevice::message(const u8* msg, u32 len)
	{
		// this mutex is uncontended except for a short time
		// after a midi device switch is done in the settings.
		// It's here to avoid multiple threads writing to this
		// port at the same time, which confuses the hell out
		// of at least the Linux ALSA midi parser.
		SDL_LockMutex(m_portLock);
		recordMidiState(msg[0], msg[1], msg[2]);
		_message(msg, len);
		SDL_UnlockMutex(m_portLock);
	}

	void SystemMidiDevice::message(u8 type, u8 arg1, u8 arg2)
	{
		const u8 msg[3] = { type, arg1, arg2 };
		message(msg, 3);
	}


	void SystemMidiDevice::noteAllOff()
	{
		u8 buf[3] = { 0, MID_ALL_NOTES_OFF, 0 };

		SDL_LockMutex(m_portLock);
		for (u32 c = 0; c < MIDI_CHANNEL_COUNT; c++)
		{
			buf[0] = MID_CONTROL_CHANGE + c;
			_message(buf, 3);
		}
		SDL_UnlockMutex(m_portLock);
	}

	void SystemMidiDevice::setVolume(f32 volume)
	{
		// No-op.
	}

	u32 SystemMidiDevice::getOutputCount()
	{
		m_outputs.clear();
		m_outputs.push_back("(Disabled)");
		if (m_midiout)
		{
			u32 count = m_midiout->getPortCount();

			for (u32 i = 0; i < count; i++)
			{
				m_outputs.push_back(m_midiout->getPortName(i));
			}
		}
		return (u32)m_outputs.size();
	}

	void SystemMidiDevice::getOutputName(s32 index, char* buffer, u32 maxLength)
	{
		if (index < 0 || index >= (s32)getOutputCount()) { return; }

		const std::string& name = m_outputs[index];
		const u32 copyLength = std::min((u32)name.length(), maxLength - 1);
		strncpy(buffer, name.c_str(), copyLength);
		buffer[copyLength] = 0;
	}

	bool SystemMidiDevice::selectOutput(s32 index)
	{
		if (index < 0 || index >= (s32)getOutputCount())
		{
			index = 0;	// "disabled" device
		}
		if (index != m_outputId && m_midiout)
		{
			noteAllOff();
			m_midiout->closePort();
			if (index > 0)	// real Device
			{
				m_midiout->openPort(index - 1);
				if (m_midiout->isPortOpen())
					restoreMidiState();
			}
		}
		m_outputId = index;
		return true;
	}

	s32 SystemMidiDevice::getActiveOutput(void)
	{
		return m_outputId;
	}

	void midiErrorCallback(RtMidiError::Type type, const std::string &errorText, void *userData)
	{
		TFE_System::logWrite(LOG_ERROR, "Midi Device", "%s", errorText.c_str());
	}
}
