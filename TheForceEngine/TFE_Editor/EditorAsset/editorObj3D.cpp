#include "editorObj3D.h"
#include "editorColormap.h"
#include <TFE_Editor/editor.h>
#include <TFE_DarkForces/mission.h>
#include <TFE_System/system.h>
#include <TFE_Archive/archive.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Renderer/rcommon.h>
#include <algorithm>
#include <vector>
#include <map>

namespace TFE_Editor
{
	typedef std::vector<EditorObj3D> Obj3DList;
	static Obj3DList s_obj3DList;
	
	void freeObj3D(const char* name)
	{
		const size_t count = s_obj3DList.size();
		EditorObj3D* obj3D = s_obj3DList.data();
		for (size_t i = 0; i < count; i++, obj3D++)
		{
			if (strcasecmp(obj3D->name, name) == 0)
			{
				TFE_RenderBackend::freeTexture(obj3D->thumbnail);
				*obj3D = {};
				break;
			}
		}
	}

	void freeCachedObj3D()
	{
		const size_t count = s_obj3DList.size();
		const EditorObj3D* obj3D = s_obj3DList.data();
		for (size_t i = 0; i < count; i++, obj3D++)
		{
			TFE_RenderBackend::freeTexture(obj3D->thumbnail);
		}
		s_obj3DList.clear();
	}

	s32 allocateObj3D(const char* name)
	{
		s32 index = (s32)s_obj3DList.size();
		s_obj3DList.push_back({});
		return index;
	}

	s32 loadEditorObj3D(Obj3DSourceType type, Archive* archive, const char* filename, const u32* palette, s32 palIndex, s32 id)
	{
		if (!archive && !filename) { return -1; }

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

		if (type == OBJ3D_3DO)
		{
			return -1;
		}
		return id;
	}

	EditorObj3D* getObj3DData(u32 index)
	{
		if (index >= s_obj3DList.size()) { return nullptr; }
		return &s_obj3DList[index];
	}
}