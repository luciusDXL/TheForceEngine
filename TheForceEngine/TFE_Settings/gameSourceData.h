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
	static const char* c_gogRemasterProductId[Game_Count] = 
	{
		"1853348102",	// Game_Dark_Forces
		"",				// Game_Outlaws (no remaster)
	};

	static const char* c_steamLocalPath[Game_Count] =
	{
		"dark forces/Game/",	// Game_Dark_Forces
		"outlaws/",				// Game_Outlaws
	};

	static const char* c_steamRemasterLocalPath[Game_Count] =
	{
		"Star Wars Dark Forces Remaster/",	// Game_Dark_Forces
		"outlaws/",				// Game_Outlaws
	};

	// Special case - some remasters include a ™ in their name breaking the path.	
	static const char* c_steamRemasterTMLocalPath[Game_Count] =
	{
		"Star Wars™ Dark Forces Remaster/",	// Game_Dark_Forces
		"outlaws/",				// Game_Outlaws
	};

	static const char* c_steamLocalSubPath[Game_Count] =
	{
		"Game/",	// Game_Dark_Forces
		"/",		// Game_Outlaws
	};

	static const char* c_steamRemasterLocalSubPath[Game_Count] =
	{
		"/",	// Game_Dark_Forces
		"/",	// Game_Outlaws
	};

	static const u32 c_steamProductId[Game_Count] =
	{
		32400,					// Game_Dark_Forces
		559620,					// Game_Outlaws
	};

	static const u32 c_steamRemasterProductId[Game_Count] =
	{
		2292260,				// Game_Dark_Forces
		0xffffffff,				// Game_Outlaws - no remaster (yet?)
	};

	// A file that should be checked to double-check the found path actually works.
	static const char* c_validationFile[Game_Count] =
	{
		"DARK.GOB",				// Game_Dark_Forces
		"OUTLAWS.LAB",			// Game_Outlaws
	};

	static const char* const c_darkForcesLocations[] =
	{
		// C drive
		"C:/Program Files (x86)/Steam/steamapps/common/dark forces/Game/",
		"C:/Program Files/Steam/steamapps/common/dark forces/Game/",
		"C:/Program Files (x86)/Steam/steamapps/common/Star Wars Dark Forces Remaster/",
		"C:/Program Files/Steam/steamapps/common/Star Wars Dark Forces Remaster/",
		"C:/Program Files (x86)/GOG.com/Star Wars - Dark Forces/",
		"C:/GOG Games/Star Wars - Dark Forces/",
		// D drive
		"D:/Program Files (x86)/Steam/steamapps/common/dark forces/Game/",
		"D:/Program Files/Steam/steamapps/common/dark forces/Game/",
		"D:/Program Files (x86)/Steam/steamapps/common/Star Wars Dark Forces Remaster/",
		"D:/Program Files/Steam/steamapps/common/Star Wars Dark Forces Remaster/",
		"D:/Program Files (x86)/GOG.com/Star Wars - Dark Forces/",
		"D:/GOG Games/Star Wars - Dark Forces/",
	};
	static const char* const c_outlawsLocations[] =
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
	static const u32 c_hardcodedPathCount[] = { TFE_ARRAYSIZE(c_darkForcesLocations), TFE_ARRAYSIZE(c_outlawsLocations) };

	static const char* const * c_gameLocations[] =
	{
		c_darkForcesLocations,
		c_outlawsLocations
	};
}