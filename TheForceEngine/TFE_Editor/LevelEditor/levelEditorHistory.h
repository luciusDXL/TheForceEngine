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
		LName_InsertVertex,
		LName_DeleteVertex,
		LName_DeleteWall,
		LName_DeleteSector,
		LName_CreateSectorFromRect,
		LName_CreateSectorFromShape,
		LName_MoveTexture,
		LName_SetTexture,
		LName_CopyTexture,
		LName_ClearTexture,
		LName_Autoalign,
		LName_Count
	};

	void levHistory_init();
	void levHistory_destroy();

	void levHistory_clear();
	void levHistory_createSnapshot(const char* name);

	void levHistory_undo();
	void levHistory_redo();

	// Commands
	enum SectorAttribId
	{
		SEC_ATTRIB_GROUP_ID = 0,
		SEC_ATTRIB_NAME,
		SEC_ATTRIB_FLOOR_TEX,
		SEC_ATTRIB_CEIL_TEX,
		SEC_ATTRIB_FLOOR_HEIGHT,
		SEC_ATTRIB_CEIL_HEIGHT,
		SEC_ATTRIB_SEC_HEIGHT,
		SEC_ATTRIB_FLAGS1,
		SEC_ATTRIB_FLAGS2,
		SEC_ATTRIB_FLAGS3,
		SEC_ATTRIB_LAYER,
		SEC_ATTRIB_COUNT
	};

	void cmd_sectorChangeAttribute(u32 name, s32 count, const FeatureId* list, u32 attribId);
	void cmd_sectorSetAttribute(u32 name, s32 count, const FeatureId* list, u32 attribId);
}
