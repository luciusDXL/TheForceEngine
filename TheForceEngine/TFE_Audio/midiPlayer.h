#pragma once
#include <TFE_System/types.h>
struct GMidiAsset;

namespace TFE_MidiPlayer
{
	bool init();
	void destroy();

	void playSong(const GMidiAsset* gmidAsset, bool loop, s32 track = 0);
	void midiJump(s32 track, s32 measure, s32 beat);
	void midiSetTimeScale(f64 scale = 1.0);

	void setVolume(f32 volume);
	f32  getVolume();

	void pause();
	void resume();
	void stop();
};
