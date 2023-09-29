#pragma once
#include <array>
#include <map>
#include <TFE_System/types.h>
#include <SDL_mutex.h>
#include "midiDevice.h"

class RtMidiOut;

namespace TFE_Audio
{
	struct midiState
	{
		u8 pg[MIDI_CHANNEL_COUNT];				// per channel selected program
		std::array<std::map<u8, u8>, MIDI_CHANNEL_COUNT> cc;	// per channel control params
	};

	class SystemMidiDevice : public MidiDevice
	{
	public:
		SystemMidiDevice();
		~SystemMidiDevice() override;

		MidiDeviceType getType() override { return MIDI_TYPE_SYSTEM; }

		void exit() override;
		// The system device does not have proper global volume control.
		bool hasGlobalVolumeCtrl() override { return false; }
		const char* getName() override;

		// The System Midi device outputs commands to midi hardware or external programs and so never renders sound to the audio thread.
		bool render(f32* buffer, u32 sampleCount) override { return false; }
		bool canRender() override { return false; }

		void message(u8 type, u8 arg1, u8 arg2) override;
		void message(const u8* msg, u32 len) override;
		void noteAllOff() override;
		void setVolume(f32 volume) override;

		u32  getOutputCount() override;
		void getOutputName(s32 index, char* buffer, u32 maxLength) override;
		bool selectOutput(s32 index) override;
		s32  getActiveOutput(void) override;

	private:
		void _message(u8 type, u8 arg1, u8 arg2);
		void recordMidiState(u8 cmd, u8 a1, u8 a2);
		void restoreMidiState(void);

		RtMidiOut* m_midiout;

		s32  m_outputId;
		FileList m_outputs;
		SDL_Mutex *m_portLock;
		midiState m_midiState;
	};
}
