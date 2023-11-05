#pragma once
#include <TFE_System/types.h>
#include <TFE_Editor/history.h>
#include <TFE_Editor/LevelEditor/selection.h>

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
		LName_Count
	};

	void levHistory_init();
	void levHistory_destroy();

	void levHistory_clear();
	void levHistory_createSnapshot(const char* name);

	void levHistory_undo();
	void levHistory_redo();

	void captureEditState();
	void restoreEditState();

	// Commands
	void cmd_addMoveVertices(s32 count, const FeatureId* vertices, Vec2f delta, LevCommandName name = LName_MoveVertex);
	void cmd_addSetVertex(FeatureId vertex, Vec2f pos);
	void cmd_addMoveFlats(s32 count, const FeatureId* flats, f32 delta);
	void cmd_addInsertVertex(s32 sectorIndex, s32 wallIndex, Vec2f newPos);
	void cmd_addDeleteVertex(s32 sectorIndex, s32 vertexIndex);
}
