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
#include <TFE_Editor/editorResources.h>
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

	enum ProjectFlags
	{
		PFLAG_NONE = 0,
		// Require True color mode, true color assets are *not* converted to 8-bit on import.
		PFLAG_TRUE_COLOR = FLAG_BIT(0),
	};

	struct Project
	{
		std::string name;
		std::string path;
		std::string desc;
		std::string authors;
		std::string credits;

		bool active = false;
		ProjectType type = PROJ_LEVELS;
		GameID game = Game_Dark_Forces;
		FeatureSet featureSet = FSET_VANILLA;

		u32 flags = PFLAG_NONE;	// See ProjectFlags
	};

	Project* project_get();
	
	void project_save();
	void project_close();
	bool project_load(const char* filepath);
	bool project_editUi(bool newProject);
}
