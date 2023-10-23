#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Editor/EditorAsset/editorAsset.h>
#include <TFE_RenderShared/lineDraw3d.h>
#include "levelEditorData.h"
#include <vector>

namespace LevelEditor
{
	struct Feature
	{
		EditorSector* sector = nullptr;
		EditorSector* prevSector = nullptr;
		s32 featureIndex = -1;
		HitPart part = HP_NONE;
	};
		
	extern EditorLevel s_level;
	extern TFE_Editor::AssetList s_levelTextureList;
	extern LevelEditMode s_editMode;
	extern u32 s_editFlags;
	extern u32 s_lwinOpen;
	extern s32 s_curLayer;

	extern Feature s_featureHovered;
	extern Feature s_featureCur;

	extern Vec3f s_hoveredVtxPos;
	extern Vec3f s_curVtxPos;

	// Camera
	extern Camera3d s_camera;
	extern Vec3f s_viewDir;
	extern Vec3f s_viewRight;
	extern Vec3f s_cursor3d;

	// Search
	extern u32 s_searchKey;
}
