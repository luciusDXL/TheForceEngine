#pragma once
#include <TFE_System/types.h>

enum GameID
{
	Game_Dark_Forces = 0,
	Game_Outlaws,
	Game_Count
};

namespace TFE_Settings
{
	static const char* c_gameName[] =
	{
		"Dark Forces",
		"Outlaws",
	};

	// GOG Product IDs:
	static const char* c_gogProductId[Game_Count] =
	{
		"1421404433",	// Game_Dark_Forces
		"1425302464"	// Game_Outlaws
	};

	static const char* c_steamLocalPath[Game_Count] =
	{
		"dark forces/Game/",	// Game_Dark_Forces
		"outlaws/",				// Game_Outlaws
	};

	#ifdef _WIN32
		static const char* c_darkForcesLocations[] =
		{
			// C drive
			"C:/Program Files (x86)/Steam/steamapps/common/dark forces/Game/",
			"C:/Program Files/Steam/steamapps/common/dark forces/Game/",
			"C:/Program Files (x86)/GOG.com/Star Wars - Dark Forces/",
			"C:/GOG Games/Star Wars - Dark Forces/",
			// D drive
			"D:/Program Files (x86)/Steam/steamapps/common/dark forces/Game/",
			"D:/Program Files/Steam/steamapps/common/dark forces/Game/",
			"D:/Program Files (x86)/GOG.com/Star Wars - Dark Forces/",
			"D:/GOG Games/Star Wars - Dark Forces/",
		};
		static const char* c_outlawsLocations[] =
		{
			// C drive
			"C:/Program Files (x86)/Steam/steamapps/common/outlaws/",
			"C:/Program Files/Steam/steamapps/common/outlaws/",
			"C:/Program Files (x86)/GOG.com/outlaws/",
			"C:/GOG Games/outlaws/",
			// D drive
			"D:/Program Files (x86)/Steam/steamapps/common/outlaws/",
			"D:/Program Files/Steam/steamapps/common/outlaws/",
			"D:/Program Files (x86)/GOG.com/outlaws/",
			"D:/GOG Games/outlaws/",
		};
		static const u32 c_hardcodedPathCount = TFE_ARRAYSIZE(c_darkForcesLocations);

		static const char** c_gameLocations[] =
		{
			c_darkForcesLocations,
			c_outlawsLocations
		};
	#endif /* _WIN32 */
}