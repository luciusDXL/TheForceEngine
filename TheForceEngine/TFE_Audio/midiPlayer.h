#pragma once
#include <TFE_System/types.h>
struct GMidiAsset;

namespace TFE_MidiPlayer
{
	bool init();
	void destroy();

	void playSong(const GMidiAsset* gmidAsset, bool loop);

	void setVolume(f32 volume);
	void pause();
	void resume();
	void stop();
};
