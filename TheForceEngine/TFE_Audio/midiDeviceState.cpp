#include <cstring>
#include "midiDeviceState.h"

namespace TFE_Audio
{
	static PresetNumber s_presetNumbers[MIDI_CHANNEL_COUNT];

	void midiState_clearPresets()
	{
		memset(s_presetNumbers, 0, sizeof(PresetNumber) * MIDI_CHANNEL_COUNT);
	}

	void midiState_setPreset(u32 channel, s32 arg1, s32 drums)
	{
		s_presetNumbers[channel] = { arg1, drums };
	}

	const PresetNumber* midiState_getPresets()
	{
		return s_presetNumbers;
	}
}
