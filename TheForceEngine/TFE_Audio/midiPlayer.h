#pragma once
#include <TFE_System/types.h>
#include <TFE_Audio/iMuseEvent.h>
struct GMidiAsset;
typedef void(*iMuseCallback)(const iMuseEvent*);

namespace TFE_MidiPlayer
{
	bool init();
	void destroy();

	void playSong(const GMidiAsset* gmidAsset, bool loop, s32 track = 0);
	void midiJump(s32 track, s32 measure, s32 beat);
	void midiSetTimeScale(f64 scale = 1.0);
	void setVolume(f32 volume);
	void midiSet_iMuseCallback(iMuseCallback callback);

	f32  getVolume();

	void pause();
	void resume();
	void stop();
};
