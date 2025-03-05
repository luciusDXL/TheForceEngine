#include "groups.h"
#include "levelEditorData.h"
#include "sharedState.h"
#include <TFE_System/math.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/snapshotReaderWriter.h>
#include <TFE_Ui/ui.h>

#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;
using namespace TFE_Jedi;

namespace LevelEditor
{
	typedef std::map<u32, u32> GroupIdToIndex;
	std::vector<Group> s_groups;
	s32 s_groupCurrent = 0;

	static GroupIdToIndex s_groupIdToIndex;
	static u32 s_uniqueId = 0;
	static char s_nameInput[256];
	void groups_buildIndexMap();

	void groups_clearName()
	{
		memset(s_nameInput, 0, 256);
	}

	bool groups_chooseName()
	{
		pushFont(TFE_Editor::FONT_SMALL);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

		bool create = false;
		bool cancel = false;
		if (ImGui::BeginPopupModal("Choose Name", nullptr, window_flags))
		{
			ImGui::Text("Choose Name");
			ImGui::SameLine(0.0f, 8.0f);
			ImGui::SetNextItemWidth(264.0f);
			ImGui::InputText(editor_getUniqueLabel(""), s_nameInput, 32);

			if (ImGui::Button("Create Group"))
			{
				create = true;
			}
			ImGui::SameLine(0.0f, 8.0f);
			if (ImGui::Button("Cancel"))
			{
				cancel = true;
			}
			ImGui::EndPopup();
		}
		popFont();

		if (create && s_nameInput[0])
		{
			groups_add(s_nameInput, s_groupCurrent);
		}
		return create || cancel;
	}
		
	void groups_init()
	{
		s_uniqueId = 0;
		s_groups.clear();
		groups_add("Main");
		groups_buildIndexMap();
	}
		
	void groups_loadFromSnapshot()
	{
		s_groupCurrent = readS32();
		s_uniqueId = readU32();
		u32 count = readU32();

		s_groups.resize(count);
		Group* group = s_groups.data();
		for (u32 i = 0; i < count; i++, group++)
		{
			group->id = readU32();
			readString(group->name);
			group->flags = readU32();
			readData(group->color.m, sizeof(Vec3f) * 3);
		}
		groups_buildIndexMap();
	}

	void groups_saveToSnapshot()
	{
		const u32 count = (u32)s_groups.size();
		writeS32(s_groupCurrent);
		writeU32(s_uniqueId);
		writeU32(count);

		const Group* group = s_groups.data();
		for (u32 i = 0; i < count; i++, group++)
		{
			writeU32(group->id);
			writeString(group->name);
			writeU32(group->flags);
			writeData(group->color.m, sizeof(Vec3f) * 3);
		}
	}

	void groups_loadBinary(FileStream& file, u32 version)
	{
		if (version < LEF_Groups) { return; }

		u32 count = 0;
		file.read(&s_groupCurrent);
		file.read(&s_uniqueId);
		file.read(&count);
		
		s_groups.resize(count);
		Group* group = s_groups.data();
		for (u32 i = 0; i < count; i++, group++)
		{
			file.read(&group->id);
			file.read(&group->name);
			file.read(&group->flags);
			file.read(group->color.m, 3);
		}
		groups_buildIndexMap();
	}

	void groups_saveBinary(FileStream& file)
	{
		const u32 count = (u32)s_groups.size();
		file.write(&s_groupCurrent);
		file.write(&s_uniqueId);
		file.write(&count);

		const Group* group = s_groups.data();
		for (u32 i = 0; i < count; i++, group++)
		{
			file.write(&group->id);
			file.write(&group->name);
			file.write(&group->flags);
			file.write(group->color.m, 3);
		}
	}

	bool groups_isIdValid(s32 id)
	{
		return groups_getById(id) != nullptr;
	}

	u32 groups_getMainId()
	{
		assert(!s_groups.empty());
		return s_groups[0].id;
	}

	u32 groups_getCurrentId()
	{
		assert(s_groupCurrent >= 0 && s_groupCurrent < s_groups.size());
		return s_groups[s_groupCurrent].id;
	}

	Group* groups_getByIndex(u32 index)
	{
		return index < (u32)s_groups.size() ? &s_groups[index] : nullptr;
	}
		
	Group* groups_getById(u32 id)
	{
		GroupIdToIndex::iterator iGroup = s_groupIdToIndex.find(id);
		if (iGroup != s_groupIdToIndex.end())
		{
			return &s_groups[iGroup->second];
		}
		return nullptr;
	}

	void groups_add(const char* name, s32 index)
	{
		s_uniqueId++;
		Group group = { s_uniqueId, 0, name, GRP_NONE, {1.0f, 1.0f, 1.0f} };
		if (s_groups.empty() || index < 0)
		{
			s_groupCurrent = (s32)s_groups.size();
			s_groups.push_back(group);
		}
		else
		{
			s_groupCurrent = index + 1;
			s_groups.insert(s_groups.begin() + s_groupCurrent, group);
		}
		groups_buildIndexMap();
	}

	void groups_select(s32 id)
	{
		Group* group = groups_getById(id);
		if (group)
		{
			s_groupCurrent = group->index;
		}
	}
	   
	void groups_swapSectorGroupID(u32 srcId, u32 dstId)
	{
		const s32 sectorCount = (s32)s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		for (s32 i = 0; i < sectorCount; i++, sector++)
		{
			if (sector->groupId == srcId)
			{
				sector->groupId = dstId;
				sector->groupIndex = 0;
			}
		}
	}

	void groups_remove(s32 index)
	{
		if (index < 0 || index >= (s32)s_groups.size()) { return; }
		u32 id = s_groups[index].id;
		// Move sectors with this id to the main group.
		groups_swapSectorGroupID(id, groups_getMainId());

		const s32 count = (s32)s_groups.size();
		for (s32 i = index; i < count - 1; i++)
		{
			s_groups[i] = s_groups[i + 1];
		}
		s_groups.pop_back();
		groups_buildIndexMap();
		s_groupCurrent = 0;
	}

	void groups_moveUp(s32 index)
	{
		//swap with the item above.
		std::swap(s_groups[index], s_groups[index - 1]);
		s_groupCurrent--;
		groups_buildIndexMap();
	}

	void groups_moveDown(s32 index)
	{
		//swap with the item below.
		std::swap(s_groups[index], s_groups[index + 1]);
		s_groupCurrent++;
		groups_buildIndexMap();
	}

	void groups_buildIndexMap()
	{
		s_groupIdToIndex.clear();
		const s32 count = (s32)s_groups.size();
		Group* group = s_groups.data();
		for (s32 i = 0; i < count; i++, group++)
		{
			s_groupIdToIndex[group->id] = i;
			group->index = u32(i);
		}
	}
}
