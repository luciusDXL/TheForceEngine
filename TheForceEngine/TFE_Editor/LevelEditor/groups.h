#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/filestream.h>
#include <vector>
#include <string>

namespace LevelEditor
{
	struct EditorSector;

	enum GroupFlags
	{
		GRP_NONE = 0,
		GRP_HIDDEN = FLAG_BIT(0),
		GRP_LOCKED = FLAG_BIT(1),
		GRP_EXCLUDE = FLAG_BIT(2),
	};

	struct Group
	{
		u32 id = 0;
		u32 index = 0;
		std::string name;
		u32 flags = GRP_NONE;
		Vec3f color = { 1.0f, 1.0f, 1.0f };
	};

	void groups_init();
	void groups_loadBinary(FileStream& file, u32 version);
	void groups_saveBinary(FileStream& file);
	void groups_swapSectorGroupID(u32 srcId, u32 dstId);

	void groups_loadFromSnapshot();
	void groups_saveToSnapshot();

	void groups_add(const char* name, s32 index = -1);
	void groups_remove(s32 index);
	void groups_moveUp(s32 index);
	void groups_moveDown(s32 index);
	void groups_select(s32 id);

	void groups_clearName();
	bool groups_chooseName();
		
	bool groups_isIdValid(s32 id);
	u32 groups_getMainId();
	u32 groups_getCurrentId();
	Group* groups_getById(u32 id);
	Group* groups_getByIndex(u32 index);
	
	extern std::vector<Group> s_groups;
	extern s32 s_groupCurrent;
	extern s32 s_groupInsertIndex;
}
