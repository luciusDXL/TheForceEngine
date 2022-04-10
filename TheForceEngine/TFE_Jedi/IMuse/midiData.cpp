#include "imuse.h"
#include "midiData.h"
#include <TFE_System/system.h>
#include <TFE_Audio/midi.h>
#include <assert.h>

namespace TFE_Jedi
{
	u32 midi_getChunkSize(u8* data)
	{
		u8* curData = data;
		u32 v0 = curData[0] << 8; curData++;
		u32 v1 = (curData[0] | v0) << 8; curData++;
		u32 v2 = (curData[0] | v1) << 8; curData++;
		u32 v3 = curData[0] | v2;

		return data[3] | (data[2] << 8) | (data[1] << 16) | (data[0] << 24);
	}

	u8* midi_gotoHeader(u8* data, const char* header, s32 hdrIndex)
	{
		if (data)
		{
			data += 4;
			u32 size = midi_getChunkSize(data);
			u32 curIndex = 0;
			data += 4;
			while (curIndex < size)
			{
				s32 hdrNotMatch = 0;
				for (s32 i = 0; i < 4; i++, curIndex++)
				{
					if (data[curIndex] != header[i])
					{
						hdrNotMatch = 1;
					}
				}
				if (!hdrNotMatch) // Match
				{
					if (hdrIndex == 0)
					{
						// returns the pointer starting at the beginning of the header.
						return data + curIndex - 4;
					}
					hdrIndex--;
				}
				curIndex += midi_getChunkSize(data + curIndex) + 4;
			}
		}
		return nullptr;
	}

	u32 midi_getVariableLengthValue(u8** chunkData)
	{
		u32 value = 0u;
		for (u32 i = 0; i < 4; i++)
		{
			const u8 partial = (*chunkData)[0];
			(*chunkData)++;

			value = (value << 7u) | (partial & 0x7f);
			if (!(partial & 0x80)) { break; }
		}
		return value;
	}
}  // namespace TFE_Jedi