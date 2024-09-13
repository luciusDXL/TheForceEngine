#pragma once
#include <TFE_System/types.h>
#include <TFE_Editor/history.h>
#include <TFE_Editor/LevelEditor/selection.h>
#include <TFE_Editor/LevelEditor/levelEditorData.h>

namespace LevelEditor
{
	enum LevCommandName
	{
		LName_MoveVertex = 0,
		LName_SetVertex,
		LName_MoveWall,
		LName_MoveFlat,
		LName_MoveSector,
		LName_MoveObject,
		LName_MoveObjectToFloor,
		LName_MoveObjectToCeil,
		LName_InsertVertex,
		LName_DeleteVertex,
		LName_DeleteWall,
		LName_DeleteSector,
		LName_CreateSectorFromRect,
		LName_CreateSectorFromShape,
		LName_ExtrudeSectorFromWall,
		LName_MoveTexture,
		LName_SetTexture,
		LName_CopyTexture,
		LName_ClearTexture,
		LName_Autoalign,
		LName_DeleteObject,
		LName_AddObject,
		LName_RotateSector,
		LName_RotateWall,
		LName_RotateVertex,
		LName_ChangeWallAttrib,
		LName_ChangeSectorAttrib,
		LName_Count
	};

	void levHistory_init();
	void levHistory_destroy();

	void levHistory_clear();
	void levHistory_createSnapshot(const char* name);

	void levHistory_undo();
	void levHistory_redo();

	// Commands
	void cmd_sectorSnapshot(u32 name, std::vector<s32>& sectorWallIds);
	void cmd_sectorWallSnapshot(u32 name, std::vector<IndexPair>& sectorIds, bool idsChanged);
	void cmd_sectorAttributeSnapshot(u32 name, std::vector<IndexPair>& sectorIds, bool idsChanged);
	void cmd_objectListSnapshot(u32 name, s32 sectorId);
}
