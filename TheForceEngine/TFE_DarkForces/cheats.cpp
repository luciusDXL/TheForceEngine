#include <cstring>

#include "cheats.h"
#include <TFE_Jedi/Math/core_math.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	// In the DOS code, the cheats were encoded by taking the character and subtracting 0x3c (60) before storing in the executable.
	// For TFE, I don't see the point - there is no hiding the cheats now with the source code freely available. :)-
	static const char* c_cheatStrings[] =
	{
		"CDS",
		"NTFH",
		"POGO",
		"RANDY",
		"IMLAME",
		"POSTAL",
		"DATA",
		"BUG",
		"REDLITE",
		"SECBASE",
		"TALAY",
		"SEWERS",
		"TESTBASE",
		"GROMAS",
		"DTENTION",
		"RAMSHAD",
		"ROBOTICS",
		"NARSHADA",
		"JABSHIP",
		"IMPCITY",
		"FUELSTAT",
		"EXECUTOR",
		"ARC",
		"SKIP",
		"BRADY",
		"UNLOCK",
		"MAXOUT",
	};

	char s_cheatString[32] = { 0 };
	s32  s_cheatCharIndex = 0;
	s32  s_cheatInputCount = 0;

	CheatID cheat_getIDFromString(const char* cheatStr)
	{
		if (!cheatStr || strlen(cheatStr) <= 2)
		{
			return CHEAT_NONE;
		}

		s32 maxMatchIndex = 0;
		const size_t cheatCount = TFE_ARRAYSIZE(c_cheatStrings);
		for (size_t cheatIndex = 0; cheatIndex < cheatCount; cheatIndex++)
		{
			if (strcasecmp(cheatStr + 2, c_cheatStrings[cheatIndex]) == 0)
			{
				return CheatID(cheatIndex + 1);
			}
		}
		return CHEAT_NONE;
	}

	CheatID cheat_getID()
	{
		if (!s_cheatCharIndex)
		{
			return CHEAT_NONE;
		}

		s32 maxMatchIndex = 0;
		const size_t cheatCount = TFE_ARRAYSIZE(c_cheatStrings);
		for (size_t cheatIndex = 0; cheatIndex < cheatCount; cheatIndex++)
		{
			const char* srcCheatStr = c_cheatStrings[cheatIndex];
			for (s32 c = 0; c <= s_cheatCharIndex; c++)
			{
				if (srcCheatStr[c])
				{
					if (srcCheatStr[c] != s_cheatString[c])
					{
						break;
					}
				}
				else if (s_cheatString[c])
				{
					break;
				}

				// If the strings match to the end, clear out the data and return the cheat ID.
				maxMatchIndex = max(maxMatchIndex, c + 1);
				if (!srcCheatStr[c + 1])
				{
					cheat_clearData();
					return CheatID(cheatIndex + 1);
				}
			}
		}

		// If we match don't match up to this point, then clear out the data.
		if (maxMatchIndex != s_cheatCharIndex)
		{
			cheat_clearData();
		}
		return CHEAT_NONE;
	}

	void cheat_clearData()
	{
		s_cheatInputCount = 0;
		s_cheatString[0] = 0;
		s_cheatCharIndex = 0;
	}
}  // TFE_DarkForces