#include "editorLevelPreview.h"
#include "editorColormap.h"
#include <TFE_Editor/editor.h>
#include <TFE_DarkForces/mission.h>
#include <TFE_System/system.h>
#include <TFE_Archive/archive.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Renderer/rcommon.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <map>

namespace TFE_Editor
{
	typedef std::vector<EditorLevelPreview> LevelPreviewList;
	static LevelPreviewList s_levelPreviewList;
	
	void freeLevelPreview(const char* name)
	{
		const size_t count = s_levelPreviewList.size();
		EditorLevelPreview* lev = s_levelPreviewList.data();
		for (size_t i = 0; i < count; i++, lev++)
		{
			if (strcasecmp(lev->name, name) == 0)
			{
				TFE_RenderBackend::freeTexture(lev->thumbnail);
				*lev = EditorLevelPreview{};
				break;
			}
		}
	}

	void freeCachedLevelPreview()
	{
		const size_t count = s_levelPreviewList.size();
		const EditorLevelPreview* lev = s_levelPreviewList.data();
		for (size_t i = 0; i < count; i++, lev++)
		{
			TFE_RenderBackend::freeTexture(lev->thumbnail);
		}
		s_levelPreviewList.clear();
	}

	s32 allocateLevelPreview(const char* name)
	{
		s32 index = (s32)s_levelPreviewList.size();
		s_levelPreviewList.emplace_back();
		return index;
	}

	s32 loadEditorLevelPreview(LevSourceType type, Archive* archive, const char* filename, s32 id)
	{
		if (!archive && !filename) { return -1; }

		if (archive)
		{
			if (!archive->openFile(filename))
			{
				return -1;
			}
			size_t len = archive->getFileLength();
			if (!len)
			{
				archive->closeFile();
				return -1;
			}
			WorkBuffer& buffer = getWorkBuffer();
			buffer.resize(len);
			archive->readFile(buffer.data(), len);
			archive->closeFile();
		}
		// TODO: Handle non-archive...
		
		if (type == LEV_LEV)
		{
			return -1;
		}
		return id;
	}

	EditorLevelPreview* getLevelPreviewData(u32 index)
	{
		if (index >= s_levelPreviewList.size()) { return nullptr; }
		return &s_levelPreviewList[index];
	}
}