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

namespace LevelEditor
{
	extern EditorLevel s_level;
	extern TFE_Editor::AssetList s_levelTextureList;
	extern LevelEditMode s_editMode;
	extern u32 s_editFlags;
	extern u32 s_lwinOpen;
	extern s32 s_curLayer;

	// Sector
	extern EditorSector* s_hoveredSector;
	extern EditorSector* s_selectedSector;

	// Vertex
	extern EditorSector* s_hoveredVtxSector;
	extern EditorSector* s_selectedVtxSector;
	extern s32 s_hoveredVtxId;
	extern s32 s_selectedVtxId;

	// Wall
	extern EditorSector* s_hoveredWallSector;
	extern EditorSector* s_selectedWallSector;
	extern s32 s_hoveredWallId;
	extern s32 s_selectedWallId;

	// Camera
	extern Camera3d s_camera;
	extern Vec3f s_viewDir;
	extern Vec3f s_viewRight;
	extern Vec3f s_cursor3d;
}
