#pragma once
#include <TFE_System/types.h>
#include <TFE_Audio/iMuseEvent.h>
struct GMidiAsset;
typedef void(*iMuseCallback)(const iMuseEvent*);

namespace TFE_MidiPlayer
{
	bool init();
	void destroy();
	void midiSetTimeScale(f64 scale = 1.0);

	///////////////////////////////////////////////////////////
	// Commands
	//   Commands are queued for processing by the midi thread.
	///////////////////////////////////////////////////////////

	// Start a new song.
	void playSong(const GMidiAsset* gmidAsset, bool loop, s32 track = 0);
	// Jump to a new location in the midi stream.
	void midiJump(s32 track, s32 measure, s32 beat = 1, s32 tick = 0);
	// Change the overall music volume.
	void setVolume(f32 volume);

	// The iMuse callback is called whenever iMuse commands are encountered in the midi stream.
	// The callback runs in the same thread as the Midi playback, which means that callbacks should not
	// do any heavy processing. It also means that reads are accurate.
	void midiSet_iMuseCallback(iMuseCallback callback);
		
	// Pause the midi player, which also stops all sound channels.
	void pause();
	// Resume midi playback from where it left off.
	void resume();
	// Completely stop the current song. This causes all music sounds to be turned off.
	void stop();

	///////////////////////////////////////////////////////////
	// Reads
	//   Reads are not synced, so there may be latency in the
	//   results if not being read from an iMuse callback.
	///////////////////////////////////////////////////////////
	f32 getVolume();
	u32 getMeasure();
	u32 getBeat();
	u64 getTick();

	iMuseCallback midiGet_iMuseCallback();
};
