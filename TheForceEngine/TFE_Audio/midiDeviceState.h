#pragma once
///////////////////////////////////////////////
// Shared state that can be used to restore
// some of the midi state when switching midi devices
// or outputs.
///////////////////////////////////////////////
#include <TFE_System/types.h>
#include "midi.h"

namespace TFE_Audio
{
	struct PresetNumber
	{
		s32 arg1;
		s32 drums;
	};
	
	void midiState_clearPresets();
	void midiState_setPreset(u32 channel, s32 arg1, s32 drums);
	const PresetNumber* midiState_getPresets();
}
