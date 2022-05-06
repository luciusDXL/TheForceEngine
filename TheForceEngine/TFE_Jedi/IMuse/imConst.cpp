#include "imConst.h"
#include "imuse.h"
#include <TFE_System/system.h>
#include <TFE_Audio/midi.h>

namespace TFE_Jedi
{
	// In the original code, there are two arrays of midi message size, but the values are exactly the same.
	const u8 c_midiMsgSize[] =
	{
		3, 3, 3, 3, 2, 2, 3, 1,
	};
	const u32 c_channelMask[MIDI_CHANNEL_COUNT] =
	{
		FLAG_BIT( 0), FLAG_BIT( 1), FLAG_BIT( 2), FLAG_BIT( 3),
		FLAG_BIT( 4), FLAG_BIT( 5), FLAG_BIT( 6), FLAG_BIT( 7),
		FLAG_BIT( 8), FLAG_BIT( 9), FLAG_BIT(10), FLAG_BIT(11),
		FLAG_BIT(12), FLAG_BIT(13), FLAG_BIT(14), FLAG_BIT(15)
	};
}  // namespace TFE_Jedi
