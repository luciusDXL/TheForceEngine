#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Game/igame.h>
#include <string>

namespace TFE_Editor
{
	enum ProjectType
	{
		PROJ_RESOURCE_ONLY = 0,
		PROJ_LEVELS,
		PROJ_COUNT
	};

	enum FeatureSet
	{
		FSET_VANILLA = 0,
		FSET_TFE,
		FSET_COUNT
	};

	struct Project
	{
		std::string name;
		std::string path;
		std::string desc;
		std::string authors;

		ProjectType type;
		GameID game;
		FeatureSet featureSet;
	};

	Project* getProject();

	bool ui_loadProject();
	void ui_closeProject();
	void ui_newProject();
}
