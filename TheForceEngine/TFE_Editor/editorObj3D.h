#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Archive/archive.h>
#include <TFE_RenderBackend/renderBackend.h>

namespace TFE_Editor
{
	struct EditorObj3D
	{
		TextureGpu* thumbnail = nullptr;
		char name[64] = "";
	};
	enum Obj3DSourceType
	{
		OBJ3D_3DO,
		OBJ3D_COUNT
	};
		
	void freeCachedObj3D();
	void freeObj3D(const char* name);

	EditorObj3D* getObj3DData(u32 index);
	s32 loadEditorObj3D(Obj3DSourceType type, Archive* archive, const char* filename, const u32* palette, s32 palIndex, s32 id = -1);
}
