#pragma once
#include <TFE_System/types.h>
#include "midiDevice.h"
struct GMidiAsset;

namespace TFE_MidiPlayer
{
	bool init(s32 midiDeviceIndex, MidiDeviceType type = MIDI_TYPE_DEVICE);
	void setDeviceType(MidiDeviceType type);
	MidiDeviceType getDeviceType();
	void destroy();

	TFE_Audio::MidiDevice* getMidiDevice();
	const char* getMidiDeviceTypeName(MidiDeviceType type);
	
	///////////////////////////////////////////////////////////
	// Commands
	//   Commands are queued for processing by the midi thread.
	///////////////////////////////////////////////////////////
		
	// Change the overall music volume.
	void setVolume(f32 volume);
	// Set the maximum length in seconds that a note is allowed to play for in seconds.
	void setMaximumNoteLength(f32 dt = 16.0f);

	// Send a direct midi message.
	// Note: this should be called from the midi thread.
	void sendMessageDirect(u8 type, u8 arg1=0, u8 arg2=0);

	// Callback
	void midiSetCallback(void(*callback)(void) = nullptr, f64 timeStep = 0.0);
	void midiClearCallback();

	void pauseThread();
	void resumeThread();
		
	// Pause the midi player, which also stops all sound channels.
	void pause();
	// Resume midi playback from where it left off.
	void resume();
	// Stop all notes.
	void stopMidiSound();

	void synthesizeMidi(f32* buffer, u32 stereoSampleCount, bool updateBuffer = true);

	///////////////////////////////////////////////////////////
	// Reads
	//   Reads are not synced, so there may be latency in the
	//   results if not being read from an iMuse callback.
	///////////////////////////////////////////////////////////
	f32 getVolume();
};
