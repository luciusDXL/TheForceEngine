#include "imConst.h"
#include "imuse.h"
#include <TFE_System/system.h>

namespace TFE_Jedi
{
	const u8 s_midiMsgSize[] =
	{
		3, 3, 3, 3, 2, 2, 3, 1,
	};
	const s32 s_midiMessageSize2[] =
	{
		3, 3, 3, 3, 2, 2, 3, 1,
	};
	const s32 s_channelMask[imChannelCount] =
	{
		(1 << 0), (1 << 1), (1 << 2), (1 << 3),
		(1 << 4), (1 << 5), (1 << 6), (1 << 7),
		(1 << 8), (1 << 9), (1 << 10), (1 << 11),
		(1 << 12), (1 << 13), (1 << 14), (1 << 15)
	};
}  // namespace TFE_Jedi
