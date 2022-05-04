#pragma once
#include <TFE_System/types.h>
struct GMidiAsset;

namespace TFE_MidiPlayer
{
	bool init();
	void destroy();
	void midiSetTimeScale(f64 scale = 1.0);

	///////////////////////////////////////////////////////////
	// Commands
	//   Commands are queued for processing by the midi thread.
	///////////////////////////////////////////////////////////

	// Change the overall music volume.
	void setVolume(f32 volume);

	// Send a direct midi message.
	// Note: this should be called from the midi thread.
	void sendMessageDirect(u8 type, u8 arg1=0, u8 arg2=0);

	// Callback
	void midiSetCallback(void(*callback)(void) = nullptr, f64 timeStep = 0.0);
	void midiClearCallback();
		
	// Pause the midi player, which also stops all sound channels.
	void pause();
	// Resume midi playback from where it left off.
	void resume();

	///////////////////////////////////////////////////////////
	// Reads
	//   Reads are not synced, so there may be latency in the
	//   results if not being read from an iMuse callback.
	///////////////////////////////////////////////////////////
	f32 getVolume();
};
