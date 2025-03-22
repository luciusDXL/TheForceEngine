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
#include <TFE_FileSystem/filestream.h>
#include <TFE_Settings/gameSourceData.h>
#include <string>

namespace TFE_Editor
{
	enum ResourceType
	{
		RES_ARCHIVE = 0,
		RES_DIRECTORY,
		RES_COUNT
	};
	struct EditorResource
	{
		ResourceType type;
		char name[TFE_MAX_PATH];
		union
		{
			Archive* archive;
			char path[TFE_MAX_PATH];
		};
	};
	
	bool resources_ui();
	void resources_clear();
	void resources_setGame(GameID gameId);
	bool resources_listChanged();
	void resources_dirty();
	bool resources_ignoreVanillaAssets();

	void resources_save(FileStream& outFile);
	void resources_createExternalEmpty();
	void resources_parse(const char* key, const char* value);

	EditorResource* resources_getExternal(u32& count);
	EditorResource* resources_getBaseGame(u32& count);
}
