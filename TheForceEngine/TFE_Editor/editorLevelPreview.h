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
	struct EditorLevelPreview
	{
		TextureGpu* thumbnail = nullptr;
		char name[64] = "";
	};
	enum LevSourceType
	{
		LEV_LEV,
		LEV_COUNT
	};
		
	void freeCachedLevelPreview();
	void freeLevelPreview(const char* name);

	EditorLevelPreview* getLevelPreviewData(u32 index);
	s32 loadEditorLevelPreview(LevSourceType type, Archive* archive, const char* filename, s32 id = -1);
}
